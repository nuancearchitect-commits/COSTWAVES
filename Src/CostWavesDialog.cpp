#include "CostWavesPrecompiledHeader.hpp"

#include "CostWavesDialog.hpp"

#include "ArticleManager.hpp"
#include "ArticlePickerDialog.hpp"
#include "Exporter.hpp"
#include "MaterialDialog.hpp"
#include "ModelReader.hpp"
#include "SummaryDialog.hpp"
#include "SendDialog.hpp"
#include "CostWavesApi.hpp"

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

// La ligne peut-elle encore recevoir ce fragment (~110 caractères) ?
bool LineFits (const GS::UniString& line, const GS::UniString& chunk)
{
	return line.GetLength () + chunk.GetLength () < 110u;
}

// Le libellé de type d'une ligne correspond-il au filtre 2D choisi ?
// (index popup : 1 = tous, 2 = Lignes, …, 7 = Hachures)
bool Matches2DTypeFilter (const CWElementRow& row, short typeFilter)
{
	if (typeFilter <= 1)
		return true;

	const GS::UniString needle = (typeFilter == 2) ? FR ("Ligne")
		: (typeFilter == 3) ? FR ("Polyligne")
		: (typeFilter == 4) ? FR ("Spline")
		: (typeFilter == 5) ? FR ("Arc")
		: (typeFilter == 6) ? FR ("Cercle")
		: FR ("Hachure");

	return row.typeName.ToUpperCase ().BeginsWith (needle.ToUpperCase ());
}

// Ajoute à outLabels les libellés de quantités pas encore vus (ordre de
// première apparition).
void CollectQuantityLabels (const GS::Array<CWQuantity>& quantities, GS::Array<GS::UniString>& outLabels)
{
	for (UIndex q = 0; q < quantities.GetSize (); ++q) {
		bool known = false;
		for (UIndex l = 0; l < outLabels.GetSize (); ++l) {
			if (outLabels[l] == quantities[q].label) {
				known = true;
				break;
			}
		}
		if (!known)
			outLabels.Push (quantities[q].label);
	}
}

// Réordonne les libellés : types usuels d'abord, puis les autres (ordre de
// découverte). Une colonne par type de quantité, dans un ordre stable.
void SortQuantityLabels (GS::Array<GS::UniString>& labels)
{
	const char* preferred[] = { "Surface", "Volume", "Longueur 3D", "Épaisseur",
								"Surface projetée", "Périmètre", "Longueur" };

	GS::Array<GS::UniString> ordered;
	for (UIndex p = 0; p < sizeof (preferred) / sizeof (preferred[0]); ++p) {
		const GS::UniString wanted (preferred[p], CC_UTF8);
		for (UIndex l = 0; l < labels.GetSize (); ++l) {
			if (labels[l] == wanted)
				ordered.Push (wanted);
		}
	}
	for (UIndex l = 0; l < labels.GetSize (); ++l) {
		bool isPreferred = false;
		for (UIndex p = 0; p < sizeof (preferred) / sizeof (preferred[0]); ++p) {
			if (labels[l] == GS::UniString (preferred[p], CC_UTF8)) {
				isPreferred = true;
				break;
			}
		}
		if (!isPreferred)
			ordered.Push (labels[l]);
	}
	labels = ordered;
}

// Valeur de la quantité portant exactement ce libellé (0 si absente).
double QuantityValueForLabel (const GS::Array<CWQuantity>& quantities, const GS::UniString& label)
{
	for (UIndex q = 0; q < quantities.GetSize (); ++q) {
		if (quantities[q].label == label)
			return quantities[q].value;
	}
	return 0.0;
}

// Cellule « quantité » : valeur formatée pour ce libellé (vide si absente).
GS::UniString QuantitiesSummaryHas (const GS::Array<CWQuantity>& quantities, const GS::UniString& label)
{
	for (UIndex q = 0; q < quantities.GetSize (); ++q) {
		if (quantities[q].label == label)
			return FormatValue (quantities[q].value);
	}
	return GS::UniString ();
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

// a < b selon la colonne de tri ? (1..4 = colonnes de base, 5 = facturé,
// >5 = colonne de quantité identifiée par son libellé.)
// Les données de facturation (articles) et les libellés de colonnes sont
// fournis par l'appelant (SortRows).
bool RowLess (const CWElementRow& a, const CWElementRow& b, short column,
			  const GS::Array<CWArticle>& articles, const GS::Array<CWElementRow>& allRows,
			  const GS::Array<GS::UniString>& quantityLabels)
{
	switch (column) {
		case 1: {
			// Source (BIM avant 2D), puis type.
			if (a.is2D != b.is2D)
				return a.is2D < b.is2D;
			return a.typeName.ToUpperCase () < b.typeName.ToUpperCase ();
		}
		case 2:
			return a.typeName.ToUpperCase () < b.typeName.ToUpperCase ();
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
		case 6: {
			const CWArticle* articleA = FindArticleById (articles, a.classItemId);
			const CWArticle* articleB = FindArticleById (articles, b.classItemId);
			double valueA = 0.0;
			double valueB = 0.0;
			if (articleA != nullptr) {
				GS::UniString unit;
				valueA = ArticleManager::ComputeBilledQuantity (*articleA, a, allRows, unit);
			}
			if (articleB != nullptr) {
				GS::UniString unit;
				valueB = ArticleManager::ComputeBilledQuantity (*articleB, b, allRows, unit);
			}
			return valueA < valueB;
		}
		default: {
			const UIndex labelIndex = static_cast<UIndex> (column) - 7;
			if (labelIndex >= quantityLabels.GetSize ())
				return false;
			return QuantityValueForLabel (a.quantities, quantityLabels[labelIndex])
				 < QuantityValueForLabel (b.quantities, quantityLabels[labelIndex]);
		}
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
		sendButton (GetReference (), SendButtonId),
		sourceLabel (GetReference (), SourceLabelId),
		modePopup (GetReference (), ModePopupId),
		draw2DCheck (GetReference (), Draw2DCheckId),
		draw2DTypePopup (GetReference (), Draw2DTypePopupId),
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
	sendButton.Attach (*this);	// ButtonItemObserver
	modePopup.Attach (*this);		// PopUpObserver
	draw2DTypePopup.Attach (*this);	// PopUpObserver
	draw2DCheck.Attach (*this);	// CheckItemObserver

	isFilling = true;

	// Mode de quantification BIM (§3) : l'élément OU ses composants,
	// jamais les deux simultanément.
	modePopup.AppendItem ();
	modePopup.SetItemText (1, FR ("Élément"));
	modePopup.AppendItem ();
	modePopup.SetItemText (2, FR ("Composants (skins)"));
	modePopup.SelectItem (1);

	// Filtre des dessins 2D par type (§9/§10) : tous par défaut.
	draw2DTypePopup.AppendItem ();
	draw2DTypePopup.SetItemText (1, FR ("Tous types 2D"));
	draw2DTypePopup.AppendItem ();
	draw2DTypePopup.SetItemText (2, FR ("Lignes"));
	draw2DTypePopup.AppendItem ();
	draw2DTypePopup.SetItemText (3, FR ("Polylignes"));
	draw2DTypePopup.AppendItem ();
	draw2DTypePopup.SetItemText (4, FR ("Splines"));
	draw2DTypePopup.AppendItem ();
	draw2DTypePopup.SetItemText (5, FR ("Arcs"));
	draw2DTypePopup.AppendItem ();
	draw2DTypePopup.SetItemText (6, FR ("Cercles"));
	draw2DTypePopup.AppendItem ();
	draw2DTypePopup.SetItemText (7, FR ("Hachures"));
	draw2DTypePopup.SelectItem (1);
	draw2DTypePopup.Enable (false);

	isFilling = false;
	searchEdit.Attach (*this);		// SearchEditObserver
	exportJsonButton.Attach (*this);
	exportCsvButton.Attach (*this);
	closeButton.Attach (*this);

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
	ModelReader::Scan (selectedSystem, elemIdPropGuid, groupPropGuid, include2D,
					  filter, rows, report);
	FillTable ();

	isFilling = false;

	UpdateStatus ();
	UpdateDetails (1);
}




void CostWavesDialog::SortRows ()
{
	const short columnCount = static_cast<short> (6 + quantityColumnLabels.GetSize ());
	if (sortColumn < 1 || sortColumn > columnCount || rows.GetSize () < 2)
		return;

	// Trier des indices puis réassembler (les composants restent avec leur
	// élément : on trie "rows" entières).
	std::vector<UIndex> order;
	order.reserve (rows.GetSize ());
	for (UIndex i = 0; i < rows.GetSize (); ++i)
		order.push_back (i);

	const short column = sortColumn;
	const bool ascending = sortAscending;
	const GS::Array<CWArticle>& catalog = articles;
	const GS::Array<CWElementRow>& allRows = rows;
	const GS::Array<GS::UniString>& labels = quantityColumnLabels;
	std::stable_sort (order.begin (), order.end (),
					  [&] (UIndex a, UIndex b) {
						  return ascending ? RowLess (rows[a], rows[b], column, catalog, allRows, labels)
										   : RowLess (rows[b], rows[a], column, catalog, allRows, labels);
					  });

	GS::Array<CWElementRow> sorted;
	for (UIndex k = 0; k < order.size (); ++k)
		sorted.Push (rows[order[k]]);
	rows = sorted;
}


void CostWavesDialog::FillTable ()
{
	table.SetHeaderSynchronState (true);

	// --- Règles d'affichage ---------------------------------------------------
	// Seules les lignes PORTANT UNE CLASSE sont affichées :
	//  - un élément sans classe n'est pas affiché (seuls ses skins classés
	//    le sont) ;
	//  - un skin est affiché s'il a une classe (celle de son matériau) ;
	//  - les composants « properties » (sans quantités) ne sont pas affichés.

	// --- Colonnes : une par type de quantité, sur les lignes affichées ------
	quantityColumnLabels.Clear ();
	for (UIndex e = 0; e < rows.GetSize (); ++e) {
		const CWElementRow& element = rows[e];
		if (element.consumed)
			continue;
		if (!element.classItemId.IsEmpty ())
			CollectQuantityLabels (element.quantities, quantityColumnLabels);
		for (UIndex c = 0; c < element.components.GetSize (); ++c) {
			const CWComponentRow& component = element.components[c];
			if (component.kind == RowKind::Skin && !component.classItemId.IsEmpty ())
				CollectQuantityLabels (component.quantities, quantityColumnLabels);
		}
	}
	SortQuantityLabels (quantityColumnLabels);

	// Tri courant avant affichage (utilise les libellés frais).
	SortRows ();

	const short baseColumnCount = 6;	// Source, Type, ID, Étage/Calque, Classe, Facturé
	const short columnCount = static_cast<short> (baseColumnCount + quantityColumnLabels.GetSize ());

	table.SetHeaderItemCount (columnCount);
	// Le GRC ne définit pas les colonnes : créer les champs de tabulation
	// (sans cela, seule la première colonne s'affiche).
	table.SetTabFieldCount (columnCount);
	table.SetHeaderItemText (1, FR ("Source"));
	table.SetHeaderItemText (2, FR ("Type"));
	table.SetHeaderItemText (3, FR ("ID élément"));
	table.SetHeaderItemText (4, FR ("Étage / Calque"));
	table.SetHeaderItemText (5, FR ("Classe"));
	table.SetHeaderItemText (6, FR ("Facturé"));
	for (UIndex q = 0; q < quantityColumnLabels.GetSize (); ++q)
		table.SetHeaderItemText (static_cast<short> (baseColumnCount + 1 + q), quantityColumnLabels[q]);

	while (table.GetItemCount () > 0)
		table.DeleteItem (1);

	displayRows.Clear ();

	// Filtre de recherche : testé sur les colonnes de chaque ligne affichée.
	const GS::UniString needle = searchFilter.ToUpperCase ();
	auto lineMatches = [&] (const GS::UniString& typeText, const GS::UniString& idText,
							const GS::UniString& floorText, const GS::UniString& classText) -> bool {
		if (searchFilter.IsEmpty ())
			return true;
		return typeText.ToUpperCase ().Contains (needle)
			|| idText.ToUpperCase ().Contains (needle)
			|| floorText.ToUpperCase ().Contains (needle)
			|| classText.ToUpperCase ().Contains (needle);
	};

	// Largeur automatique : maximum des contenus de chaque colonne.
	std::vector<short> columnMax(static_cast<size_t> (columnCount) + 1, 0);
	auto trackWidth = [&] (short column, const GS::UniString& text) {
		const short estimated = static_cast<short> (text.GetLength () * 7 + 18);
		if (column >= 1 && column <= columnCount && estimated > columnMax[column])
			columnMax[column] = estimated;
	};

	// Remplit les cellules « quantité » d'une ligne (une colonne par type :
	// la valeur si présente, vide sinon).
	auto fillQuantityCells = [&] (short itemIndex, const GS::Array<CWQuantity>& quantities) {
		for (UIndex q = 0; q < quantityColumnLabels.GetSize (); ++q) {
			const short column = static_cast<short> (baseColumnCount + 1 + q);
			const GS::UniString cellText = QuantitiesSummaryHas (quantities, quantityColumnLabels[q]);
			table.SetTabItemText (itemIndex, column, cellText);
			trackWidth (column, cellText);
		}
	};

	auto appendRow = [&] (const GS::UniString& sourceText, const GS::UniString& typeText,
						  const GS::UniString& idText, const GS::UniString& floorText,
						  const GS::UniString& classText, const GS::UniString& billedText,
						  const GS::Array<CWQuantity>& quantities,
						  RowKind displayKind, UIndex elementIndex, UIndex componentIndex, UIndex memberIndex) {
		table.AppendItem ();
		const short itemIndex = table.GetItemCount ();
		table.SetTabItemText (itemIndex, 1, sourceText);
		table.SetTabItemText (itemIndex, 2, typeText);
		table.SetTabItemText (itemIndex, 3, idText);
		table.SetTabItemText (itemIndex, 4, floorText);
		table.SetTabItemText (itemIndex, 5, classText);
		table.SetTabItemText (itemIndex, 6, billedText);
		trackWidth (1, sourceText);
		trackWidth (2, typeText);
		trackWidth (3, idText);
		trackWidth (4, floorText);
		trackWidth (5, classText);
		trackWidth (6, billedText);
		fillQuantityCells (itemIndex, quantities);

		DisplayRow displayRow;
		displayRow.kind = displayKind;
		displayRow.elementIndex = elementIndex;
		displayRow.componentIndex = componentIndex;
		displayRow.memberRowIndex = memberIndex;
		displayRows.Push (displayRow);
	};

	for (UIndex e = 0; e < rows.GetSize (); ++e) {
		const CWElementRow& element = rows[e];

		// Les membres d'un ensemble ne sont pas affichés au premier niveau :
		// ils apparaissent en sous-lignes de leur ligne « Ensemble ».
		if (element.consumed)
			continue;

		const bool elementHasClass = !element.classItemId.IsEmpty ();

		// Filtre des dessins 2D par type (§9/§10).
		if (element.is2D && !Matches2DTypeFilter (element, draw2DTypePopup.GetSelectedItem ()))
			continue;

		// Mode de quantification BIM (§3) : en mode Composants, les lignes
		// d'éléments BIM n'apparaissent pas (seuls leurs skins classés) ;
		// les dessins 2D (catégorie indépendante) restent affichés.
		if (quantMode == CWQuantMode::Component && !element.is2D && !element.isGroupRow)
			continue;

		const GS::UniString floorText = element.storyName.IsEmpty ()
			? GS::ToUniString (std::to_wstring (static_cast<int> (element.floorInd)))
			: GS::ToUniString (std::to_wstring (static_cast<int> (element.floorInd))) + " - " + element.storyName;
		const GS::UniString layerText = element.is2D && !element.layerName.IsEmpty ()
			? element.layerName
			: floorText;
		const GS::UniString classText = element.classItemId.IsEmpty ()
			? element.classItemName
			: element.classItemId + " - " + element.classItemName;
		const GS::UniString sourceText = element.is2D ? FR ("2D") : FR ("BIM");

		// Ligne de l'élément (seulement s'il porte une classe).
		if (elementHasClass) {
			const GS::UniString typeText = element.isGroupRow
				? (element.isNumberedGroup
					? FR ("Groupe n° ") + GS::ToUniString (std::to_wstring (element.groupNumber))
					: FR ("Ensemble"))
				: FR ("Élément");

			if (lineMatches (typeText, element.elementId, layerText, classText)) {
				appendRow (sourceText, typeText, element.elementId, layerText, classText,
						   BilledText (element), element.quantities,
						   element.isGroupRow ? RowKind::Group : RowKind::Element, e, 0, 0);

				// Membres de l'ensemble/groupe (sous-lignes « consommées »).
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
					const GS::UniString memberClass = memberRow->classItemId.IsEmpty ()
						? GS::UniString ()
						: memberRow->classItemId + " - " + memberRow->classItemName;
					const GS::UniString memberType = FR ("Membre (consommé)");

					if (lineMatches (memberType, memberRow->elementId, memberFloor, memberClass))
						appendRow (sourceText, memberType, memberRow->elementId, memberFloor, memberClass,
								   FR ("—"), memberRow->quantities,
								   RowKind::GroupMember, e, 0, memberIndex);
				}
			}
		}

		// Skins classés (mode Composants) : affichés avec la classe de leur
		// MATÉRIAU, même si l'élément parent n'a pas de classe (mur sans
		// classe à couches classées → seules les couches classées apparaissent).
		// En mode Élément (§3), les skins ne sont ni affichés ni facturés.
		if (quantMode == CWQuantMode::Component) {
			for (UIndex c = 0; c < element.components.GetSize (); ++c) {
				const CWComponentRow& component = element.components[c];
				if (component.kind != RowKind::Skin || component.classItemId.IsEmpty ())
					continue;

				GS::UniString skinType = FR ("Skin — ") + component.label;
				if (component.coreSkin)
					skinType += FR (" (cœur)");
				const GS::UniString skinClass = component.classItemId + " - " + component.classItemName;

				if (lineMatches (skinType, element.elementId, layerText, skinClass))
					appendRow (FR ("Composant"), skinType, element.elementId, layerText, skinClass,
							   SkinBilledText (component), component.quantities,
							   RowKind::Skin, e, c, 0);
			}
		}
	}

	// --- Largeurs automatiques + scroll horizontal ---------------------------
	// Largeur estimée par colonne : maximum des contenus ET des en-têtes
	// (en unités de dialogue) ; le total peut dépasser la largeur du
	// contrôle → le GRC active le scroll horizontal (HVScroll).
	trackWidth (1, FR ("Source"));
	trackWidth (2, FR ("Type"));
	trackWidth (3, FR ("ID élément"));
	trackWidth (4, FR ("Étage / Calque"));
	trackWidth (5, FR ("Classe"));
	trackWidth (6, FR ("Facturé"));
	for (UIndex q = 0; q < quantityColumnLabels.GetSize (); ++q)
		trackWidth (static_cast<short> (baseColumnCount + 1 + q), quantityColumnLabels[q]);
	for (short i = 1; i <= columnCount; ++i) {
		if (columnMax[i] < 55)
			columnMax[i] = 55;
		if (columnMax[i] > 340)
			columnMax[i] = 340;
	}

	short position = 0;
	for (short i = 1; i <= columnCount; ++i) {
		const short width = columnMax[i];
		table.SetHeaderItemSize (i, width);
		table.SetHeaderItemSizeableFlag (i, true);
		table.SetTabFieldProperties (i, position, static_cast<short> (position + width),
								 DG::ListBox::Left, DG::ListBox::MiddleTruncate, i > 1);
		position = static_cast<short> (position + width);
	}

	if (table.GetItemCount () > 0)
		table.SelectItem (1);

	// Flèche de tri sur la colonne active.
	for (short c = 1; c <= columnCount; ++c)
		table.SetHeaderItemArrowType (c, DG::ListBox::NoArrow);
	if (sortColumn >= 1 && sortColumn <= columnCount)
		table.SetHeaderItemArrowType (sortColumn, sortAscending ? DG::ListBox::Up : DG::ListBox::Down);
}


// Texte de la colonne « Facturé » pour une ligne élément/ensemble/groupe.
GS::UniString CostWavesDialog::BilledText (const CWElementRow& row) const
{
	if (row.consumed)
		return FR ("—");

	if (row.classItemId.IsEmpty ())
		return GS::UniString ();

	const CWArticle* article = FindArticleById (articles, row.classItemId);
	if (article == nullptr)
		return FR ("Article inconnu");

	GS::UniString unit;
	const double quantity = ArticleManager::ComputeBilledQuantity (*article, row, rows, unit);
	GS::UniString text = FormatValue (quantity) + " " + unit;
	if (row.isNumberedGroup)
		text += FR (" (par groupe)");
	return text;
}


// Texte de la colonne « Facturé » pour un skin classé (article du matériau).
GS::UniString CostWavesDialog::SkinBilledText (const CWComponentRow& skin) const
{
	const CWArticle* article = FindArticleById (articles, skin.classItemId);
	if (article == nullptr)
		return FR ("Article inconnu");

	GS::UniString unit;
	double quantity = 0.0;
	if (ArticleManager::IsEnsUnit (article->unit)) {
		quantity = 1.0;
		unit = FR ("ENS");
	} else {
		quantity = ArticleManager::QuantityForUnit (skin.quantities, article->unit);
		unit = article->unit;
	}
	return FormatValue (quantity) + " " + unit;
}


void CostWavesDialog::UpdateStatus ()
{
	GS::UniString status = GS::ToUniString (std::to_wstring (static_cast<int> (report.classifiedElements)))
		+ FR (" éléments classés · ")
		+ GS::ToUniString (std::to_wstring (static_cast<int> (report.classified2D)))
		+ FR (" dessins 2D · ")
		+ GS::ToUniString (std::to_wstring (static_cast<int> (report.classifiedSkins)))
		+ FR (" skins classés · ")
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

	// Liste hiérarchique : chaque article est indenté selon sa profondeur
	// dans la classification (comme l'arbre Options > Classifications).
	for (UIndex i = 0; i < articles.GetSize (); ++i) {
		GS::UniString label;
		for (short d = 0; d < articles[i].depth; ++d)
			label += FR ("    ");
		label += articles[i].id + US (" — ") + articles[i].name;
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
		// L'article n'existe dans aucune classification (articles importés
		// en JSON, par ex.) : créer le système « CostWaves » et l'item de
		// cet article, puis re-résoudre.
		GS::Array<CWArticle> singleArticle;
		singleArticle.Push (article);
		API_Guid ensuredSystem = APINULLGuid;
		USize createdItems = 0;
		GS::UniString ensureError;
		if (ArticleManager::EnsureCostWavesClassification (singleArticle, ensuredSystem, createdItems, ensureError) != NoError
			|| !ResolveArticleTarget (article.id, targetSystem, itemGuid)) {
			DG::WarningAlert (FR ("Article introuvable dans les classifications."),
							  ensureError.IsEmpty ()
								  ? FR ("Cliquez d'abord sur « Créer la classification » pour générer le système CostWaves.")
								  : ensureError,
							  FR ("OK"));
			return;
		}
		LoadSystems ();
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
		SetDetailLine (2, FR ("Source : ") + (element.is2D ? FR ("dessin 2D") : FR ("BIM"))
			+ FR (" · ID : ") + (element.elementId.IsEmpty () ? FR ("(vide)") : element.elementId)
			+ FR (" · Étage : ") + floorText
			+ (element.layerName.IsEmpty () ? GS::UniString () : FR (" · Calque : ") + element.layerName));
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

		// Classe du matériau (phase 5) : le skin est facturé sur cet article.
		if (!component.classItemId.IsEmpty ()) {
			SetDetailLine (lineIndex, FR ("Classe du matériau : ")
				+ component.classItemId + " (" + component.classItemName + ")");
			++lineIndex;
		}

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

	// Recherche dans TOUS les systèmes du projet (l'article peut venir d'un
	// système autre que « CostWaves » et que le système courant).
	{
		GS::Array<API_ClassificationSystem> allSystems;
		if (ACAPI_Classification_GetClassificationSystems (allSystems) == NoError) {
			for (UIndex i = 0; i < allSystems.GetSize (); ++i) {
				if (allSystems[i].guid == costWavesSystem || allSystems[i].guid == selectedSystem)
					continue;
				const API_Guid guid = ArticleManager::FindItemGuid (allSystems[i].guid, articleId);
				if (guid != APINULLGuid) {
					outSystemGuid = allSystems[i].guid;
					outItemGuid = guid;
					return true;
				}
			}
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
	ArticleManager::BuildArticleSummary (rows, articles, quantMode, summary);

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


void CostWavesDialog::SendToCostWaves ()
{
	// Données fraîches si le tableau est vide (envoi depuis le menu).
	if (rows.IsEmpty ())
		RefreshData ();

	if (rows.IsEmpty ()) {
		DG::WarningAlert (FR ("Aucun élément classé à envoyer."),
						  FR ("Lisez d'abord le modèle avec un système de classification (la palette liste les éléments classés et les skins classés)."),
						  FR ("OK"));
		return;
	}

	// Récapitulatif facturé + comptage du contenu de l'envoi.
	GS::Array<CWArticleSummary> summary;
	ArticleManager::BuildArticleSummary (rows, articles, quantMode, summary);

	USize classifiedElements = 0;
	USize classifiedSkins = 0;
	for (UIndex i = 0; i < rows.GetSize (); ++i) {
		if (!rows[i].consumed && !rows[i].classItemId.IsEmpty ())
			++classifiedElements;
		for (UIndex c = 0; c < rows[i].components.GetSize (); ++c) {
			if (rows[i].components[c].kind == RowKind::Skin
				&& !rows[i].components[c].classItemId.IsEmpty ())
				++classifiedSkins;
		}
	}

	// Réglages (URL, clé API, traitement des articles inconnus — spéc. §6).
	CWApiSettings settings;
	GS::UniString settingsError;
	CostWavesApi::LoadSettings (settings, settingsError);

	inModalDialog = true;
	SendDialog sendDialog (settings, classifiedElements, classifiedSkins, articles.GetSize ());
	sendDialog.Invoke ();
	inModalDialog = false;

	if (!sendDialog.IsAccepted ())
		return;

	settings = sendDialog.GetSettings ();
	CostWavesApi::SaveSettings (settings, settingsError);	// best effort

	// Projet : identifiant = nom du PLN (sans extension).
	GS::UniString folder;
	GS::UniString projectName;
	Exporter::ResolveProjectLocation (folder, projectName);

	const GS::UniString payload = CostWavesApi::BuildPayload (projectName, projectName,
															  rows, articles, quantMode,
															  summary, settings);

	DG::InformationAlert (FR ("Envoi en cours…"),
						  FR ("L'envoi vers CostWaves est bloquant pendant quelques secondes ; le résultat s'affichera ensuite."),
						  FR ("OK"));

	const CWApiSendResult result = CostWavesApi::Send (settings, payload);

	if (result.err != NoError) {
		GS::UniString detail = result.error.IsEmpty ()
			? FR ("Erreur inconnue lors de l'envoi.")
			: result.error;
		if (result.httpStatus > 0)
			detail += FR (" (HTTP ") + GS::ToUniString (std::to_wstring (result.httpStatus)) + FR (")");
		if (!result.message.IsEmpty ())
			detail += FR ("\n") + result.message;
		DG::ErrorAlert (FR ("Échec de l'envoi vers CostWaves."), detail, FR ("OK"));
		return;
	}

	// Résultat : lignes créées/mises à jour + articles signalés inconnus.
	GS::UniString detail = FR ("Projet « ") + projectName + FR (" » envoyé.");
	if (result.httpStatus > 0)
		detail += FR (" (HTTP ") + GS::ToUniString (std::to_wstring (result.httpStatus)) + FR (")");
	detail += FR ("\n");
	detail += GS::ToUniString (std::to_wstring (static_cast<int> (result.createdLines)))
		+ FR (" ligne(s) créée(s), ")
		+ GS::ToUniString (std::to_wstring (static_cast<int> (result.updatedLines)))
		+ FR (" ligne(s) mise(s) à jour.");
	if (!result.unknownArticles.IsEmpty ()) {
		detail += FR ("\nArticles signalés inconnus par le serveur : ");
		for (UIndex i = 0; i < result.unknownArticles.GetSize (); ++i) {
			if (i > 0)
				detail += FR (", ");
			detail += result.unknownArticles[i];
		}
		detail += FR ("\nMode appliqué : ")
			+ (settings.IsUnknownProjectOnly () ? FR ("ajout au projet uniquement")
			  : settings.IsUnknownBaseAndProject () ? FR ("ajout à la base + au projet")
			  : FR ("ignorés"));
	}
	if (!result.message.IsEmpty ())
		detail += FR ("\n") + result.message;

	DG::InformationAlert (FR ("Envoi vers CostWaves effectué."), detail, FR ("OK"));
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
		? Exporter::ExportJSON (systemName, quantMode, rows, report, articles, path, error)
		: Exporter::ExportCSV (systemName, quantMode, rows, report, articles, path, error);

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
	// Rangée envoi (bas-gauche) : suit le bas de la palette.
	sendButton.Move (0, dy);

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
	} else if (ev.GetSource () == &sendButton) {
		SendToCostWaves ();
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
	} else if (ev.GetSource () == &modePopup) {
		// Mode de quantification BIM (§3) : Élément ou Composants — le
		// tableau et la facturation sont recalculés (pas de relecture).
		quantMode = (modePopup.GetSelectedItem () == 2) ? CWQuantMode::Component
														: CWQuantMode::Element;
		FillTable ();
		UpdateStatus ();
	} else if (ev.GetSource () == &draw2DTypePopup) {
		// Filtre par type de dessin 2D (§9/§10) : affichage seul.
		FillTable ();
	}
}


void CostWavesDialog::CheckItemChanged (const DG::CheckItemChangeEvent& ev)
{
	if (isFilling)
		return;

	if (ev.GetSource () == &draw2DCheck) {
		// Inclure ou non les dessins 2D dans le scan (§2/§4) — relecture.
		include2D = draw2DCheck.IsChecked ();
		draw2DTypePopup.Enable (include2D);
		RefreshData ();
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
	const short columnCount = static_cast<short> (6 + quantityColumnLabels.GetSize ());
	if (column < 1 || column > columnCount)
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
