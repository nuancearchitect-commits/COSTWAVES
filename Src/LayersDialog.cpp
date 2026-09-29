#include "CostWavesPrecompiledHeader.hpp"

#include "UniStringWStringConversion.hpp"

#include "LayersDialog.hpp"

#include "ArticleManager.hpp"
#include "RuleLibrary.hpp"

namespace CostWaves {

namespace {

GS::UniString FR (const char* utf8Text)
{
	return GS::UniString (utf8Text, CC_UTF8);
}

} // namespace


LayersDialog::LayersDialog (const GS::UniString& inStructureName,
							const GS::Array<GS::UniString>& materialNames,
							const GS::Array<CWArticle>& inArticles,
							GS::Array<CWMapRule>& ioRules)
	:	DG::ModalDialog (ACAPI_GetOwnResModule (), ID_ADDON_DLG_LAYERS, ACAPI_GetOwnResModule ()),
		infoText (GetReference (), InfoTextId),
		table (GetReference (), TableId),
		articleLabel (GetReference (), ArticleLabelId),
		articlePopup (GetReference (), ArticlePopupId),
		assignButton (GetReference (), AssignButtonId),
		statusText (GetReference (), StatusTextId),
		closeButton (GetReference (), CloseButtonId),
		structureName (inStructureName),
		materials (materialNames),
		articles (inArticles),
		rules (ioRules)
{
	infoText.SetText (FR ("Couches de « ") + structureName
					  + FR (" » — donnez un article à chaque matériau : le métré ")
					  + FR ("se fera sur les quantités des matériaux."));

	// Popup des articles de la base.
	for (UIndex a = 0; a < articles.GetSize (); ++a) {
		GS::UniString label = articles[a].id + FR (" — ") + articles[a].name;
		if (!articles[a].unit.IsEmpty ())
			label += FR (" (") + articles[a].unit + FR (")");
		articlePopup.AppendItem ();
		articlePopup.SetItemText (articlePopup.GetItemCount (), label);
	}
	if (articlePopup.GetItemCount () > 0)
		articlePopup.SelectItem (1);

	FillTable ();

	assignButton.Attach (*this);
	closeButton.Attach (*this);
	table.Attach (*this);
}


void LayersDialog::FillTable ()
{
	isFilling = true;

	const short columnCount = 2;
	table.SetHeaderItemCount (columnCount);
	table.SetTabFieldCount (columnCount);
	table.SetHeaderItemText (1, FR ("Matériau"));
	table.SetHeaderItemText (2, FR ("Article"));

	const short widths[2] = { 260, 280 };
	short position = 0;
	for (short i = 1; i <= columnCount; ++i) {
		table.SetHeaderItemSize (i, widths[i - 1]);
		table.SetHeaderItemSizeableFlag (i, true);
		table.SetTabFieldProperties (i, position, static_cast<short> (position + widths[i - 1]),
									 DG::ListBox::Left, DG::ListBox::MiddleTruncate, i > 1);
		position = static_cast<short> (position + widths[i - 1]);
	}

	while (table.GetItemCount () > 0)
		table.DeleteItem (1);

	// Sélection par défaut : le premier matériau sans règle.
	short firstUnassigned = 0;
	for (UIndex m = 0; m < materials.GetSize (); ++m) {
		table.AppendItem ();
		const short item = table.GetItemCount ();
		table.SetTabItemText (item, 1, materials[m]);

		const CWMapRule* rule = RuleLibrary::FindRule (rules, CWStructureType::BuildingMaterial, materials[m]);
		if (rule != nullptr && !rule->ignored && !rule->articleId.IsEmpty ()) {
			const CWArticle* article = ArticleManager::FindArticle (articles, rule->articleId);
			table.SetTabItemText (item, 2, article != nullptr
				? rule->articleId + FR (" — ") + article->name
				: rule->articleId);
		} else {
			table.SetTabItemText (item, 2, FR ("⚠ à définir"));
			if (firstUnassigned == 0)
				firstUnassigned = item;
		}
	}

	if (table.GetItemCount () > 0)
		table.SelectItem (firstUnassigned > 0 ? firstUnassigned : 1);

	UpdateStatus ();
	isFilling = false;
}


void LayersDialog::ListBoxSelectionChanged (const DG::ListBoxSelectionEvent& ev)
{
	(void) ev;
}


void LayersDialog::AssignArticleToSelected ()
{
	const short selection = table.GetSelectedItem ();
	if (selection < 1 || static_cast<UIndex> (selection) > materials.GetSize ())
		return;

	const short articleSelection = articlePopup.GetSelectedItem ();
	if (articleSelection < 1 || static_cast<UIndex> (articleSelection) > articles.GetSize ()) {
		DG::WarningAlert (FR ("Aucun article"),
						  FR ("Aucun article dans la base — créez-en un ou importez la base."),
						  FR ("OK"));
		return;
	}

	const GS::UniString materialName = materials[static_cast<UIndex> (selection) - 1];
	const CWArticle& article = articles[static_cast<UIndex> (articleSelection) - 1];

	// Ajout ou mise à jour de la règle matériau (clé = type + nom).
	bool replaced = false;
	for (UIndex r = 0; r < rules.GetSize (); ++r) {
		if (rules[r].structureType == CWStructureType::BuildingMaterial
			&& rules[r].structureName == materialName) {
			rules[r].articleId = article.id;
			rules[r].mode = CWQuantMode::Component;
			rules[r].ignored = false;
			replaced = true;
			break;
		}
	}
	if (!replaced) {
		CWMapRule rule;
		rule.structureType = CWStructureType::BuildingMaterial;
		rule.structureName = materialName;
		rule.articleId = article.id;
		rule.mode = CWQuantMode::Component;
		rules.Push (rule);
	}

	FillTable ();

	// Sélectionner le matériau suivant sans article, s'il en reste.
	const short count = table.GetItemCount ();
	for (short item = 1; item <= count; ++item) {
		// La colonne 2 des matériaux sans article contient « ⚠ ».
		if (table.GetTabItemText (item, 2).Contains (FR ("⚠"))) {
			table.SelectItem (item);
			break;
		}
	}
}


void LayersDialog::UpdateStatus ()
{
	USize unassigned = 0;
	for (UIndex m = 0; m < materials.GetSize (); ++m) {
		const CWMapRule* rule = RuleLibrary::FindRule (rules, CWStructureType::BuildingMaterial, materials[m]);
		if (rule == nullptr || rule->ignored || rule->articleId.IsEmpty ())
			++unassigned;
	}

	statusText.SetText (unassigned == 0
		? FR ("Toutes les couches ont un article.")
		: GS::ToUniString (std::to_wstring (static_cast<int> (unassigned)))
		  + FR (" matériau(x) sans article — ces couches resteront ⚠ dans le projet."));
}


void LayersDialog::ButtonClicked (const DG::ButtonClickEvent& ev)
{
	if (ev.GetSource () == &assignButton) {
		AssignArticleToSelected ();
	} else if (ev.GetSource () == &closeButton) {
		PostCloseRequest (DG::ModalDialog::Accept);
	}
}

} // namespace CostWaves
