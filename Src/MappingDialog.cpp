#include "CostWavesPrecompiledHeader.hpp"

#include "MappingDialog.hpp"

#include "ArticleManager.hpp"
#include "ModelReader.hpp"
#include "RuleLibrary.hpp"

#include "UniStringWStringConversion.hpp"

namespace CostWaves {

namespace {

GS::UniString FR (const char* utf8Text)
{
	return GS::UniString (utf8Text, CC_UTF8);
}

// Indentation hiérarchique des classes (style classification Archicad).
GS::UniString Indent (short depth)
{
	GS::UniString indent;
	for (short d = 0; d < depth; ++d)
		indent += FR ("    ");
	return indent;
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
		closeButton (GetReference (), CloseButtonId),
		editorHeader (GetReference (), EditorHeaderId),
		editorSearch (GetReference (), EditorSearchId),
		editorList (GetReference (), EditorListId),
		editorChooseButton (GetReference (), EditorChooseId),
		editorCloseButton (GetReference (), EditorCloseId)
{
	infoText.SetText (FR ("Cliquez sur un matériau (▼) pour lui choisir sa classe : ")
					  + FR ("la liste s'ouvre ici même, avec recherche. ")
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

	// L'éditeur en place est masqué tant qu'aucun matériau n'est cliqué.
	editorHeader.Hide ();
	editorSearch.Hide ();
	editorList.Hide ();
	editorChooseButton.Hide ();
	editorCloseButton.Hide ();

	saveButton.Attach (*this);
	closeButton.Attach (*this);
	systemPopup.Attach (*this);
	list.Attach (*this);
	editorSearch.Attach (*this);
	editorList.Attach (*this);
	editorChooseButton.Attach (*this);
	editorCloseButton.Attach (*this);
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

	const short columnCount = 3;
	list.SetHeaderItemCount (columnCount);
	list.SetTabFieldCount (columnCount);
	list.SetHeaderItemText (1, FR ("Matériau"));
	list.SetHeaderItemText (2, FR ("Classe (article)"));
	list.SetHeaderItemText (3, FR (""));

	const short widths[3] = { 300, 330, 50 };
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

	for (UIndex m = 0; m < materials.GetSize (); ++m) {
		list.AppendItem ();
		const short item = list.GetItemCount ();
		list.SetTabItemText (item, 1, materials[m]);
		list.SetTabItemText (item, 2, FR ("—"));
		list.SetTabItemText (item, 3, FR ("▼"));
	}

	// Cellules « Classe » d'après les règles.
	for (short item = 1; item <= list.GetItemCount (); ++item) {
		UpdateMaterialRow (item);
	}

	isFilling = false;

	UpdateStatusCounter ();
}


void MappingDialog::UpdateMaterialRow (short item)
{
	if (item < 1 || static_cast<UIndex> (item) > materials.GetSize ())
		return;

	const GS::UniString& materialName = materials[static_cast<UIndex> (item) - 1];
	const CWMapRule* rule = RuleLibrary::FindRule (rules, CWStructureType::BuildingMaterial, materialName);
	if (rule != nullptr && !rule->articleId.IsEmpty ()) {
		const CWArticle* article = ArticleManager::FindArticle (articles, rule->articleId);
		list.SetTabItemText (item, 2, article != nullptr
			? rule->articleId + FR (" — ") + article->name
			: rule->articleId);
	} else {
		list.SetTabItemText (item, 2, FR ("—"));
	}
}


void MappingDialog::OpenEditorForSelection ()
{
	if (selectedMaterial < 1 || static_cast<UIndex> (selectedMaterial) > materials.GetSize ())
		return;

	if (articles.IsEmpty ()) {
		SetStatus (FR ("Aucune classe — choisissez un système de classification."));
		return;
	}
	if (editorOpen)
		return;

	editorOpen = true;
	selectedClassItem = 0;

	const GS::UniString materialName = materials[static_cast<UIndex> (selectedMaterial) - 1];
	editorHeader.SetText (FR ("Classe de « ") + materialName + FR (" » — recherchez, puis double-cliquez :"));
	editorSearch.SetText (GS::UniString ());
	FillEditorList (GS::UniString ());

	// La liste s'ouvre À LA PLACE du tableau (sélection en place).
	list.Hide ();
	editorHeader.Show ();
	editorSearch.Show ();
	editorList.Show ();
	editorChooseButton.Show ();
	editorCloseButton.Show ();
}


void MappingDialog::FillEditorList (const GS::UniString& filter)
{
	isFilling = true;

	const short columnCount = 2;
	editorList.SetHeaderItemCount (columnCount);
	editorList.SetTabFieldCount (columnCount);
	editorList.SetHeaderItemText (1, FR ("ID"));
	editorList.SetHeaderItemText (2, FR ("Nom"));

	const short widths[2] = { 220, 460 };
	short position = 0;
	for (short i = 1; i <= columnCount; ++i) {
		editorList.SetHeaderItemSize (i, widths[i - 1]);
		editorList.SetHeaderItemSizeableFlag (i, true);
		editorList.SetTabFieldProperties (i, position, static_cast<short> (position + widths[i - 1]),
										 DG::ListBox::Left, DG::ListBox::MiddleTruncate, i > 1);
		position = static_cast<short> (position + widths[i - 1]);
	}

	while (editorList.GetItemCount () > 0)
		editorList.DeleteItem (1);

	visibleArticles.Clear ();

	// Première entrée : « (aucune) » = pas de correspondance (matériau ignoré).
	editorList.AppendItem ();
	editorList.SetTabItemText (1, 1, FR ("—"));
	editorList.SetTabItemText (1, 2, FR ("(aucune)"));

	const GS::UniString needle = filter.ToUpperCase ();
	for (UIndex a = 0; a < articles.GetSize (); ++a) {
		if (!needle.IsEmpty ()
			&& !articles[a].id.ToUpperCase ().Contains (needle)
			&& !articles[a].name.ToUpperCase ().Contains (needle))
			continue;

		editorList.AppendItem ();
		const short item = editorList.GetItemCount ();
		editorList.SetTabItemText (item, 1, Indent (articles[a].depth) + articles[a].id);
		editorList.SetTabItemText (item, 2, articles[a].name);
		visibleArticles.Push (static_cast<short> (a + 1));
	}

	if (editorList.GetItemCount () > 0) {
		editorList.SelectItem (1);
		selectedClassItem = 1;
	}

	isFilling = false;
}


void MappingDialog::EditorChoose ()
{
	if (!editorOpen)
		return;

	const short item = selectedClassItem;
	if (item < 1 || item > editorList.GetItemCount ())
		return;

	const GS::UniString materialName = materials[static_cast<UIndex> (selectedMaterial) - 1];

	if (item == 1) {
		// « (aucune) » : retirer la correspondance (matériau ignoré du métré).
		for (UIndex r = 0; r < rules.GetSize (); ++r) {
			if (rules[r].structureType == CWStructureType::BuildingMaterial
				&& rules[r].structureName == materialName) {
				rules.Delete (r);
				break;
			}
		}
		SetStatus (FR ("« ") + materialName + FR (" » : sans classe (ignoré du métré)."));
	} else {
		const short visibleIndex = static_cast<short> (item - 1);
		if (visibleIndex < 1 || static_cast<UIndex> (visibleIndex) > visibleArticles.GetSize ())
			return;
		const short articleIndex = visibleArticles[static_cast<UIndex> (visibleIndex) - 1];
		if (articleIndex < 1 || static_cast<UIndex> (articleIndex) > articles.GetSize ())
			return;
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

	CloseEditor (true);
}


void MappingDialog::CloseEditor (bool applied)
{
	editorHeader.Hide ();
	editorSearch.Hide ();
	editorList.Hide ();
	editorChooseButton.Hide ();
	editorCloseButton.Hide ();
	list.Show ();
	editorOpen = false;

	if (applied) {
		UpdateMaterialRow (selectedMaterial);
		UpdateStatusCounter ();
	}

	// Désélectionner la ligne pour qu'un nouveau clic (même ligne) rouvre la
	// liste — la sélection programmatique n'émet pas d'événement utilisateur,
	// le garde neutralise tout risque de rebouclage.
	isFilling = true;
	if (selectedMaterial >= 1 && selectedMaterial <= list.GetItemCount ())
		list.DeselectItem (selectedMaterial);
	isFilling = false;
}


void MappingDialog::UpdateStatusCounter ()
{
	USize withArticle = 0;
	for (UIndex m = 0; m < materials.GetSize (); ++m) {
		const CWMapRule* rule = RuleLibrary::FindRule (rules, CWStructureType::BuildingMaterial, materials[m]);
		if (rule != nullptr && !rule->articleId.IsEmpty ())
			++withArticle;
	}

	SetStatus (GS::ToUniString (std::to_wstring (static_cast<int> (materials.GetSize ())))
			   + FR (" matériau(x) · ")
			   + GS::ToUniString (std::to_wstring (static_cast<int> (withArticle)))
			   + FR (" avec classe"));
}


void MappingDialog::SetStatus (const GS::UniString& message)
{
	statusText.SetText (message);
}


void MappingDialog::ListBoxSelectionChanged (const DG::ListBoxSelectionEvent& ev)
{
	if (isFilling)
		return;

	if (ev.GetSource () == &list) {
		if (editorOpen)
			return;
		const short newSelection = list.GetSelectedItem ();
		if (newSelection < 1 || static_cast<UIndex> (newSelection) > materials.GetSize ())
			return;
		selectedMaterial = newSelection;
		OpenEditorForSelection ();
	} else if (ev.GetSource () == &editorList) {
		selectedClassItem = editorList.GetSelectedItem ();
	}
}


void MappingDialog::ListBoxDoubleClicked (const DG::ListBoxDoubleClickEvent& ev)
{
	if (isFilling)
		return;

	if (ev.GetSource () == &editorList) {
		selectedClassItem = editorList.GetSelectedItem ();
		EditorChoose ();
	} else if (ev.GetSource () == &list) {
		if (editorOpen)
			return;
		const short newSelection = list.GetSelectedItem ();
		if (newSelection < 1 || static_cast<UIndex> (newSelection) > materials.GetSize ())
			return;
		selectedMaterial = newSelection;
		OpenEditorForSelection ();
	}
}


void MappingDialog::PopUpChanged (const DG::PopUpChangeEvent& ev)
{
	if (ev.GetSource () != &systemPopup || isFilling)
		return;

	if (editorOpen)
		CloseEditor (false);

	RefreshArticles ();
	FillList ();
}


void MappingDialog::SearchTextChanged (const DG::SearchEditChangeEvent& ev)
{
	if (ev.GetSource () != &editorSearch || !editorOpen)
		return;

	FillEditorList (editorSearch.GetText ());
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
	} else if (ev.GetSource () == &editorChooseButton) {
		EditorChoose ();
	} else if (ev.GetSource () == &editorCloseButton) {
		CloseEditor (false);
	}
}

} // namespace CostWaves
