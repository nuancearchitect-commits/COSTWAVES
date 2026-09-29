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

// Types du popup « Type ».
const CWStructureType kTypes[3] = {
	CWStructureType::BuildingMaterial,
	CWStructureType::Composite,
	CWStructureType::Profile
};

// Texte du mode « décomposé » (composite/profil sans article propre).
const char* kByMaterialsText = "quantifié par matériau décomposé";

} // namespace


MappingDialog::MappingDialog ()
	:	DG::ModalDialog (ACAPI_GetOwnResModule (), ID_ADDON_DLG_MAPPING, ACAPI_GetOwnResModule ()),
		infoText (GetReference (), InfoTextId),
		typeLabel (GetReference (), TypeLabelId),
		typePopup (GetReference (), TypePopupId),
		systemLabel (GetReference (), SystemLabelId),
		systemPopup (GetReference (), SystemPopupId),
		list (GetReference (), ListId),
		statusText (GetReference (), StatusTextId),
		saveButton (GetReference (), SaveButtonId),
		closeButton (GetReference (), CloseButtonId)
{
	infoText.SetText (FR ("Cliquez sur un attribut pour lui choisir sa classe (article). ")
					  + FR ("Matériau sans classe = ignoré ; composite/profil sans classe = ")
					  + FR ("quantifié par ses matériaux."));

	// Filtre type d'attribut : Matériau / Composite / Profil complexe.
	typePopup.AppendItem ();
	typePopup.SetItemText (1, FR ("Matériau"));
	typePopup.AppendItem ();
	typePopup.SetItemText (2, FR ("Composite"));
	typePopup.AppendItem ();
	typePopup.SetItemText (3, FR ("Profil complexe"));
	typePopup.SelectItem (1);

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

	// Bibliothèque existante (Documents/CostWaves-regles.json). Un échec de
	// lecture est AFFICHÉ : ne jamais perdre des règles en silence.
	GS::UniString rulesError;
	if (!RuleLibrary::LoadRules (rules, rulesError) && !rulesError.IsEmpty ())
		DG::WarningAlert (FR ("La bibliothèque de correspondances n'a pas pu être lue."),
						  rulesError, FR ("OK"));

	RefreshArticles ();
	RefreshMaterials ();

	saveButton.Attach (*this);
	closeButton.Attach (*this);
	typePopup.Attach (*this);
	systemPopup.Attach (*this);
	list.Attach (*this);
}


bool MappingDialog::IsMaterialMode () const
{
	return typePopup.GetSelectedItem () == 1;
}


CWStructureType MappingDialog::CurrentType () const
{
	const short selection = typePopup.GetSelectedItem ();
	if (selection >= 1 && selection <= 3)
		return kTypes[selection - 1];
	return CWStructureType::BuildingMaterial;
}


void MappingDialog::RefreshMaterials ()
{
	isFilling = true;
	materials.Clear ();
	RuleLibrary::CollectAvailableStructures (CurrentType (), materials);
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

	const bool materialMode = IsMaterialMode ();
	const CWStructureType structureType = CurrentType ();
	const short columnCount = materialMode ? 2 : 3;

	list.SetHeaderItemCount (columnCount);
	list.SetTabFieldCount (columnCount);
	if (materialMode) {
		list.SetHeaderItemText (1, FR ("Matériau"));
		list.SetHeaderItemText (2, FR ("Classe (article)"));
	} else {
		list.SetHeaderItemText (1, structureType == CWStructureType::Composite
			? FR ("Composite") : FR ("Profil complexe"));
		list.SetHeaderItemText (2, FR (""));
		list.SetHeaderItemText (3, FR ("Article"));
	}

	const short widthsMaterial[2] = { 280, 360 };
	const short widthsStructure[3] = { 300, 60, 300 };
	short position = 0;
	for (short i = 1; i <= columnCount; ++i) {
		const short width = materialMode ? widthsMaterial[i - 1] : widthsStructure[i - 1];
		list.SetHeaderItemSize (i, width);
		list.SetHeaderItemSizeableFlag (i, true);
		list.SetTabFieldProperties (i, position, static_cast<short> (position + width),
									 DG::ListBox::Left, DG::ListBox::MiddleTruncate, i > 1);
		position = static_cast<short> (position + width);
	}

	while (list.GetItemCount () > 0)
		list.DeleteItem (1);

	USize withArticle = 0;
	for (UIndex m = 0; m < materials.GetSize (); ++m) {
		list.AppendItem ();
		const short item = list.GetItemCount ();
		list.SetTabItemText (item, 1, materials[m]);

		const CWMapRule* rule = RuleLibrary::FindRule (rules, structureType, materials[m]);

		if (materialMode) {
			// Matériau : classe directement, pas de case à cocher.
			if (rule != nullptr && !rule->articleId.IsEmpty ()) {
				++withArticle;
				const CWArticle* article = ArticleManager::FindArticle (articles, rule->articleId);
				list.SetTabItemText (item, 2, article != nullptr
					? rule->articleId + FR (" — ") + article->name
					: rule->articleId);
			} else {
				list.SetTabItemText (item, 2, FR ("—"));
			}
		} else {
			// Composite / profil : case à cocher (icône) + article ou
			// « quantifié par matériau décomposé ». Sans classe, la structure
			// n'est pas comptée comme un tout : le métré passe par les
			// matériaux de ses couches.
			const bool hasArticle = (rule != nullptr && !rule->articleId.IsEmpty ());
			list.SetTabItemIcon (item, 2, DG::Icon (SysResModule,
				static_cast<short> (hasArticle ? DG::ListBox::CheckedIcon : DG::ListBox::UncheckedIcon)));
			if (hasArticle) {
				++withArticle;
				const CWArticle* article = ArticleManager::FindArticle (articles, rule->articleId);
				list.SetTabItemText (item, 3, article != nullptr
					? rule->articleId + FR (" — ") + article->name
					: rule->articleId);
				list.SetTabItemFontStyle (item, 3, DG::Font::Plain);
			} else {
				list.SetTabItemText (item, 3, FR (kByMaterialsText));
				list.SetTabItemFontStyle (item, 3, DG::Font::Italic);
			}
		}
	}

	isFilling = false;

	if (materialMode) {
		SetStatus (GS::ToUniString (std::to_wstring (static_cast<int> (materials.GetSize ())))
				   + FR (" matériau(x) · ")
				   + GS::ToUniString (std::to_wstring (static_cast<int> (withArticle)))
				   + FR (" avec classe"));
	} else {
		SetStatus (GS::ToUniString (std::to_wstring (static_cast<int> (materials.GetSize ())))
				   + (structureType == CWStructureType::Composite ? FR (" composite(s) · ") : FR (" profil(s) · "))
				   + GS::ToUniString (std::to_wstring (static_cast<int> (withArticle)))
				   + FR (" avec article — les autres quantifiés par leurs matériaux"));
	}
}


void MappingDialog::OpenPickerForSelection ()
{
	if (selectedMaterial < 1 || static_cast<UIndex> (selectedMaterial) > materials.GetSize ())
		return;

	if (articles.IsEmpty ()) {
		SetStatus (FR ("Aucune classe — choisissez un système de classification."));
		return;
	}

	const bool materialMode = IsMaterialMode ();
	const CWStructureType structureType = CurrentType ();
	const GS::UniString materialName = materials[static_cast<UIndex> (selectedMaterial) - 1];
	ArticlePickerDialog picker (articles);
	picker.Invoke ();
	if (!picker.IsAccepted ())
		return;

	const short articleIndex = picker.GetSelectedArticleIndex ();
	if (articleIndex == 0) {
		// « (aucune) » : retirer la correspondance. Matériau = ignoré du
		// métré ; composite/profil = quantifié par ses matériaux (décomposé).
		for (UIndex r = 0; r < rules.GetSize (); ++r) {
			if (rules[r].structureType == structureType
				&& rules[r].structureName == materialName) {
				rules.Delete (r);
				break;
			}
		}
		SetStatus (FR ("« ") + materialName
				   + (materialMode ? FR (" » : sans classe (ignoré du métré).")
								   : FR (" » : quantifié par matériau décomposé.")));
	} else if (articleIndex >= 1 && static_cast<UIndex> (articleIndex) <= articles.GetSize ()) {
		const CWArticle& article = articles[static_cast<UIndex> (articleIndex) - 1];

		CWMapRule rule;
		rule.structureType = structureType;
		rule.structureName = materialName;
		rule.articleId = article.id;
		rule.mode = CWQuantMode::Element;

		bool replaced = false;
		for (UIndex r = 0; r < rules.GetSize (); ++r) {
			if (rules[r].structureType == structureType
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

	// Mode matériau : la sélection ouvre le choix de la classe. Les composites
	// et profils passent par ListBoxClicked (case à cocher).
	if (!IsMaterialMode ())
		return;

	const short newSelection = list.GetSelectedItem ();
	if (newSelection < 1 || static_cast<UIndex> (newSelection) > materials.GetSize ())
		return;

	selectedMaterial = newSelection;
	OpenPickerForSelection ();
}


void MappingDialog::ListBoxClicked (const DG::ListBoxClickEvent& ev)
{
	if (ev.GetSource () != &list || isFilling)
		return;

	// Mode composite / profil : le clic (sur la ligne ou sa case à cocher)
	// ouvre le choix de la classe. « (aucune) » décoche -> quantifié par
	// matériau décomposé.
	if (IsMaterialMode ())
		return;

	const short clicked = list.GetSelectedItem ();
	if (clicked < 1 || static_cast<UIndex> (clicked) > materials.GetSize ())
		return;

	selectedMaterial = clicked;
	OpenPickerForSelection ();
}


void MappingDialog::PopUpChanged (const DG::PopUpChangeEvent& ev)
{
	if (isFilling)
		return;

	if (ev.GetSource () == &typePopup) {
		// Changement de type d'attribut : recharger la liste du projet.
		RefreshMaterials ();
	} else if (ev.GetSource () == &systemPopup) {
		RefreshArticles ();
		FillList ();
	}
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
