#include "CostWavesPrecompiledHeader.hpp"

#include "CostWavesDialog.hpp"

#include "ArticleManager.hpp"
#include "ArticlePickerDialog.hpp"
#include "Exporter.hpp"
#include "MaterialDialog.hpp"
#include "ModelReader.hpp"
#include "SummaryDialog.hpp"

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

// Article correspondant à l'identifiant (nullptr si aucun) — phase 4.
const CWArticle* FindArticleById (const GS::Array<CWArticle>& articles, const GS::UniString& articleId)
{
	if (articleId.IsEmpty ())
		return nullptr;

	for (UIndex i = 0; i < articles.GetSize (); ++i) {
		if (articles[i].id == articleId)
			return &articles[i];
	}
	return nullptr;
}

// Garde anti-réentrance : remet le drapeau à false en sortant de portée,
// quelle que soit la façon de sortir (return anticipé compris).
struct BoolFlagGuard {
	bool&	flag;
	explicit BoolFlagGuard (bool& inFlag) : flag (inFlag) { flag = true; }
	~BoolFlagGuard () { flag = false; }
};

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
		case 5: {
			// (a + b + c) renvoie une Concatenation, pas une UniString :
			// passer par des locales pour pouvoir appeler ToUpperCase.
			const GS::UniString keyA = a.classItemId + US (" ") + a.classItemName;
			const GS::UniString keyB = b.classItemId + US (" ") + b.classItemName;
			return keyA.ToUpperCase () < keyB.ToUpperCase ();
		}
		case 6:
			return FirstQuantityValue (a) < FirstQuantityValue (b);
		default:
			return false;
	}
}

} // namespace


// --- Singleton de la palette (phase 5) -----------------------------------------

const GS::Guid CostWavesDialog::paletteGuid ("{2F7C4D18-A5B3-4E69-9C0D-71E8B4A2D935");
GS::Ref<CostWavesDialog> CostWavesDialog::instance;


bool CostWavesDialog::HasInstance ()
{
	return instance != nullptr;
}


CostWavesDialog& CostWavesDialog::Instance ()
{
	if (!HasInstance ())
		instance = new CostWavesDialog ();
	return *instance;
}


GSErrCode CostWavesDialog::PaletteControlCallBack (Int32 /*paletteId*/, API_PaletteMessageID messageID, GS::IntPtr param)
{
	switch (messageID) {
		case APIPalMsg_OpenPalette:
			Instance ().ShowPalette ();
			break;

		case APIPalMsg_ClosePalette:
			if (HasInstance ())
				Instance ().HidePalette ();
			break;

		case APIPalMsg_HidePalette_Begin:
			if (HasInstance () && Instance ().IsVisible ())
				Instance ().HidePalette ();
			break;

		case APIPalMsg_HidePalette_End:
			if (HasInstance () && !Instance ().IsVisible ())
				Instance ().ShowPalette ();
			break;

		case APIPalMsg_IsPaletteVisible:
			*(reinterpret_cast<bool*> (param)) = HasInstance () && Instance ().IsVisible ();
			break;

		default:
			break;
	}

	return NoError;
}


GSErrCode CostWavesDialog::RegisterPalette ()
{
	return ACAPI_RegisterModelessWindow (
					static_cast<Int32> (GS::CalculateHashValue (paletteGuid)),
					PaletteControlCallBack,
					API_PalEnabled_FloorPlan + API_PalEnabled_Section + API_PalEnabled_Elevation +
					API_PalEnabled_InteriorElevation + API_PalEnabled_3D + API_PalEnabled_Detail +
					API_PalEnabled_Worksheet + API_PalEnabled_Layout + API_PalEnabled_DocumentFrom3D,
					GSGuid2APIGuid (paletteGuid));
}


GSErrCode CostWavesDialog::SelectionChangeHandler (const API_Neig* /*selElemNeig*/)
{
	if (HasInstance ())
		Instance ().RefreshFromSelectionChange ();
	return NoError;
}


void CostWavesDialog::ShowPalette ()
{
	DG::Palette::Show ();
}


void CostWavesDialog::HidePalette ()
{
	DG::Palette::Hide ();
}


bool CostWavesDialog::WantsSelectionFollow () const
{
	return selectionCheck.IsChecked ();
}


void CostWavesDialog::RefreshFromSelectionChange ()
{
	// Ne rien faire si une boîte modale est ouverte, si une lecture est en
	// cours, ou si la palette ne suit pas la sélection.
	if (inModalDialog || refreshing || !IsVisible () || !WantsSelectionFollow ())
		return;

	RefreshData ();
}


CostWavesDialog::CostWavesDialog ()
	:	DG::Palette (ACAPI_GetOwnResModule (), DialogResourceId, ACAPI_GetOwnResModule (), paletteGuid),
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
		groupButton (GetReference (), GroupButtonId),
		createGroupButton (GetReference (), CreateGroupButtonId),
		ungroupButton (GetReference (), UngroupButtonId),
		summaryButton (GetReference (), SummaryButtonId),
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
	importButton.Attach (*this);		// ButtonItemObserver
	createClassButton.Attach (*this);// ButtonItemObserver
	createMaterialsButton.Attach (*this);	// ButtonItemObserver
	groupButton.Attach (*this);		// ButtonItemObserver
	createGroupButton.Attach (*this);	// ButtonItemObserver
	ungroupButton.Attach (*this);	// ButtonItemObserver
	summaryButton.Attach (*this);	// ButtonItemObserver
	searchEdit.Attach (*this);		// SearchEditObserver
	exportJsonButton.Attach (*this);
	exportCsvButton.Attach (*this);
	closeButton.Attach (*this);

	InitTable ();

	ModelReader::ResolveElementIdPropertyGuid (elemIdPropGuid, elemIdPropNote);

	LoadSystems ();
	ReloadArticlesFromSystem ();

	// La palette suit par défaut la sélection du plan (case cochée) : on
	// sélectionne dans Archicad, la palette affiche et actualise.
	selectionCheck.Check ();

	if (selectedSystem != APINULLGuid)
		RefreshData ();
	else
		ClearDetails ();

	BeginEventProcessing ();
}


CostWavesDialog::~CostWavesDialog ()
{
	EndEventProcessing ();
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

	// Le GRC ne définit pas les colonnes : créer les 6 champs de tabulation
	// (sans cela, seule la première colonne s'affiche).
	table.SetTabFieldCount (columnCount);

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
	// Anti-réentrance (callback de sélection pendant une lecture).
	if (refreshing)
		return;
	BoolFlagGuard refreshingGuard (refreshing);

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
	// CW_Group_ID : résolu à chaque lecture (peut être créé entre-temps).
	groupPropGuid = ArticleManager::FindGroupIdPropertyGuid ();
	ModelReader::Scan (selectedSystem, elemIdPropGuid, groupPropGuid, filter, rows, report);
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

		// Les membres d'un ensemble ne sont pas affichés au premier niveau :
		// ils apparaissent en sous-lignes de leur ligne « Ensemble ».
		if (element.consumed)
			continue;

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

		// Quantité facturée (ensembles : forfait ENS ou somme des membres ;
		// groupes numérotés : 1 par groupe).
		GS::UniString quantitiesText = QuantitiesSummary (element.quantities, 3);
		if (element.isGroupRow) {
			const CWArticle* article = FindArticleById (articles, element.classItemId);
			if (article != nullptr) {
				GS::UniString unit;
				const double quantity = ArticleManager::ComputeBilledQuantity (*article, element, rows, unit);
				quantitiesText = article->id + " · " + FormatValue (quantity) + " " + unit;
				if (element.isNumberedGroup)
					quantitiesText += FR (" · groupe n° ")
						+ GS::ToUniString (std::to_wstring (element.groupNumber));
			} else {
				quantitiesText = FR ("Article inconnu");
			}
		}

		table.AppendItem ();
		const short itemIndex = table.GetItemCount ();
		table.SetTabItemText (itemIndex, 1,
			element.isGroupRow
				? (element.isNumberedGroup
					? FR ("Groupe n° ") + GS::ToUniString (std::to_wstring (element.groupNumber))
					: FR ("Ensemble"))
				: FR ("Élément"));
		table.SetTabItemText (itemIndex, 2, APIGuidToString (element.guid));
		table.SetTabItemText (itemIndex, 3, element.elementId);
		table.SetTabItemText (itemIndex, 4, floorText);
		table.SetTabItemText (itemIndex, 5, classText);
		table.SetTabItemText (itemIndex, 6, quantitiesText);

		DisplayRow elementRow;
		elementRow.kind = element.isGroupRow ? RowKind::Group : RowKind::Element;
		elementRow.elementIndex = e;
		displayRows.Push (elementRow);

		// Membres de l'ensemble (sous-lignes « consommées »).
		for (UIndex m = 0; m < element.groupMembers.GetSize (); ++m) {
			const CWElementRow* memberRow = nullptr;
			UIndex memberIndex = 0;
			for (UIndex r = 0; r < rows.GetSize (); ++r) {
				if (rows[r].guid == element.groupMembers[m]) {
					memberRow = &rows[r];
					memberIndex = r;
					break;
				}
			}
			if (memberRow == nullptr || memberRow->isGroupRow)
				continue;

			const GS::UniString memberFloor = memberRow->storyName.IsEmpty ()
				? GS::ToUniString (std::to_wstring (static_cast<int> (memberRow->floorInd)))
				: GS::ToUniString (std::to_wstring (static_cast<int> (memberRow->floorInd))) + " - " + memberRow->storyName;

			table.AppendItem ();
			const short memberItemIndex = table.GetItemCount ();
			table.SetTabItemText (memberItemIndex, 1, FR ("Membre (consommé)"));
			table.SetTabItemText (memberItemIndex, 2, APIGuidToString (memberRow->guid));
			table.SetTabItemText (memberItemIndex, 3, memberRow->elementId);
			table.SetTabItemText (memberItemIndex, 4, memberFloor);
			table.SetTabItemText (memberItemIndex, 5, classText);
			table.SetTabItemText (memberItemIndex, 6, QuantitiesSummary (memberRow->quantities, 3));

			DisplayRow memberDisplay;
			memberDisplay.kind = RowKind::GroupMember;
			memberDisplay.elementIndex = e;
			memberDisplay.memberRowIndex = memberIndex;
			displayRows.Push (memberDisplay);
		}

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

	if (report.groupCount > 0 || report.numberedGroupCount > 0) {
		status += FR (" · ")
			+ GS::ToUniString (std::to_wstring (static_cast<int> (report.groupCount)))
			+ FR (" ensemble(s) · ")
			+ GS::ToUniString (std::to_wstring (static_cast<int> (report.numberedGroupCount)))
			+ FR (" groupe(s) numéroté(s) · ")
			+ GS::ToUniString (std::to_wstring (static_cast<int> (report.consumedElements)))
			+ FR (" consommé(s)");
	}

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
	//    Une ligne « Ensemble » applique l'article à tous ses membres.
	GS::Array<API_Guid>	elemGuids;
	USize			skippedComponents = 0;
	USize			skippedConsumed = 0;
	USize			selectedGroups = 0;

	const GS::Array<short> selectedItems = table.GetSelectedItems ();
	for (UIndex i = 0; i < selectedItems.GetSize (); ++i) {
		const short listItem = selectedItems[i];
		if (listItem < 1 || static_cast<UIndex> (listItem) > displayRows.GetSize ())
			continue;

		const DisplayRow& displayRow = displayRows[static_cast<UIndex> (listItem) - 1];
		if (displayRow.elementIndex >= rows.GetSize ())
			continue;

		if (displayRow.kind == RowKind::Group) {
			// Ensemble : l'article est appliqué à tous les membres.
			const CWElementRow& groupRow = rows[displayRow.elementIndex];
			for (UIndex m = 0; m < groupRow.groupMembers.GetSize (); ++m)
				elemGuids.Push (groupRow.groupMembers[m]);
			++selectedGroups;
			continue;
		}

		if (displayRow.kind == RowKind::GroupMember) {
			++skippedConsumed;
			continue;
		}

		if (displayRow.kind != RowKind::Element) {
			++skippedComponents;
			continue;
		}

		elemGuids.Push (rows[displayRow.elementIndex].guid);
	}

	// Déduplication (ensemble + membres sélectionnés ensemble).
	for (UIndex i = 0; i < elemGuids.GetSize (); ++i) {
		for (UIndex k = i + 1; k < elemGuids.GetSize (); ) {
			if (elemGuids[k] == elemGuids[i])
				elemGuids.Delete (k);
			else
				++k;
		}
	}

	if (elemGuids.IsEmpty ()) {
		if (skippedConsumed > 0) {
			DG::WarningAlert (FR ("L'affectation se fait sur des éléments."),
							  FR ("Les membres d'un ensemble sont facturés via leur ensemble — dissolvez-le d'abord (bouton « Dissoudre l'ensemble ») pour les affecter individuellement."),
							  FR ("OK"));
		} else if (skippedComponents > 0) {
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

	if (!ResolveArticleTarget (article.id, targetSystem, itemGuid)) {
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
	if (selectedGroups > 0)
		summary += FR (" · ") + GS::ToUniString (std::to_wstring (static_cast<int> (selectedGroups)))
				 + FR (" ensemble(s) reclassé(s)");
	if (skippedConsumed > 0)
		summary += FR (" · ") + GS::ToUniString (std::to_wstring (static_cast<int> (skippedConsumed)))
				 + FR (" membre(s) ignoré(s)");
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
	if (systems.IsEmpty ()) {
		DG::WarningAlert (FR ("Aucun système de classification."),
						  FR ("Importez des articles ou créez la classification avant de créer un matériau."),
						  FR ("OK"));
		return;
	}

	inModalDialog = true;
	MaterialDialog dialog (systems);
	dialog.Invoke ();
	inModalDialog = false;

	if (!dialog.IsAccepted ())
		return;

	GS::UniString summary;
	GS::UniString error;
	if (!dialog.Apply (summary, error)) {
		DG::ErrorAlert (FR ("Échec de la création du matériau."), error, FR ("OK"));
		return;
	}

	// Une classe a pu être créée : recharger les systèmes et les articles en
	// conservant la sélection courante si possible.
	const API_Guid previousSystem = selectedSystem;
	LoadSystems ();

	isFilling = true;
	for (UIndex i = 0; i < systems.GetSize (); ++i) {
		if (systems[i].guid == previousSystem) {
			selectedSystem = previousSystem;
			systemPopup.SelectItem (static_cast<short> (i + 1));
			break;
		}
	}
	isFilling = false;

	ReloadArticlesFromSystem ();
	RefreshData ();

	DG::InformationAlert (FR ("Matériau CostWaves prêt."), summary, FR ("OK"));
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

	// --- Ligne ensemble / groupe numéroté (phases 4-5) ---
	if (displayRow.kind == RowKind::Group) {
		if (element.isNumberedGroup) {
			SetDetailLine (1, FR ("Groupe n° ")
				+ GS::ToUniString (std::to_wstring (element.groupNumber))
				+ FR (" — ") + element.classItemId);
		} else {
			SetDetailLine (1, FR ("Ensemble — ") + element.groupId);
		}
		SetDetailLine (2, FR ("Article : ") + element.classItemId + " (" + element.classItemName + ")"
		+ FR (" · Étage : ") + floorText);

		const CWArticle* article = FindArticleById (articles, element.classItemId);
		GS::UniString billedLine = FR ("Membres : ")
		+ GS::ToUniString (std::to_wstring (static_cast<int> (element.groupMembers.GetSize ())))
		+ FR (" (consommés, facturés via leur groupe)");
		if (article != nullptr) {
			GS::UniString unit;
			const double quantity = ArticleManager::ComputeBilledQuantity (*article, element, rows, unit);
			billedLine += FR (" · Facturé : ") + FormatValue (quantity) + " " + unit;
			if (element.isNumberedGroup)
				billedLine += FR (" — la quantité réelle du métré est le nombre de groupes de l'article");
		} else {
			billedLine += FR (" · Article inconnu (importez-le pour la facturation)");
		}
		SetDetailLine (3, billedLine);
		return;
	}

	// --- Ligne membre consommé (phase 4) ---
	if (displayRow.kind == RowKind::GroupMember) {
		if (displayRow.memberRowIndex >= rows.GetSize ())
			return;
		const CWElementRow& member = rows[displayRow.memberRowIndex];

		const GS::UniString memberFloor = member.storyName.IsEmpty ()
			? GS::ToUniString (std::to_wstring (static_cast<int> (member.floorInd)))
			: GS::ToUniString (std::to_wstring (static_cast<int> (member.floorInd))) + " - " + member.storyName;

		SetDetailLine (1, FR ("Membre (consommé) — ") + member.typeName + " — " + APIGuidToString (member.guid));
		SetDetailLine (2, FR ("Ensemble : ") + member.groupId
			+ FR (" · Type : ") + member.typeName
			+ FR (" · Étage : ") + memberFloor);
		SetDetailLine (3, FR ("Non facturé individuellement — l'ensemble ") + member.groupId
			+ FR (" est facturé à sa place."));

		// Quantités du membre, réparties sur les lignes restantes.
		GS::UniString line;
		short lineIndex = 4;
		for (UIndex q = 0; q < member.quantities.GetSize (); ++q) {
			const GS::UniString chunk = member.quantities[q].label + " = "
				+ FormatValue (member.quantities[q].value) + " " + member.quantities[q].unit;
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


// --- Phase 4 : ensembles CostWaves --------------------------------------------

bool CostWavesDialog::ResolveArticleTarget (const GS::UniString& articleId,
											API_Guid& outSystemGuid, API_Guid& outItemGuid)
{
	outSystemGuid = APINULLGuid;
	outItemGuid = APINULLGuid;

	const API_Guid costWavesSystem = ArticleManager::FindCostWavesSystemGuid ();
	if (costWavesSystem != APINULLGuid) {
		const API_Guid guid = ArticleManager::FindItemGuid (costWavesSystem, articleId);
		if (guid != APINULLGuid) {
			outSystemGuid = costWavesSystem;
			outItemGuid = guid;
			return true;
		}
	}

	if (selectedSystem != APINULLGuid) {
		const API_Guid guid = ArticleManager::FindItemGuid (selectedSystem, articleId);
		if (guid != APINULLGuid) {
			outSystemGuid = selectedSystem;
			outItemGuid = guid;
			return true;
		}
	}

	return false;
}


void CostWavesDialog::CreateEnsembleOrGroup (bool numbered)
{
	// 1) Éléments sélectionnés DANS LE PLAN (la palette ne bloque pas la
	//    sélection : on sélectionne dans Archicad puis on clique ici).
	GS::Array<API_Guid> selection;
	if (ModelReader::GetSelectedElements (selection) != NoError || selection.IsEmpty ()) {
		DG::WarningAlert (FR ("Aucun élément sélectionné dans le plan."),
						  FR ("Sélectionnez un ou plusieurs éléments dans Archicad, puis cliquez sur le bouton : la fenêtre de choix de l'article s'ouvrira."),
						  FR ("OK"));
		return;
	}

	// 2) Choix de l'article (classe) dans une fenêtre dédiée.
	if (articles.IsEmpty ()) {
		DG::WarningAlert (FR ("Aucun article disponible."),
						  FR ("Importez des articles (JSON) ou choisissez un système contenant des items."),
						  FR ("OK"));
		return;
	}

	inModalDialog = true;
	ArticlePickerDialog picker (articles, selection.GetSize (), numbered);
	picker.Invoke ();
	inModalDialog = false;

	if (!picker.IsAccepted ())
		return;
	const CWArticle article = picker.GetSelectedArticle ();

	// 3) Item de classification de l'article (système « CostWaves » en
	//    priorité, sinon le système courant).
	API_Guid targetSystem = APINULLGuid;
	API_Guid itemGuid = APINULLGuid;
	if (!ResolveArticleTarget (article.id, targetSystem, itemGuid)) {
		DG::WarningAlert (FR ("Article introuvable dans les classifications."),
						  FR ("Cliquez d'abord sur « Créer la classification » pour générer le système CostWaves."),
						  FR ("OK"));
		return;
	}

	// 4) Propriétés CW_Article_ID et CW_Group_ID.
	GS::UniString articlePropError;
	const API_Guid articleIdPropGuid = ArticleManager::EnsureArticleIdProperty (articlePropError);

	GS::UniString groupPropError;
	const API_Guid groupIdPropGuid = ArticleManager::EnsureGroupIdProperty (groupPropError);
	if (groupIdPropGuid == APINULLGuid) {
		DG::ErrorAlert (numbered ? FR ("Création du groupe impossible.") : FR ("Création de l'ensemble impossible."),
						groupPropError.IsEmpty () ? FR ("Propriété CW_Group_ID indisponible.") : groupPropError,
						FR ("OK"));
		return;
	}

	// 5) Valeur du groupe : numéro séquentiel pour un groupe numéroté,
	//    identifiant horodaté pour un ensemble. Les valeurs existantes sont
	//    lues sur TOUT le projet (pas seulement les lignes affichées).
	GS::Array<GS::Pair<API_Guid, GS::UniString>> groupValues;
	ModelReader::CollectGroupValues (groupIdPropGuid, groupValues);

	GS::Array<GS::UniString> existingIds;
	for (UIndex i = 0; i < groupValues.GetSize (); ++i)
		existingIds.Push (groupValues[i].second);

	// 6) Écarter les éléments déjà groupés (ils appartiennent à un autre
	//    ensemble/groupe : il faudrait le dissoudre d'abord).
	GS::Array<API_Guid> elemGuids;
	USize skippedGrouped = 0;
	for (UIndex s = 0; s < selection.GetSize (); ++s) {
		bool alreadyGrouped = false;
		for (UIndex g = 0; g < groupValues.GetSize (); ++g) {
			if (groupValues[g].first == selection[s] && !groupValues[g].second.IsEmpty ()) {
				alreadyGrouped = true;
				break;
			}
		}
		if (alreadyGrouped) {
			++skippedGrouped;
			continue;
		}
		elemGuids.Push (selection[s]);
	}

	if (elemGuids.IsEmpty ()) {
		DG::WarningAlert (FR ("Ces éléments appartiennent déjà à un ensemble ou un groupe."),
						  FR ("Dissolvez l'ensemble/le groupe concerné avant de les regrouper ailleurs (bouton « Dissoudre ensemble / groupe »)."),
						  FR ("OK"));
		return;
	}

	const GS::UniString groupValue = numbered
		? ArticleManager::NumberedGroupValue (ArticleManager::NextGroupNumber (existingIds))
		: ArticleManager::GenerateGroupId (existingIds);

	// 7) Création (une seule commande annulable pour tout le groupe).
	USize changedCount = 0;
	USize failedCount = 0;
	GS::UniString error;
	const GSErrCode err = ArticleManager::CreateGroupFromElements (elemGuids, targetSystem, itemGuid,
															   article.id, articleIdPropGuid,
															   groupValue, groupIdPropGuid,
															   numbered ? FR ("CostWaves : création d'un groupe")
																		: FR ("CostWaves : création d'un ensemble"),
															   changedCount, failedCount, error);
	if (err != NoError) {
		DG::ErrorAlert (numbered ? FR ("Échec de la création du groupe.") : FR ("Échec de la création de l'ensemble."),
						error, FR ("OK"));
		return;
	}

	RefreshData ();

	GS::UniString summary;
	if (numbered) {
		int createdNumber = 0;
		ArticleManager::ParseNumberedGroupValue (groupValue, createdNumber);
		summary = FR ("Groupe n° ")
			+ GS::ToUniString (std::to_wstring (createdNumber))
			+ FR (" créé — ")
			+ GS::ToUniString (std::to_wstring (static_cast<int> (changedCount)))
			+ FR (" élément(s) regroupé(s) sur l'article ") + article.id + " (" + article.name + ").";
		summary += FR ("\nLa quantité facturée de cet article est le NOMBRE de groupes : créez autant de groupes que d'unités à facturer.");
	} else {
		summary = FR ("Ensemble ") + groupValue + FR (" créé — ")
			+ GS::ToUniString (std::to_wstring (static_cast<int> (changedCount)))
			+ FR (" élément(s) regroupé(s) sur l'article ") + article.id + " (" + article.name + ").";
		if (ArticleManager::IsEnsUnit (article.unit))
			summary += FR ("\nFacturation à l'ensemble : 1 ENS.");
		else
			summary += FR ("\nFacturé à l'unité de l'article (") + article.unit
					 + FR (") : somme des quantités des membres.");
	}
	if (failedCount > 0)
		summary += FR ("\n") + GS::ToUniString (std::to_wstring (static_cast<int> (failedCount)))
				 + FR (" échec(s).");
	if (skippedGrouped > 0)
		summary += FR ("\n") + GS::ToUniString (std::to_wstring (static_cast<int> (skippedGrouped)))
				 + FR (" élément(s) ignoré(s) — déjà groupé(s).");

	DG::InformationAlert (numbered ? FR ("Groupe créé.") : FR ("Ensemble créé."), summary, FR ("OK"));
}


void CostWavesDialog::UngroupSelected ()
{
	// Lignes sélectionnées : ensembles (tous leurs membres) ou membres.
	GS::Array<API_Guid>	elemGuids;
	USize			skippedPlain = 0;

	const GS::Array<short> selectedItems = table.GetSelectedItems ();
	for (UIndex i = 0; i < selectedItems.GetSize (); ++i) {
		const short listItem = selectedItems[i];
		if (listItem < 1 || static_cast<UIndex> (listItem) > displayRows.GetSize ())
			continue;

		const DisplayRow& displayRow = displayRows[static_cast<UIndex> (listItem) - 1];
		if (displayRow.elementIndex >= rows.GetSize ())
			continue;

		if (displayRow.kind == RowKind::Group) {
			const CWElementRow& groupRow = rows[displayRow.elementIndex];
			for (UIndex m = 0; m < groupRow.groupMembers.GetSize (); ++m)
				elemGuids.Push (groupRow.groupMembers[m]);
			continue;
		}

		if (displayRow.kind == RowKind::GroupMember) {
			if (displayRow.memberRowIndex < rows.GetSize ())
				elemGuids.Push (rows[displayRow.memberRowIndex].guid);
			continue;
		}

		++skippedPlain;
	}

	if (elemGuids.IsEmpty ()) {
		DG::WarningAlert (FR ("Aucun ensemble ni groupe sélectionné."),
						  FR ("Sélectionnez une ligne « Ensemble » / « Groupe n° … » ou un de ses membres, puis « Dissoudre ensemble / groupe »."),
						  FR ("OK"));
		return;
	}

	// Déduplication (ensemble + membres sélectionnés ensemble).
	for (UIndex i = 0; i < elemGuids.GetSize (); ++i) {
		for (UIndex k = i + 1; k < elemGuids.GetSize (); ) {
			if (elemGuids[k] == elemGuids[i])
				elemGuids.Delete (k);
			else
				++k;
		}
	}

	GS::UniString groupPropError;
	const API_Guid groupIdPropGuid = ArticleManager::EnsureGroupIdProperty (groupPropError);
	if (groupIdPropGuid == APINULLGuid) {
		DG::ErrorAlert (FR ("Dissolution impossible."),
						groupPropError.IsEmpty () ? FR ("Propriété CW_Group_ID indisponible.") : groupPropError,
						FR ("OK"));
		return;
	}

	USize changedCount = 0;
	USize failedCount = 0;
	GS::UniString error;
	const GSErrCode err = ArticleManager::DissolveGroupFromElements (elemGuids, groupIdPropGuid,
																	 changedCount, failedCount, error);
	if (err != NoError) {
		DG::ErrorAlert (FR ("Échec de la dissolution de l'ensemble."), error, FR ("OK"));
		return;
	}

	RefreshData ();

	GS::UniString summary = GS::ToUniString (std::to_wstring (static_cast<int> (changedCount)))
		+ FR (" élément(s) libéré(s) — ils redeviennent facturables individuellement (classe et article conservés).");
	if (failedCount > 0)
		summary += FR ("\n") + GS::ToUniString (std::to_wstring (static_cast<int> (failedCount)))
				 + FR (" échec(s).");
	if (skippedPlain > 0)
		summary += FR ("\n") + GS::ToUniString (std::to_wstring (static_cast<int> (skippedPlain)))
				 + FR (" ligne(s) hors ensemble ignorée(s).");

	DG::InformationAlert (FR ("Ensemble dissous."), summary, FR ("OK"));
}


void CostWavesDialog::ShowSummary ()
{
	if (rows.IsEmpty ()) {
		DG::WarningAlert (FR ("Aucune donnée à résumer."),
						  FR ("Lancez d'abord une lecture avec un système de classification."),
						  FR ("OK"));
		return;
	}

	GS::Array<CWArticleSummary> summary;
	ArticleManager::BuildArticleSummary (rows, articles, summary);

	if (summary.IsEmpty ()) {
		DG::WarningAlert (FR ("Aucun article facturable."),
						  FR ("Aucune ligne ne porte de classe — affectez des articles puis relisez le modèle."),
						  FR ("OK"));
		return;
	}

	inModalDialog = true;
	SummaryDialog summaryDialog (summary);
	summaryDialog.Invoke ();
	inModalDialog = false;
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
	// Rangée ensembles/groupes : le bouton « Dissoudre » absorbe la largeur.
	groupButton.Move (0, dy);
	createGroupButton.Move (0, dy);
	ungroupButton.MoveAndResize (0, dy, dx, 0);
	// Rangée exports : le récapitulatif absorbe la largeur.
	exportJsonButton.Move (0, dy);
	exportCsvButton.Move (0, dy);
	summaryButton.MoveAndResize (0, dy, dx, 0);
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
	} else if (ev.GetSource () == &groupButton) {
		CreateEnsembleOrGroup (false);
	} else if (ev.GetSource () == &createGroupButton) {
		CreateEnsembleOrGroup (true);
	} else if (ev.GetSource () == &ungroupButton) {
		UngroupSelected ();
	} else if (ev.GetSource () == &summaryButton) {
		ShowSummary ();
	} else if (ev.GetSource () == &closeButton) {
		HidePalette ();
	}
}


void CostWavesDialog::PanelCloseRequested (const DG::PanelCloseRequestEvent& /*ev*/, bool* accepted)
{
	// Croix de fermeture de la palette : on la masque (elle se rouvre depuis
	// le menu, l'instance reste en vie).
	HidePalette ();
	*accepted = true;
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
