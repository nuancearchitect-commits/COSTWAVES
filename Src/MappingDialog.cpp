#include "CostWavesPrecompiledHeader.hpp"

#include "MappingDialog.hpp"

#include "ArticleManager.hpp"
#include "ArticlePickerDialog.hpp"
#include "CostWavesStyle.hpp"
#include "KeyCatalog.hpp"
#include "KeyPickerDialog.hpp"
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
	CostWavesStyle::ApplyHelp (infoText);
	CostWavesStyle::ApplyFieldLabel (typeLabel);
	CostWavesStyle::ApplyFieldLabel (systemLabel);
	CostWavesStyle::ApplyStatusChip (statusText);

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
	// Matériau : Matériau | Classe | Unité | Mode | Déduit | Déduit | Clé.
	// Composite/profil : Composite | case | Article | Unité | Mode | Déduit |
	// Déduit | Clé. Les réglages de calcul (tous types) se changent DANS la
	// ligne : Unité et Mode au clic (cycle), déductions par case à cocher.
	const short columnCount = materialMode ? 7 : 8;

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
	const short unitColumn = materialMode ? 3 : 4;
	list.SetHeaderItemText (unitColumn, FR ("Unité"));
	list.SetHeaderItemText (static_cast<short> (unitColumn + 1), FR ("Mode calcul"));
	list.SetHeaderItemText (static_cast<short> (unitColumn + 2), FR ("Déduit fen."));
	list.SetHeaderItemText (static_cast<short> (unitColumn + 3), FR ("Déduit trou"));
	list.SetHeaderItemText (columnCount, FR ("Valeur clé"));

	const short widthsMaterial[7] = { 150, 150, 56, 86, 60, 60, 118 };
	const short widthsStructure[8] = { 130, 32, 140, 56, 86, 60, 60, 116 };
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
	USize withKey = 0;
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
				CostWavesStyle::CellAccent (list, item, 2);
			} else {
				list.SetTabItemText (item, 2, FR ("—"));
				CostWavesStyle::CellMuted (list, item, 2);
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
				CostWavesStyle::CellAccent (list, item, 3);
			} else {
				list.SetTabItemText (item, 3, FR (kByMaterialsText));
				list.SetTabItemFontStyle (item, 3, DG::Font::Italic);
				CostWavesStyle::CellMuted (list, item, 3);
			}
		}

		// Colonnes de réglages de calcul (tous types) : Unité / Mode /
		// Déduit fenêtres / Déduit trous — éditables au clic dans la ligne.
		if (rule != nullptr && !rule->articleId.IsEmpty ()) {
			list.SetTabItemText (item, unitColumn, CWUnitDisplay (rule->unit));
			list.SetTabItemText (item, static_cast<short> (unitColumn + 1),
								 FR (CWCalcModeLabel (rule->calcMode)));
			list.SetTabItemIcon (item, static_cast<short> (unitColumn + 2), DG::Icon (SysResModule,
				static_cast<short> (rule->deductOpenings ? DG::ListBox::CheckedIcon : DG::ListBox::UncheckedIcon)));
			list.SetTabItemIcon (item, static_cast<short> (unitColumn + 3), DG::Icon (SysResModule,
				static_cast<short> (rule->deductHoles ? DG::ListBox::CheckedIcon : DG::ListBox::UncheckedIcon)));
		} else {
			list.SetTabItemText (item, unitColumn, FR ("—"));
			list.SetTabItemText (item, static_cast<short> (unitColumn + 1), FR ("—"));
		}

		// Dernière colonne : « Valeur clé » — paramètre différenciant les
		// articles d'une même classe (ex. épaisseur). « — » = aucun.
		const short keyColumn = columnCount;
		if (rule != nullptr && !rule->keyName.IsEmpty ()) {
			++withKey;
			list.SetTabItemText (item, keyColumn, rule->keyName);
			CostWavesStyle::CellOk (list, item, keyColumn);
		} else {
			list.SetTabItemText (item, keyColumn, FR ("—"));
			CostWavesStyle::CellMuted (list, item, keyColumn);
		}
	}

	isFilling = false;

	if (materialMode) {
		SetStatus (GS::ToUniString (std::to_wstring (static_cast<int> (materials.GetSize ())))
				   + FR (" matériau(x) · ")
				   + GS::ToUniString (std::to_wstring (static_cast<int> (withArticle)))
				   + FR (" avec classe · ")
				   + GS::ToUniString (std::to_wstring (static_cast<int> (withKey)))
				   + FR (" avec valeur clé"));
	} else {
		SetStatus (GS::ToUniString (std::to_wstring (static_cast<int> (materials.GetSize ())))
				   + (structureType == CWStructureType::Composite ? FR (" composite(s) · ") : FR (" profil(s) · "))
				   + GS::ToUniString (std::to_wstring (static_cast<int> (withArticle)))
				   + FR (" avec article — les autres quantifiés par leurs matériaux · ")
				   + GS::ToUniString (std::to_wstring (static_cast<int> (withKey)))
				   + FR (" avec valeur clé"));
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


void MappingDialog::OpenKeyPickerForSelection ()
{
	if (selectedMaterial < 1 || static_cast<UIndex> (selectedMaterial) > materials.GetSize ())
		return;

	const CWStructureType structureType = CurrentType ();
	const GS::UniString materialName = materials[static_cast<UIndex> (selectedMaterial) - 1];

	// La valeur clé différencie les ARTICLES d'une même classe : elle n'a
	// de sens que pour une structure qui a déjà sa classe (article).
	const CWMapRule* rule = RuleLibrary::FindRule (rules, structureType, materialName);
	if (rule == nullptr || rule->articleId.IsEmpty ()) {
		SetStatus (FR ("« ") + materialName
				   + FR (" » : choisissez d'abord sa classe — la valeur clé différencie ses articles."));
		return;
	}

	GS::Array<CWKeyEntry> keys;
	KeyCatalog::CollectAvailableKeys (keys);

	KeyPickerDialog picker (keys, rule->keyId);
	picker.Invoke ();
	if (!picker.IsAccepted ())
		return;

	const short keyIndex = picker.GetSelectedKeyIndex ();
	for (UIndex r = 0; r < rules.GetSize (); ++r) {
		if (rules[r].structureType != structureType || rules[r].structureName != materialName)
			continue;

		if (keyIndex == 0) {
			// « (aucune) » : retirer la valeur clé.
			rules[r].keyId.Clear ();
			rules[r].keyName.Clear ();
			SetStatus (FR ("« ") + materialName + FR (" » : valeur clé retirée."));
		} else if (keyIndex >= 1 && static_cast<UIndex> (keyIndex) <= keys.GetSize ()) {
			const CWKeyEntry& key = keys[static_cast<UIndex> (keyIndex) - 1];
			rules[r].keyId = key.id;
			rules[r].keyName = key.name;
			SetStatus (FR ("« ") + materialName + FR (" » — valeur clé : ") + key.name);
		}
		break;
	}

	FillList ();

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

	// Le choix s'ouvre au CLIC (ListBoxClicked), qui connaît la COLONNE
	// cliquée : « Classe »/« Article » -> classe ; « Valeur clé » ->
	// paramètre différenciant. Ici, on ne fait que mémoriser la sélection.
	const short newSelection = list.GetSelectedItem ();
	if (newSelection >= 1 && static_cast<UIndex> (newSelection) <= materials.GetSize ())
		selectedMaterial = newSelection;
}


void MappingDialog::ListBoxClicked (const DG::ListBoxClickEvent& ev)
{
	if (ev.GetSource () != &list || isFilling)
		return;

	const short clicked = ev.GetListItem ();
	if (clicked < 1 || static_cast<UIndex> (clicked) > materials.GetSize ())
		return;

	selectedMaterial = clicked;

	// Colonne « Valeur clé » -> catalogue des paramètres différenciants
	// (clés calculées + propriétés Archicad) ; colonnes de réglages
	// (Unité / Mode calcul / Déduit fenêtre / Déduit trou) -> changement
	// DANS la ligne ; le reste de la ligne -> classe (article). Matériau,
	// composite et profil suivent le même schéma.
	const short keyColumn = IsMaterialMode () ? 7 : 8;
	const short unitColumn = IsMaterialMode () ? 3 : 4;
	const short tab = ev.GetTabFieldIndex ();
	if (tab == keyColumn)
		OpenKeyPickerForSelection ();
	else if (tab >= unitColumn && tab <= static_cast<short> (unitColumn + 3))
		EditRuleCalcSetting (static_cast<short> (tab - unitColumn));
	else
		OpenPickerForSelection ();
}


void MappingDialog::EditRuleCalcSetting (short setting)
{
	if (selectedMaterial < 1 || static_cast<UIndex> (selectedMaterial) > materials.GetSize ())
		return;

	const CWStructureType structureType = CurrentType ();
	const GS::UniString materialName = materials[static_cast<UIndex> (selectedMaterial) - 1];

	for (UIndex r = 0; r < rules.GetSize (); ++r) {
		if (rules[r].structureType != structureType || rules[r].structureName != materialName)
			continue;
		if (rules[r].articleId.IsEmpty ()) {
			SetStatus (FR ("« ") + materialName + FR (" » : choisissez d'abord sa classe (article)."));
			return;
		}

		// Cycle Unité (auto -> m² -> ml -> m³ -> u -> kg) / Mode calcul
		// (Brute -> Conditionnelle -> Nette) / bascule des déductions.
		GS::UniString message;
		switch (setting) {
			case 0:
				rules[r].unit = CWNextUnit (rules[r].unit);
				message = FR ("« ") + materialName + FR (" » — unité : ") + CWUnitDisplay (rules[r].unit);
				break;
			case 1:
				rules[r].calcMode = static_cast<CWCalcMode> ((static_cast<int> (rules[r].calcMode) + 1) % 3);
				message = FR ("« ") + materialName + FR (" » — mode de calcul : ")
					+ FR (CWCalcModeLabel (rules[r].calcMode));
				break;
			case 2:
				rules[r].deductOpenings = !rules[r].deductOpenings;
				message = FR ("« ") + materialName + FR (" » — déduire les ouvertures : ")
					+ (rules[r].deductOpenings ? FR ("oui") : FR ("non"));
				break;
			default:
				rules[r].deductHoles = !rules[r].deductHoles;
				message = FR ("« ") + materialName + FR (" » — déduire les trous : ")
					+ (rules[r].deductHoles ? FR ("oui") : FR ("non"));
				break;
		}

		FillList ();
		SetStatus (message);
		isFilling = true;
		if (selectedMaterial >= 1 && selectedMaterial <= list.GetItemCount ())
			list.SelectItem (selectedMaterial);
		isFilling = false;
		return;
	}

	SetStatus (FR ("« ") + materialName + FR (" » : choisissez d'abord sa classe (article)."));
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
