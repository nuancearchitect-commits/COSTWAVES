#include "CostWavesPrecompiledHeader.hpp"

#include "CostWavesDialog.hpp"

#include "ArticleManager.hpp"
#include "Exporter.hpp"
#include "ModelReader.hpp"

#include "UniStringWStringConversion.hpp"

#include <algorithm>
#include <cwchar>

namespace CostWaves {

namespace {

GS::UniString FR (const char* utf8Text)
{
	return GS::UniString (utf8Text, CC_UTF8);
}

GS::UniString US (const char* utf8Text)
{
	return GS::UniString (utf8Text, CC_UTF8);
}

// "1234,56" — affichage français.
GS::UniString FormatValue (double value)
{
	wchar_t buffer[64];
	swprintf (buffer, 64, L"%.2f", value);

	std::wstring text (buffer);
	const std::size_t dot = text.find (L'.');
	if (dot != std::wstring::npos)
		text[dot] = L',';

	return GS::ToUniString (text);
}

// Résumé compact des quantités (colonne "Quantités").
GS::UniString QuantitiesSummary (const GS::Array<CWQuantity>& quantities, USize maxItems)
{
	if (quantities.IsEmpty ())
		return FR ("—");

	GS::UniString summary;
	USize count = 0;
	for (UIndex i = 0; i < quantities.GetSize () && count < maxItems; ++i) {
		if (count > 0)
			summary += US (" · ");
		summary += quantities[i].label + " " + FormatValue (quantities[i].value) + " " + quantities[i].unit;
		++count;
	}
	if (quantities.GetSize () > maxItems)
		summary += FR (" · …");
	return summary;
}

// La ligne peut-elle encore recevoir ce fragment (~110 caractères) ?
bool LineFits (const GS::UniString& line, const GS::UniString& chunk)
{
	return line.GetLength () + chunk.GetLength () < 110u;
}

// Première quantité (clé de tri numérique), 0 si aucune.
double FirstQuantityValue (const CWElementRow& element)
{
	return element.quantities.IsEmpty () ? 0.0 : element.quantities[0].value;
}

// a < b selon la colonne de tri (1..6) ?
bool RowLess (const CWElementRow& a, const CWElementRow& b, short column)
{
	switch (column) {
		case 1:
			return a.typeName.ToUpperCase () < b.typeName.ToUpperCase ();
		case 2:
			return APIGuidToString (a.guid) < APIGuidToString (b.guid);
		case 3:
			return a.elementId.ToUpperCase () < b.elementId.ToUpperCase ();
		case 4:
			if (a.floorInd != b.floorInd)
				return a.floorInd < b.floorInd;
			return a.storyName.ToUpperCase () < b.storyName.ToUpperCase ();
		case 5:
			return (a.classItemId + US (" ") + a.classItemName).ToUpperCase ()
				 < (b.classItemId + US (" ") + b.classItemName).ToUpperCase ();
		case 6:
			return FirstQuantityValue (a) < FirstQuantityValue (b);
		default:
			return false;
	}
}

} // namespace


CostWavesDialog::CostWavesDialog ()
	:	DG::ModalDialog (ACAPI_GetOwnResModule (), DialogResourceId, ACAPI_GetOwnResModule ()),
		systemPopup (GetReference (), SystemPopupId),
		refreshButton (GetReference (), RefreshButtonId),
		selectionCheck (GetReference (), SelectionCheckId),
		statusText (GetReference (), StatusTextId),
		table (GetReference (), TableId),
		detailsGroup (GetReference (), DetailsGroupId),
		detail1 (GetReference (), DetailText1Id),
		detail2 (GetReference (), DetailText2Id),
		detail3 (GetReference (), DetailText3Id),
		detail4 (GetReference (), DetailText4Id),
		detail5 (GetReference (), DetailText5Id),
		articlePopup (GetReference (), ArticlePopupId),
		assignButton (GetReference (), AssignButtonId),
		importButton (GetReference (), ImportButtonId),
		createClassButton (GetReference (), CreateClassButtonId),
		createMaterialsButton (GetReference (), CreateMaterialsButtonId),
		articlesInfo (GetReference (), ArticlesInfoId),
		searchLabel (GetReference (), SearchLabelId),
		searchEdit (GetReference (), SearchEditId),
		exportJsonButton (GetReference (), ExportJsonButtonId),
		exportCsvButton (GetReference (), ExportCsvButtonId),
		closeButton (GetReference (), CloseButtonId)
{
	SetTitle (GS::UniString (ADDON_NAME) + " " + ADDON_VERSION);

	Attach (*this);					// PanelObserver
	systemPopup.Attach (*this);		// PopUpObserver
	refreshButton.Attach (*this);	// ButtonItemObserver
	table.Attach (*this);			// ListBoxObserver
	assignButton.Attach (*this);	// ButtonItemObserver
	importButton.Attach (*this);	// ButtonItemObserver
	createClassButton.Attach (*this);// ButtonItemObserver
	createMaterialsButton.Attach (*this);	// ButtonItemObserver
	searchEdit.Attach (*this);		// SearchEditObserver
	exportJsonButton.Attach (*this);
	exportCsvButton.Attach (*this);
	closeButton.Attach (*this);

	InitTable ();

	ModelReader::ResolveElementIdPropertyGuid (elemIdPropGuid, elemIdPropNote);

	LoadSystems ();
	ReloadArticlesFromSystem ();

	if (selectedSystem != APINULLGuid)
		RefreshData ();
	else
		ClearDetails ();
}


void CostWavesDialog::InitTable ()
{
	const short columnCount = 6;

	// En-têtes de colonnes (indices DG : 1-based).
	table.SetHeaderItemCount (columnCount);
	table.SetHeaderItemText (1, FR ("Type"));
	table.SetHeaderItemText (2, FR ("GUID"));
	table.SetHeaderItemText (3, FR ("ID élément"));
	table.SetHeaderItemText (4, FR ("Étage"));
	table.SetHeaderItemText (5, FR ("Classe"));
	table.SetHeaderItemText (6, FR ("Quantités disponibles"));

	// Colonnes : positions calculées sur la largeur du contrôle.
	const short tableWidth = table.GetWidth ();
	const short proportions[columnCount] = { 9, 24, 12, 14, 17, 24 };

	const short totalProportion = 100;
	short position = 0;
	for (short i = 1; i <= columnCount; ++i) {
		const short width = static_cast<short> ((tableWidth * proportions[i - 1]) / totalProportion);
		table.SetHeaderItemSize (i, width);
		table.SetHeaderItemSizeableFlag (i, true);
		table.SetTabFieldProperties (i, position, position + width,
									 DG::ListBox::Left, DG::ListBox::MiddleTruncate, i > 1);
		position = static_cast<short> (position + width);
	}
}


void CostWavesDialog::LoadSystems ()
{
	isFilling = true;

	systems = ModelReader::GetClassificationSystems ();

	for (UIndex i = 0; i < systems.GetSize (); ++i) {
		systemPopup.AppendItem ();
		systemPopup.SetItemText (static_cast<short> (i + 1), systems[i].name);
	}

	if (!systems.IsEmpty ()) {
		systemPopup.SelectItem (1);
		selectedSystem = systems[0].guid;
	}

	isFilling = false;
}


void CostWavesDialog::RefreshData ()
{
	if (selectedSystem == APINULLGuid) {
		SetDetailLine (1, FR ("Aucun système de classification trouvé dans le projet."));
		SetDetailLine (2, FR ("Créez / importez un système dans Archicad (Option > Classifications…) puis Actualiser."));
		SetDetailLine (3, GS::UniString ());
		SetDetailLine (4, GS::UniString ());
		SetDetailLine (5, GS::UniString ());
		return;
	}

	// Lecture de la sélection courante si la case est cochée.
	GS::Array<API_Guid> selection;
	const GS::Array<API_Guid>* filter = nullptr;
	if (selectionCheck.IsChecked ()) {
		const GSErrCode selErr = ModelReader::GetSelectedElements (selection);
		if (selErr != NoError) {
			statusText.SetText (FR ("Impossible de lire la sélection courante (code ")
								 + GS::ToUniString (std::to_wstring (static_cast<int> (selErr)))
								 + FR (") — décochez « Sélection uniquement »."));
			ClearDetails ();
			return;
		}
		if (selection.IsEmpty ()) {
			isFilling = true;
			rows.Clear ();
			report = CWScanReport ();
			FillTable ();
			isFilling = false;
			statusText.SetText (FR ("Aucun élément sélectionné dans Archicad — sélectionnez puis Actualiser."));
			ClearDetails ();
			return;
		}
		filter = &selection;
	}

	isFilling = true;

	rows.Clear ();
	ModelReader::Scan (selectedSystem, elemIdPropGuid, filter, rows, report);
	FillTable ();

	isFilling = false;

	UpdateStatus ();
	UpdateDetails (1);
}


bool CostWavesDialog::ElementMatchesFilter (const CWElementRow& element) const
{
	if (searchFilter.IsEmpty ())
		return true;

	const GS::UniString needle = searchFilter.ToUpperCase ();

	const GS::UniString haystacks[] = {
		element.typeName,
		APIGuidToString (element.guid),
		element.elementId,
		element.storyName,
		element.classItemId,
		element.classItemName
	};

	for (UIndex i = 0; i < sizeof (haystacks) / sizeof (haystacks[0]); ++i) {
		if (haystacks[i].ToUpperCase ().Contains (needle))
			return true;
	}

	return false;
}


void CostWavesDialog::SortRows ()
{
	if (sortColumn < 1 || sortColumn > 6 || rows.GetSize () < 2)
		return;

	// Trier des indices puis réassembler (les composants restent avec leur
	// élément : on trie "rows" entières).
	std::vector<UIndex> order;
	order.reserve (rows.GetSize ());
	for (UIndex i = 0; i < rows.GetSize (); ++i)
		order.push_back (i);

	const short column = sortColumn;
	const bool ascending = sortAscending;
	std::stable_sort (order.begin (), order.end (),
					  [&] (UIndex a, UIndex b) {
						  return ascending ? RowLess (rows[a], rows[b], column)
										   : RowLess (rows[b], rows[a], column);
					  });

	GS::Array<CWElementRow> sorted;
	for (UIndex k = 0; k < order.size (); ++k)
		sorted.Push (rows[order[k]]);
	rows = sorted;
}


void CostWavesDialog::FillTable ()
{
	table.SetHeaderSynchronState (true);

	// Tri courant avant affichage.
	SortRows ();

	while (table.GetItemCount () > 0)
		table.DeleteItem (1);

	displayRows.Clear ();

	for (UIndex e = 0; e < rows.GetSize (); ++e) {
		const CWElementRow& element = rows[e];

		// Filtre de recherche : l'élément et ses composants sont masqués
		// si aucune de ses colonnes ne correspond.
		if (!ElementMatchesFilter (element))
			continue;

		const GS::UniString floorText = element.storyName.IsEmpty ()
			? GS::ToUniString (std::to_wstring (static_cast<int> (element.floorInd)))
			: GS::ToUniString (std::to_wstring (static_cast<int> (element.floorInd))) + " - " + element.storyName;

		const GS::UniString classText = element.classItemId.IsEmpty ()
			? element.classItemName
			: element.classItemId + " - " + element.classItemName;

		table.AppendItem ();
		const short itemIndex = table.GetItemCount ();
		table.SetTabItemText (itemIndex, 1, FR ("Élément"));
		table.SetTabItemText (itemIndex, 2, APIGuidToString (element.guid));
		table.SetTabItemText (itemIndex, 3, element.elementId);
		table.SetTabItemText (itemIndex, 4, floorText);
		table.SetTabItemText (itemIndex, 5, classText);
		table.SetTabItemText (itemIndex, 6, QuantitiesSummary (element.quantities, 3));

		DisplayRow elementRow;
		elementRow.kind = RowKind::Element;
		elementRow.elementIndex = e;
		displayRows.Push (elementRow);

		for (UIndex c = 0; c < element.components.GetSize (); ++c) {
			const CWComponentRow& component = element.components[c];

			table.AppendItem ();
			const short compItemIndex = table.GetItemCount ();
			if (component.kind == RowKind::Skin) {
				GS::UniString skinText = FR ("Skin — ") + component.label;
				if (component.coreSkin)
					skinText += FR (" (cœur)");
				table.SetTabItemText (compItemIndex, 1, skinText);
			} else {
				table.SetTabItemText (compItemIndex, 1, FR ("Composant"));
			}
			table.SetTabItemText (compItemIndex, 2,
				component.guid == APINULLGuid ? GS::UniString () : APIGuidToString (component.guid));
			table.SetTabItemText (compItemIndex, 3, element.elementId);
			table.SetTabItemText (compItemIndex, 4, floorText);
			table.SetTabItemText (compItemIndex, 5, classText);
			table.SetTabItemText (compItemIndex, 6,
				QuantitiesSummary (component.quantities, 3));

			DisplayRow componentRow;
			componentRow.kind = component.kind;
			componentRow.elementIndex = e;
			componentRow.componentIndex = c;
			displayRows.Push (componentRow);
		}
	}

	if (table.GetItemCount () > 0)
		table.SelectItem (1);

	// Flèche de tri sur la colonne active.
	for (short c = 1; c <= 6; ++c)
		table.SetHeaderItemArrowType (c, DG::ListBox::NoArrow);
	if (sortColumn >= 1 && sortColumn <= 6)
		table.SetHeaderItemArrowType (sortColumn, sortAscending ? DG::ListBox::Up : DG::ListBox::Down);
}


void CostWavesDialog::UpdateStatus ()
{
	GS::UniString status = GS::ToUniString (std::to_wstring (static_cast<int> (report.classifiedElements)))
		+ FR (" éléments classés · ")
		+ GS::ToUniString (std::to_wstring (static_cast<int> (report.componentCount)))
		+ FR (" composants · ")
		+ GS::ToUniString (std::to_wstring (static_cast<int> (report.skinCount)))
		+ FR (" skins · ")
		+ GS::ToUniString (std::to_wstring (static_cast<int> (report.scannedElements)))
		+ FR (" éléments analysés");

	if (report.quantityErrors > 0) {
		status += FR (" · ⚠ ")
			+ GS::ToUniString (std::to_wstring (static_cast<int> (report.quantityErrors)))
			+ FR (" erreurs de quantités");
	}

	statusText.SetText (status);
}


void CostWavesDialog::SetDetailLine (short lineIndex, const GS::UniString& text)
{
	switch (lineIndex) {
		case 1: detail1.SetText (text); break;
		case 2: detail2.SetText (text); break;
		case 3: detail3.SetText (text); break;
		case 4: detail4.SetText (text); break;
		case 5: detail5.SetText (text); break;
		default: break;
	}
}


void CostWavesDialog::ClearDetails ()
{
	for (short i = 1; i <= 5; ++i)
		SetDetailLine (i, GS::UniString ());
}


// ---------------------------------------------------------------------------
// Articles CostWaves (phase 2)
// ---------------------------------------------------------------------------

void CostWavesDialog::ReloadArticlesFromSystem ()
{
	// Les articles importés (JSON) ont priorité sur les items du système.
	if (articlesImported) {
		LoadArticlesPopup ();
		return;
	}

	articles.Clear ();
	articlesSourceName.Clear ();

	if (selectedSystem != APINULLGuid) {
		ArticleManager::CollectFromClassification (selectedSystem, articles);
		for (UIndex i = 0; i < systems.GetSize (); ++i) {
			if (systems[i].guid == selectedSystem) {
				articlesSourceName = systems[i].name;
				break;
			}
		}
	}

	LoadArticlesPopup ();
}


void CostWavesDialog::LoadArticlesPopup ()
{
	isFilling = true;

	while (articlePopup.GetItemCount () > 0)
		articlePopup.DeleteItem (1);

	for (UIndex i = 0; i < articles.GetSize (); ++i) {
		GS::UniString label = articles[i].id + US (" — ") + articles[i].name;
		if (!articles[i].unit.IsEmpty ())
			label += US (" (") + articles[i].unit + US (")");
		articlePopup.AppendItem ();
		articlePopup.SetItemText (articlePopup.GetItemCount (), label);
	}

	if (articlePopup.GetItemCount () > 0)
		articlePopup.SelectItem (1);

	GS::UniString info;
	if (articles.IsEmpty ()) {
		info = FR ("Aucun article — importez un JSON ou utilisez la classification.");
	} else {
		info = GS::ToUniString (std::to_wstring (static_cast<int> (articles.GetSize ())))
			 + (articlesImported ? FR (" articles importés — ") : FR (" articles (classification) — "))
			 + articlesSourceName;
	}
	articlesInfo.SetText (info);

	isFilling = false;
}


void CostWavesDialog::ImportArticles ()
{
	DG::FileDialog fileDialog (DG::FileDialog::OpenFile);
	fileDialog.SetTitle (FR ("Importer des articles CostWaves (JSON)"));

	// Dossier par défaut : celui du projet, sinon Documents.
	GS::UniString folder;
	GS::UniString projectName;
	if (Exporter::ResolveProjectLocation (folder, projectName)) {
		const IO::Location defaultFolder (folder);
		fileDialog.SetFolder (defaultFolder);
	}

	if (!fileDialog.Invoke ())
		return;		// annulé par l'utilisateur

	GS::UniString path;
	if (fileDialog.GetSelectedFile ().ToPath (&path) != NoError || path.IsEmpty ()) {
		DG::WarningAlert (FR ("Impossible de récupérer le fichier choisi."), GS::UniString (), FR ("OK"));
		return;
	}

	GS::Array<CWArticle> imported;
	GS::UniString error;
	if (!ArticleManager::ImportFromJsonFile (path, imported, error)) {
		DG::ErrorAlert (FR ("Échec de l'import des articles."), error, FR ("OK"));
		return;
	}

	articles = imported;
	articlesImported = true;

	// Nom du fichier (sans chemin) pour la ligne d'information.
	std::wstring pathW = GS::ToWString (path);
	for (wchar_t& ch : pathW) {
		if (ch == L'\\')
			ch = L'/';
	}
	const std::size_t slash = pathW.find_last_of (L'/');
	articlesSourceName = GS::ToUniString (slash != std::wstring::npos ? pathW.substr (slash + 1) : pathW);

	LoadArticlesPopup ();

	DG::InformationAlert (FR ("Articles importés."),
						  GS::ToUniString (std::to_wstring (static_cast<int> (articles.GetSize ())))
							  + FR (" articles lus depuis ") + articlesSourceName,
						  FR ("OK"));
}


void CostWavesDialog::CreateClassification ()
{
	if (articles.IsEmpty ()) {
		DG::WarningAlert (FR ("Aucun article disponible."),
						  FR ("Importez des articles (JSON) ou choisissez un système contenant des items."),
						  FR ("OK"));
		return;
	}

	API_Guid	systemGuid = APINULLGuid;
	USize		createdItems = 0;
	GS::UniString error;
	if (ArticleManager::EnsureCostWavesClassification (articles, systemGuid, createdItems, error) != NoError) {
		DG::ErrorAlert (FR ("Échec de la création de la classification."), error, FR ("OK"));
		return;
	}

	// Recharger les systèmes et sélectionner « CostWaves » (sans déclencher
	// la relecture : on l'appelle nous-mêmes juste après).
	LoadSystems ();

	isFilling = true;
	const GS::UniString costWavesName (ArticleManager::CostWavesSystemName (), CC_UTF8);
	for (UIndex i = 0; i < systems.GetSize (); ++i) {
		if (systems[i].name == costWavesName) {
			selectedSystem = systems[i].guid;
			systemPopup.SelectItem (static_cast<short> (i + 1));
			break;
		}
	}
	isFilling = false;

	// Si les articles venaient de la classification, relire depuis le système
	// « CostWaves » fraîchement créé.
	ReloadArticlesFromSystem ();
	RefreshData ();

	DG::InformationAlert (FR ("Classification CostWaves prête."),
						  createdItems > 0
							  ? GS::ToUniString (std::to_wstring (static_cast<int> (createdItems)))
									+ FR (" items créés.")
							  : FR ("Tous les articles existaient déjà."),
						  FR ("OK"));
}


void CostWavesDialog::AssignCurrentArticle ()
{
	// 1) Article choisi dans le popup.
	const short articleIndex = articlePopup.GetSelectedItem ();
	if (articleIndex < 1 || static_cast<UIndex> (articleIndex) > articles.GetSize ()) {
		DG::WarningAlert (FR ("Aucun article sélectionné."),
						  FR ("Importez des articles ou choisissez-en un dans la liste."),
						  FR ("OK"));
		return;
	}
	const CWArticle& article = articles[static_cast<UIndex> (articleIndex) - 1];

	// 2) Lignes sélectionnées : on garde les lignes d'éléments (les lignes
	//    composants sont ignorées — l'affectation se fait sur l'élément).
	GS::Array<API_Guid>	elemGuids;
	USize				skippedComponents = 0;

	const GS::Array<short> selectedItems = table.GetSelectedItems ();
	for (UIndex i = 0; i < selectedItems.GetSize (); ++i) {
		const short listItem = selectedItems[i];
		if (listItem < 1 || static_cast<UIndex> (listItem) > displayRows.GetSize ())
			continue;

		const DisplayRow& displayRow = displayRows[static_cast<UIndex> (listItem) - 1];
		if (displayRow.elementIndex >= rows.GetSize ())
			continue;

		if (displayRow.kind != RowKind::Element) {
			++skippedComponents;
			continue;
		}

		elemGuids.Push (rows[displayRow.elementIndex].guid);
	}

	if (elemGuids.IsEmpty ()) {
		if (skippedComponents > 0) {
			DG::WarningAlert (FR ("L'affectation se fait sur des éléments."),
							  GS::ToUniString (std::to_wstring (static_cast<int> (skippedComponents)))
								  + FR (" ligne(s) composant ignorée(s) — sélectionnez des lignes d'élément."),
							  FR ("OK"));
		} else {
			DG::WarningAlert (FR ("Aucune ligne sélectionnée."),
							  FR ("Sélectionnez un ou plusieurs éléments dans le tableau (Ctrl+clic)."),
							  FR ("OK"));
		}
		return;
	}

	// 3) Item de classification correspondant : système « CostWaves » en
	//    priorité, sinon le système courant.
	API_Guid targetSystem = APINULLGuid;
	API_Guid itemGuid = APINULLGuid;

	const API_Guid costWavesSystem = ArticleManager::FindCostWavesSystemGuid ();
	if (costWavesSystem != APINULLGuid) {
		const API_Guid guid = ArticleManager::FindItemGuid (costWavesSystem, article.id);
		if (guid != APINULLGuid) {
			targetSystem = costWavesSystem;
			itemGuid = guid;
		}
	}
	if (itemGuid == APINULLGuid && selectedSystem != APINULLGuid) {
		const API_Guid guid = ArticleManager::FindItemGuid (selectedSystem, article.id);
		if (guid != APINULLGuid) {
			targetSystem = selectedSystem;
			itemGuid = guid;
		}
	}

	if (itemGuid == APINULLGuid) {
		DG::WarningAlert (FR ("Article introuvable dans les classifications."),
						  FR ("Cliquez d'abord sur « Créer la classification » pour générer le système CostWaves."),
						  FR ("OK"));
		return;
	}

	// 4) Affectation (annulable, une seule commande pour tous les éléments)
	//    + propriété CW_Article_ID (créée si absente).
	GS::UniString propError;
	const API_Guid articleIdPropGuid = ArticleManager::EnsureArticleIdProperty (propError);

	USize changedCount = 0;
	USize failedCount = 0;
	GS::UniString error;
	const GSErrCode err = ArticleManager::AssignArticleToElements (elemGuids, targetSystem, itemGuid,
																   article.id, articleIdPropGuid,
																   changedCount, failedCount, error);
	if (err != NoError) {
		DG::ErrorAlert (FR ("Échec de l'affectation."), error, FR ("OK"));
		return;
	}

	RefreshData ();

	// Message de résultat.
	GS::UniString summary = GS::ToUniString (std::to_wstring (static_cast<int> (changedCount)))
							 + FR (" élément(s) mis à jour");
	if (failedCount > 0)
		summary += FR (" · ") + GS::ToUniString (std::to_wstring (static_cast<int> (failedCount)))
				 + FR (" échec(s)");
	if (skippedComponents > 0)
		summary += FR (" · ") + GS::ToUniString (std::to_wstring (static_cast<int> (skippedComponents)))
				 + FR (" ligne(s) composant ignorée(s)");
	if (articleIdPropGuid == APINULLGuid)
		summary += FR (" · propriété CW_Article_ID non disponible");
	summary += ".";

	if (failedCount > 0) {
		DG::WarningAlert (FR ("Affectation terminée (avec échecs)."), summary, FR ("OK"));
	} else if (changedCount == 0) {
		DG::InformationAlert (FR ("Aucun changement."), FR ("Ces éléments portent déjà cet article."), FR ("OK"));
	} else if (targetSystem != selectedSystem) {
		DG::InformationAlert (FR ("Articles affectés."),
							  summary + FR ("\nAffectés dans le système « CostWaves » — basculez le système en haut pour le voir dans la colonne Classe."),
							  FR ("OK"));
	} else {
		DG::InformationAlert (FR ("Articles affectés."), summary, FR ("OK"));
	}
}


void CostWavesDialog::CreateMaterials ()
{
	if (articles.IsEmpty ()) {
		DG::WarningAlert (FR ("Aucun article disponible."),
						  FR ("Importez des articles (JSON) ou choisissez un système contenant des items."),
						  FR ("OK"));
		return;
	}

	API_Guid	systemGuid = APINULLGuid;
	USize		createdMaterials = 0;
	USize		createdItems = 0;
	USize		assigned = 0;
	GS::UniString error;
	if (ArticleManager::CreateBuildingMaterials (articles, systemGuid, createdMaterials, createdItems,
												  assigned, error) != NoError) {
		DG::ErrorAlert (FR ("Échec de la création des matériaux."), error, FR ("OK"));
		return;
	}

	// Le système « CostWaves » a pu être créé : recharger et resélectionner
	// (sans déclencher la relecture via PopUpChanged).
	LoadSystems ();

	isFilling = true;
	const GS::UniString costWavesName (ArticleManager::CostWavesSystemName (), CC_UTF8);
	for (UIndex i = 0; i < systems.GetSize (); ++i) {
		if (systems[i].name == costWavesName) {
			selectedSystem = systems[i].guid;
			systemPopup.SelectItem (static_cast<short> (i + 1));
			break;
		}
	}
	isFilling = false;

	ReloadArticlesFromSystem ();
	RefreshData ();

	DG::InformationAlert (FR ("Matériaux CostWaves prêts."),
						  GS::ToUniString (std::to_wstring (static_cast<int> (createdMaterials)))
							  + FR (" matériaux traités · ")
							  + GS::ToUniString (std::to_wstring (static_cast<int> (createdItems)))
							  + FR (" items créés · ")
							  + GS::ToUniString (std::to_wstring (static_cast<int> (assigned)))
							  + FR (" affectations.\nNB : la création de matériaux n'est pas annulable (limite API)."),
						  FR ("OK"));
}


void CostWavesDialog::UpdateDetails (short listItem)
{
	ClearDetails ();

	if (listItem < 1 || static_cast<UIndex> (listItem) > displayRows.GetSize ())
		return;

	const DisplayRow& displayRow = displayRows[static_cast<UIndex> (listItem) - 1];
	if (displayRow.elementIndex >= rows.GetSize ())
		return;

	CWElementRow& element = rows[displayRow.elementIndex];

	const GS::UniString floorText = element.storyName.IsEmpty ()
		? GS::ToUniString (std::to_wstring (static_cast<int> (element.floorInd)))
		: GS::ToUniString (std::to_wstring (static_cast<int> (element.floorInd))) + " - " + element.storyName;

	if (displayRow.kind == RowKind::Element) {
		SetDetailLine (1, FR ("Élément — ") + element.typeName + " — " + APIGuidToString (element.guid));
		SetDetailLine (2, FR ("ID : ") + (element.elementId.IsEmpty () ? FR ("(vide)") : element.elementId)
			+ FR (" · Étage : ") + floorText
			+ FR (" · Type : ") + element.typeName);
		SetDetailLine (3, FR ("Classe : ") + element.classItemId + " (" + element.classItemName + ")");

		// Quantités complètes, réparties sur les lignes restantes.
		GS::UniString line;
		short lineIndex = 4;
		for (UIndex q = 0; q < element.quantities.GetSize (); ++q) {
			const GS::UniString chunk = element.quantities[q].label + " = "
				+ FormatValue (element.quantities[q].value) + " " + element.quantities[q].unit;
			if (line.IsEmpty ())
				line = chunk;
			else if (LineFits (line, chunk))
				line += US (" · ") + chunk;
			else {
				SetDetailLine (lineIndex, line);
				++lineIndex;
				if (lineIndex > 5)
					return;
				line = chunk;
			}
		}
		if (!line.IsEmpty () && lineIndex <= 5)
			SetDetailLine (lineIndex, line);
		return;
	}

	// --- Ligne composant / skin ---
	if (displayRow.componentIndex >= element.components.GetSize ())
		return;

	CWComponentRow& component = element.components[displayRow.componentIndex];

	short lineIndex = 1;
	if (component.kind == RowKind::Skin) {
		GS::UniString title = FR ("Skin (composite) — matériau : ") + component.label;
		if (component.coreSkin)
			title += FR (" · cœur");
		if (component.finishSkin)
			title += FR (" · finition");
		SetDetailLine (lineIndex, title);
		++lineIndex;

		if (!component.compositeName.IsEmpty ()) {
			GS::UniString compositeLine = FR ("Composite : ") + component.compositeName;
			if (component.skinIndex >= 0 && component.skinCount > 0) {
				compositeLine += FR (" · couche ")
					+ GS::ToUniString (std::to_wstring (component.skinIndex + 1))
					+ "/" + GS::ToUniString (std::to_wstring (component.skinCount));
			}
			SetDetailLine (lineIndex, compositeLine);
			++lineIndex;
		}

		SetDetailLine (lineIndex, FR ("Élément parent — ") + element.typeName + " — " + APIGuidToString (element.guid));
		++lineIndex;
	} else {
		SetDetailLine (lineIndex, FR ("Composant — ") + APIGuidToString (component.guid));
		++lineIndex;
		SetDetailLine (lineIndex, FR ("Élément parent — ") + element.typeName + " — "
			+ APIGuidToString (element.guid) + " · ID : " + element.elementId);
		++lineIndex;
	}

	if (!component.quantities.IsEmpty ()) {
		GS::UniString line;
		for (UIndex q = 0; q < component.quantities.GetSize (); ++q) {
			const GS::UniString chunk = component.quantities[q].label + " = "
				+ FormatValue (component.quantities[q].value) + " " + component.quantities[q].unit;
			if (line.IsEmpty ())
				line = chunk;
			else {
				SetDetailLine (lineIndex, line);
				++lineIndex;
				if (lineIndex > 5)
					return;
				line = chunk;
			}
		}
		if (!line.IsEmpty () && lineIndex <= 5)
			SetDetailLine (lineIndex, line);
		++lineIndex;
	}

	// Propriétés du composant, lues à la demande.
	if (component.kind == RowKind::Component && !component.propertiesFetched) {
		API_ElemComponentID componentId;
		BNZeroMemory (&componentId, sizeof (componentId));
		componentId.elemGuid = element.guid;
		componentId.componentID.componentGuid = component.guid;

		component.properties = ModelReader::GetComponentProperties (componentId);
		component.propertiesFetched = true;
	}

	if (component.kind == RowKind::Component) {
		if (component.properties.IsEmpty ()) {
			SetDetailLine (lineIndex <= 5 ? lineIndex : 5, FR ("Aucune propriété de composant."));
		} else {
			GS::UniString line;
			for (UIndex p = 0; p < component.properties.GetSize (); ++p) {
				const GS::UniString chunk = component.properties[p].name + " : " + component.properties[p].value;
				if (line.IsEmpty ())
					line = chunk;
				else if (LineFits (line, chunk))
					line += US (" · ") + chunk;
				else {
					SetDetailLine (lineIndex, line);
					++lineIndex;
					if (lineIndex > 5)
						return;
					line = chunk;
				}
			}
			if (!line.IsEmpty () && lineIndex <= 5)
				SetDetailLine (lineIndex, line);
		}
	}
}


void CostWavesDialog::Export (bool jsonFormat)
{
	if (rows.IsEmpty ()) {
		DG::WarningAlert (FR ("Aucune donnée à exporter."),
						  FR ("Lancez d'abord une lecture avec un système de classification."),
						  FR ("OK"));
		return;
	}

	GS::UniString systemName;
	for (UIndex i = 0; i < systems.GetSize (); ++i) {
		if (systems[i].guid == selectedSystem) {
			systemName = systems[i].name;
			break;
		}
	}

	GS::UniString path;
	GS::UniString error;

	const GSErrCode err = jsonFormat
		? Exporter::ExportJSON (systemName, rows, report, articles, path, error)
		: Exporter::ExportCSV (systemName, rows, report, articles, path, error);

	if (err == NoError) {
		DG::InformationAlert (FR ("Export réussi."), path, FR ("OK"));
	} else {
		DG::ErrorAlert (FR ("Échec de l'export."), error, FR ("OK"));
	}
}


void CostWavesDialog::PanelResized (const DG::PanelResizeEvent& ev)
{
	BeginMoveResizeItems ();

	const short dx = ev.GetHorizontalChange ();
	const short dy = ev.GetVerticalChange ();

	// Zone fixe au-dessus du tableau : s'élargit seulement.
	systemPopup.MoveAndResize (0, 0, dx, 0);
	statusText.MoveAndResize (0, 0, dx, 0);
	// Rangée recherche (fixe) : le texte d'info absorbe la largeur.
	searchEdit.MoveAndResize (0, 0, dx / 2, 0);
	articlesInfo.MoveAndResize (0, 0, dx - dx / 2, 0);

	// Le tableau absorbe le redimensionnement vertical.
	table.MoveAndResize (0, 0, dx, dy);

	// Rangées « articles » (sous le tableau) : descendent avec dy.
	articlePopup.MoveAndResize (0, dy, dx, 0);
	assignButton.Move (dx, dy);
	importButton.Move (0, dy);
	createClassButton.Move (0, dy);
	createMaterialsButton.Move (dx / 2, dy);

	// Panneau de détails.
	detailsGroup.MoveAndResize (0, dy, dx, 0);
	detail1.MoveAndResize (0, dy, dx, 0);
	detail2.MoveAndResize (0, dy, dx, 0);
	detail3.MoveAndResize (0, dy, dx, 0);
	detail4.MoveAndResize (0, dy, dx, 0);
	detail5.MoveAndResize (0, dy, dx, 0);

	// Boutons du bas.
	exportJsonButton.Move (0, dy);
	exportCsvButton.Move (0, dy);
	closeButton.MoveAndResize (dx, dy, 0, 0);

	EndMoveResizeItems ();
}


void CostWavesDialog::ButtonClicked (const DG::ButtonClickEvent& ev)
{
	if (ev.GetSource () == &refreshButton) {
		RefreshData ();
	} else if (ev.GetSource () == &exportJsonButton) {
		Export (true);
	} else if (ev.GetSource () == &exportCsvButton) {
		Export (false);
	} else if (ev.GetSource () == &importButton) {
		ImportArticles ();
	} else if (ev.GetSource () == &createClassButton) {
		CreateClassification ();
	} else if (ev.GetSource () == &assignButton) {
		AssignCurrentArticle ();
	} else if (ev.GetSource () == &createMaterialsButton) {
		CreateMaterials ();
	} else if (ev.GetSource () == &closeButton) {
		PostCloseRequest (DG::ModalDialog::Cancel);
	}
}


void CostWavesDialog::PopUpChanged (const DG::PopUpChangeEvent& ev)
{
	if (isFilling)
		return;

	if (ev.GetSource () == &systemPopup) {
		const short selection = systemPopup.GetSelectedItem ();
		if (selection >= 1 && static_cast<UIndex> (selection) <= systems.GetSize ()) {
			selectedSystem = systems[static_cast<UIndex> (selection) - 1].guid;
			ReloadArticlesFromSystem ();
			RefreshData ();
		}
	}
}


void CostWavesDialog::ListBoxSelectionChanged (const DG::ListBoxSelectionEvent& ev)
{
	if (isFilling)
		return;

	if (ev.GetSource () == &table)
		UpdateDetails (table.GetSelectedItem ());
}


void CostWavesDialog::ListBoxHeaderItemClicked (const DG::ListBoxHeaderItemClickEvent& ev)
{
	if (ev.GetSource () != &table)
		return;

	const short column = ev.GetHeaderItem ();
	if (column < 1 || column > 6)
		return;

	// Colonne identique : inverser le sens ; nouvelle colonne : croissant.
	if (sortColumn == column)
		sortAscending = !sortAscending;
	else {
		sortColumn = column;
		sortAscending = true;
	}

	// Réafficher trié, sans relire le modèle.
	isFilling = true;
	FillTable ();
	isFilling = false;

	UpdateDetails (table.GetItemCount () > 0 ? 1 : 0);
}


void CostWavesDialog::SearchTextChanged (const DG::SearchEditChangeEvent& ev)
{
	if (ev.GetSource () != &searchEdit)
		return;

	const GS::UniString text = searchEdit.GetText ();

	if (text == searchFilter)
		return;		// rien de nouveau

	searchFilter = text;

	// Réafficher le tableau avec le nouveau filtre, sans relire le modèle.
	isFilling = true;
	FillTable ();
	isFilling = false;

	UpdateDetails (table.GetItemCount () > 0 ? 1 : 0);
}

} // namespace CostWaves
