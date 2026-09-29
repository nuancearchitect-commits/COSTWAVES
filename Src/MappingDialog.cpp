#include "CostWavesPrecompiledHeader.hpp"

#include "MappingDialog.hpp"

#include "ArticleManager.hpp"
#include "ArticlePickerDialog.hpp"
#include "ModelReader.hpp"
#include "RuleLibrary.hpp"

#include "UniStringWStringConversion.hpp"

namespace CostWaves {

namespace {

GS::UniString FR (const char* utf8Text)
{
	return GS::UniString (utf8Text, CC_UTF8);
}

} // namespace


MappingDialog::MappingDialog ()
	:	DG::ModalDialog (ACAPI_GetOwnResModule (), ID_ADDON_DLG_MAPPING, ACAPI_GetOwnResModule ()),
		infoText (GetReference (), InfoTextId),
		systemLabel (GetReference (), SystemLabelId),
		systemPopup (GetReference (), SystemPopupId),
		list (GetReference (), ListId),
		statusText (GetReference (), StatusTextId),
		saveButton (GetReference (), SaveButtonId),
		closeButton (GetReference (), CloseButtonId)
{
	infoText.SetText (FR ("Cliquez sur un matériau pour lui choisir sa classe (article). ")
					  + FR ("Un matériau sans classe est ignoré du métré."));

	// Filtre système de classification (les classes = articles proposés).
	systems = ModelReader::GetClassificationSystems ();
	if (systems.IsEmpty ()) {
		systemPopup.AppendItem ();
		systemPopup.SetItemText (1, FR ("(aucun système)"));
	} else {
		for (UIndex s = 0; s < systems.GetSize (); ++s) {
			systemPopup.AppendItem ();
			systemPopup.SetItemText (systemPopup.GetItemCount (), systems[s].name);
		}
	}
	systemPopup.SelectItem (1);

	// Bibliothèque existante (Documents/CostWaves-regles.json).
	GS::UniString rulesError;
	RuleLibrary::LoadRules (rules, rulesError);		// absente = bibliothèque vide

	RefreshArticles ();
	RefreshMaterials ();

	saveButton.Attach (*this);
	closeButton.Attach (*this);
	systemPopup.Attach (*this);
	list.Attach (*this);
}


void MappingDialog::RefreshMaterials ()
{
	isFilling = true;
	materials.Clear ();
	RuleLibrary::CollectAvailableStructures (CWStructureType::BuildingMaterial, materials);
	selectedMaterial = 0;
	isFilling = false;

	FillList ();
}


void MappingDialog::RefreshArticles ()
{
	articles.Clear ();
	const short systemSelection = systemPopup.GetSelectedItem ();
	if (systemSelection >= 1 && static_cast<UIndex> (systemSelection) <= systems.GetSize ())
		ArticleManager::CollectFromClassification (systems[static_cast<UIndex> (systemSelection) - 1].guid,
												   articles);
}


void MappingDialog::FillList ()
{
	isFilling = true;

	const short columnCount = 2;
	list.SetHeaderItemCount (columnCount);
	list.SetTabFieldCount (columnCount);
	list.SetHeaderItemText (1, FR ("Matériau"));
	list.SetHeaderItemText (2, FR ("Classe (article)"));

	const short widths[2] = { 280, 360 };
	short position = 0;
	for (short i = 1; i <= columnCount; ++i) {
		list.SetHeaderItemSize (i, widths[i - 1]);
		list.SetHeaderItemSizeableFlag (i, true);
		list.SetTabFieldProperties (i, position, static_cast<short> (position + widths[i - 1]),
									 DG::ListBox::Left, DG::ListBox::MiddleTruncate, i > 1);
		position = static_cast<short> (position + widths[i - 1]);
	}

	while (list.GetItemCount () > 0)
		list.DeleteItem (1);

	USize withArticle = 0;
	for (UIndex m = 0; m < materials.GetSize (); ++m) {
		list.AppendItem ();
		const short item = list.GetItemCount ();
		list.SetTabItemText (item, 1, materials[m]);

		const CWMapRule* rule = RuleLibrary::FindRule (rules, CWStructureType::BuildingMaterial, materials[m]);
		if (rule != nullptr && !rule->articleId.IsEmpty ()) {
			++withArticle;
			const CWArticle* article = ArticleManager::FindArticle (articles, rule->articleId);
			list.SetTabItemText (item, 2, article != nullptr
				? rule->articleId + FR (" — ") + article->name
				: rule->articleId);
		} else {
			list.SetTabItemText (item, 2, FR ("—"));
		}
	}

	isFilling = false;

	SetStatus (GS::ToUniString (std::to_wstring (static_cast<int> (materials.GetSize ())))
			   + FR (" matériau(x) · ")
			   + GS::ToUniString (std::to_wstring (static_cast<int> (withArticle)))
			   + FR (" avec classe"));
}


void MappingDialog::OpenPickerForSelection ()
{
	if (selectedMaterial < 1 || static_cast<UIndex> (selectedMaterial) > materials.GetSize ())
		return;

	if (articles.IsEmpty ()) {
		SetStatus (FR ("Aucune classe — choisissez un système de classification."));
		return;
	}

	const GS::UniString materialName = materials[static_cast<UIndex> (selectedMaterial) - 1];
	ArticlePickerDialog picker (articles);
	picker.Invoke ();
	if (!picker.IsAccepted ())
		return;

	const short articleIndex = picker.GetSelectedArticleIndex ();
	if (articleIndex == 0) {
		// « (aucune) » : retirer la correspondance (matériau ignoré du métré).
		for (UIndex r = 0; r < rules.GetSize (); ++r) {
			if (rules[r].structureType == CWStructureType::BuildingMaterial
				&& rules[r].structureName == materialName) {
				rules.Delete (r);
				break;
			}
		}
		SetStatus (FR ("« ") + materialName + FR (" » : sans classe (ignoré du métré)."));
	} else if (articleIndex >= 1 && static_cast<UIndex> (articleIndex) <= articles.GetSize ()) {
		const CWArticle& article = articles[static_cast<UIndex> (articleIndex) - 1];

		CWMapRule rule;
		rule.structureType = CWStructureType::BuildingMaterial;
		rule.structureName = materialName;
		rule.articleId = article.id;
		rule.mode = CWQuantMode::Element;

		bool replaced = false;
		for (UIndex r = 0; r < rules.GetSize (); ++r) {
			if (rules[r].structureType == CWStructureType::BuildingMaterial
				&& rules[r].structureName == materialName) {
				rules[r] = rule;
				replaced = true;
				break;
			}
		}
		if (!replaced)
			rules.Push (rule);

		SetStatus (FR ("« ") + materialName + FR (" » → ") + article.id + FR (" — ") + article.name);
	}

	FillList ();

	// Re-sélectionner le matériau traité (sans rouvrir le picker : le
	// renvoi d'événement est neutralisé par isFilling pendant FillList,
	// mais la sélection programmatique ne génère pas d'événement utilisateur).
	isFilling = true;
	if (selectedMaterial >= 1 && selectedMaterial <= list.GetItemCount ())
		list.SelectItem (selectedMaterial);
	isFilling = false;
}


void MappingDialog::SetStatus (const GS::UniString& message)
{
	statusText.SetText (message);
}


void MappingDialog::ListBoxSelectionChanged (const DG::ListBoxSelectionEvent& ev)
{
	if (ev.GetSource () != &list || isFilling)
		return;

	const short newSelection = list.GetSelectedItem ();
	if (newSelection < 1 || static_cast<UIndex> (newSelection) > materials.GetSize ())
		return;

	selectedMaterial = newSelection;
	OpenPickerForSelection ();
}


void MappingDialog::PopUpChanged (const DG::PopUpChangeEvent& ev)
{
	if (ev.GetSource () != &systemPopup || isFilling)
		return;

	RefreshArticles ();
	FillList ();
}


void MappingDialog::ButtonClicked (const DG::ButtonClickEvent& ev)
{
	if (ev.GetSource () == &saveButton) {
		GS::UniString error;
		if (RuleLibrary::SaveRules (rules, error)) {
			DG::InformationAlert (FR ("Correspondances enregistrées."),
								  GS::ToUniString (std::to_wstring (static_cast<int> (rules.GetSize ())))
								  + FR (" règle(s) matériaux écrites dans :")
								  + FR ("\n") + RuleLibrary::RulesFilePath (),
								  FR ("OK"));
			SetStatus (FR ("Enregistré."));
		} else {
			DG::ErrorAlert (FR ("Échec de l'enregistrement."), error, FR ("OK"));
		}
	} else if (ev.GetSource () == &closeButton) {
		// Fermer enregistre la bibliothèque (best effort).
		GS::UniString error;
		RuleLibrary::SaveRules (rules, error);
		PostCloseRequest (DG::ModalDialog::Accept);
	}
}

} // namespace CostWaves
