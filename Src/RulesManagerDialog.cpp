#include "CostWavesPrecompiledHeader.hpp"

#include "UniStringWStringConversion.hpp"

#include "RulesManagerDialog.hpp"

#include "ArticleEditDialog.hpp"
#include "ArticleManager.hpp"
#include "Exporter.hpp"
#include "LayersDialog.hpp"
#include "ModelReader.hpp"
#include "RuleLibrary.hpp"

namespace CostWaves {

namespace {

GS::UniString FR (const char* utf8Text)
{
	return GS::UniString (utf8Text, CC_UTF8);
}

// Sélecteur de structure : liste les structures du type choisi dans
// l'environnement Archicad courant (composites, profils, objets .gsm,
// matériaux). Sans projet ouvert, la liste est vide — le nom reste saisissable
// à la main dans le gestionnaire.
class StructurePickerDialog final :	public DG::ModalDialog,
									public DG::ButtonItemObserver
{
public:
	StructurePickerDialog (CWStructureType structureType)
		:	DG::ModalDialog (ACAPI_GetOwnResModule (), ID_ADDON_DLG_PICKER, ACAPI_GetOwnResModule ()),
			infoText (GetReference (), 1),
			structureLabel (GetReference (), 2),
			structurePopup (GetReference (), 3),
			chooseButton (GetReference (), 4),
			cancelButton (GetReference (), 5)
	{
		infoText.SetText (RuleLibrary::StructureTypeName (structureType)
						  + FR (" — choisissez une structure de l'environnement Archicad courant."));

		GS::Array<GS::UniString> names;
		RuleLibrary::CollectAvailableStructures (structureType, names);
		for (UIndex i = 0; i < names.GetSize (); ++i) {
			structurePopup.AppendItem ();
			structurePopup.SetItemText (structurePopup.GetItemCount (), names[i]);
		}
		if (structurePopup.GetItemCount () > 0)
			structurePopup.SelectItem (1);
		else
			infoText.SetText (FR ("Aucune structure de ce type dans l'environnement courant ")
							  + FR ("(sans maquette, saisissez le nom à la main)."));

		chooseButton.Attach (*this);
		cancelButton.Attach (*this);
	}

	bool					IsAccepted () const { return accepted; }
	const GS::UniString&	GetSelectedName () const { return selectedName; }

private:
	virtual void	ButtonClicked (const DG::ButtonClickEvent& ev) override
	{
		if (ev.GetSource () == &chooseButton) {
			const short selection = structurePopup.GetSelectedItem ();
			if (selection >= 1 && selection <= structurePopup.GetItemCount ()) {
				selectedName = structurePopup.GetItemText (selection);
				accepted = true;
				PostCloseRequest (DG::ModalDialog::Accept);
			}
		} else if (ev.GetSource () == &cancelButton) {
			PostCloseRequest (DG::ModalDialog::Cancel);
		}
	}

	DG::LeftText	infoText;
	DG::LeftText	structureLabel;
	DG::PopUp		structurePopup;
	DG::Button		chooseButton;
	DG::Button		cancelButton;

	GS::UniString	selectedName;
	bool			accepted = false;
};

// Types du popup « Type », dans l'ordre.
const CWStructureType kPopupTypes[4] = {
	CWStructureType::Composite,
	CWStructureType::Profile,
	CWStructureType::LibraryPart,
	CWStructureType::BuildingMaterial
};

CWStructureType SelectedType (const DG::PopUp& popup)
{
	const short selection = popup.GetSelectedItem ();
	if (selection >= 1 && selection <= 4)
		return kPopupTypes[selection - 1];
	return CWStructureType::Composite;
}

} // namespace


RulesManagerDialog::RulesManagerDialog ()
	:	DG::ModalDialog (ACAPI_GetOwnResModule (), ID_ADDON_DLG_MANAGER, ACAPI_GetOwnResModule ()),
		infoText (GetReference (), InfoTextId),
		table (GetReference (), TableId),
		typeLabel (GetReference (), TypeLabelId),
		typePopup (GetReference (), TypePopupId),
		structureLabel (GetReference (), StructureLabelId),
		structureEdit (GetReference (), StructureEditId),
		browseButton (GetReference (), BrowseButtonId),
		articleLabel (GetReference (), ArticleLabelId),
		articlePopup (GetReference (), ArticlePopupId),
		createArticleButton (GetReference (), CreateArticleButtonId),
		modeLabel (GetReference (), ModeLabelId),
		modePopup (GetReference (), ModePopupId),
		ignoreCheck (GetReference (), IgnoreCheckId),
		applyButton (GetReference (), ApplyButtonId),
		deleteButton (GetReference (), DeleteButtonId),
		statusText (GetReference (), StatusTextId),
		importButton (GetReference (), ImportButtonId),
		saveButton (GetReference (), SaveButtonId),
		closeButton (GetReference (), CloseButtonId)
{
	infoText.SetText (FR ("Préparez les correspondances SANS maquette : composites et profils ")
					  + FR ("(eux-mêmes ou leurs couches), objets .gsm, matériaux. ")
					  + FR ("La bibliothèque (Documents/CostWaves-regles.json) est réutilisable entre projets."));

	// Types de structures.
	for (short t = 0; t < 4; ++t) {
		typePopup.AppendItem ();
		typePopup.SetItemText (static_cast<short> (t + 1), RuleLibrary::StructureTypeName (kPopupTypes[t]));
	}
	typePopup.SelectItem (1);

	// Mode de métré (composites et profils uniquement).
	modePopup.AppendItem ();
	modePopup.SetItemText (1, FR ("Lui-même (1 article)"));
	modePopup.AppendItem ();
	modePopup.SetItemText (2, FR ("Ses couches (par matériau)"));
	modePopup.SelectItem (1);

	// Base d'articles : chargée automatiquement (Documents + locaux).
	GS::UniString baseError;
	ArticleManager::LoadArticleBase (articles, baseError);
	RefreshArticlePopup (GS::UniString ());

	FillTable ();

	applyButton.Attach (*this);
	deleteButton.Attach (*this);
	browseButton.Attach (*this);
	createArticleButton.Attach (*this);
	importButton.Attach (*this);
	saveButton.Attach (*this);
	closeButton.Attach (*this);
	table.Attach (*this);
	typePopup.Attach (*this);
	modePopup.Attach (*this);

	UpdateModePopupState ();
}


void RulesManagerDialog::UpdateModePopupState ()
{
	// Le mode « Lui-même / Ses couches » n'a de sens que pour les composites
	// et les profils.
	const CWStructureType structureType = SelectedType (typePopup);
	if (structureType == CWStructureType::Composite || structureType == CWStructureType::Profile)
		modePopup.Enable ();
	else
		modePopup.Disable ();
}


void RulesManagerDialog::RefreshArticlePopup (const GS::UniString& preferredArticleId)
{
	while (articlePopup.GetItemCount () > 0)
		articlePopup.DeleteItem (1);

	for (UIndex a = 0; a < articles.GetSize (); ++a) {
		GS::UniString label = articles[a].id + FR (" — ") + articles[a].name;
		if (!articles[a].unit.IsEmpty ())
			label += FR (" (") + articles[a].unit + FR (")");
		articlePopup.AppendItem ();
		articlePopup.SetItemText (articlePopup.GetItemCount (), label);
	}

	short preferred = 0;
	if (!preferredArticleId.IsEmpty ()) {
		for (UIndex a = 0; a < articles.GetSize (); ++a) {
			if (articles[a].id == preferredArticleId) {
				preferred = static_cast<short> (a + 1);
				break;
			}
		}
	}
	if (preferred == 0 && articlePopup.GetItemCount () > 0)
		preferred = 1;
	if (preferred > 0)
		articlePopup.SelectItem (preferred);
}


void RulesManagerDialog::FillTable ()
{
	isFilling = true;

	const short columnCount = 5;
	table.SetHeaderItemCount (columnCount);
	table.SetTabFieldCount (columnCount);
	table.SetHeaderItemText (1, FR ("Type"));
	table.SetHeaderItemText (2, FR ("Structure"));
	table.SetHeaderItemText (3, FR ("Métré"));
	table.SetHeaderItemText (4, FR ("Article"));
	table.SetHeaderItemText (5, FR ("État"));

	const short widths[5] = { 140, 190, 150, 180, 120 };
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

	for (UIndex r = 0; r < rules.GetSize (); ++r) {
		const CWMapRule& rule = rules[r];
		table.AppendItem ();
		const short item = table.GetItemCount ();
		table.SetTabItemText (item, 1, RuleLibrary::StructureTypeName (rule.structureType));
		table.SetTabItemText (item, 2, rule.structureName);

		if (rule.ignored) {
			table.SetTabItemText (item, 3, FR ("—"));
			table.SetTabItemText (item, 4, FR ("—"));
			table.SetTabItemText (item, 5, FR ("Ignoré"));
			continue;
		}

		// Mode de métré affiché pour composites/profils ; les matériaux et
		// objets sont toujours au niveau de leur propre structure.
		if (rule.structureType == CWStructureType::Composite
			|| rule.structureType == CWStructureType::Profile) {
			if (rule.mode == CWQuantMode::Component) {
				table.SetTabItemText (item, 3, FR ("Ses couches"));
				table.SetTabItemText (item, 4, FR ("(par matériau)"));
			} else {
				table.SetTabItemText (item, 3, FR ("Lui-même"));
			}
		} else if (rule.structureType == CWStructureType::BuildingMaterial) {
			table.SetTabItemText (item, 3, FR ("Couche"));
		} else {
			table.SetTabItemText (item, 3, FR ("Lui-même"));
		}

		const CWArticle* article = ArticleManager::FindArticle (articles, rule.articleId);
		if (!rule.articleId.IsEmpty ()) {
			table.SetTabItemText (item, 4, article != nullptr
				? rule.articleId + FR (" — ") + article->name
				: rule.articleId);
			table.SetTabItemText (item, 5, article != nullptr ? FR ("OK") : FR ("⚠ Article introuvable"));
		} else if (rule.mode == CWQuantMode::Component) {
			// Mode couches sans article : normal (articles sur les matériaux).
			table.SetTabItemText (item, 5, FR ("OK"));
		} else {
			table.SetTabItemText (item, 5, FR ("⚠ sans article"));
		}
	}

	if (table.GetItemCount () > 0) {
		const short toSelect = (selectedRuleIndex >= 1 && selectedRuleIndex <= table.GetItemCount ())
			? selectedRuleIndex : 1;
		table.SelectItem (toSelect);
		selectedRuleIndex = toSelect;
		LoadSelectedRuleToControls ();
	} else {
		selectedRuleIndex = 0;
	}

	isFilling = false;
}


void RulesManagerDialog::LoadSelectedRuleToControls ()
{
	if (selectedRuleIndex < 1 || static_cast<UIndex> (selectedRuleIndex) > rules.GetSize ())
		return;
	const CWMapRule& rule = rules[static_cast<UIndex> (selectedRuleIndex) - 1];

	for (short t = 0; t < 4; ++t) {
		if (kPopupTypes[t] == rule.structureType)
			typePopup.SelectItem (static_cast<short> (t + 1));
	}

	structureEdit.SetText (rule.structureName);
	RefreshArticlePopup (rule.articleId);
	modePopup.SelectItem (rule.mode == CWQuantMode::Component ? 2 : 1);
	ignoreCheck.SetState (rule.ignored);
	UpdateModePopupState ();
}


void RulesManagerDialog::ListBoxSelectionChanged (const DG::ListBoxSelectionEvent& ev)
{
	if (ev.GetSource () != &table || isFilling)
		return;

	selectedRuleIndex = table.GetSelectedItem ();
	LoadSelectedRuleToControls ();
}


void RulesManagerDialog::PopUpChanged (const DG::PopUpChangeEvent& ev)
{
	if (ev.GetSource () == &typePopup)
		UpdateModePopupState ();
}


void RulesManagerDialog::ApplyRuleFromControls ()
{
	const GS::UniString structureName = structureEdit.GetText ();
	if (structureName.IsEmpty ()) {
		UpdateStatusText (FR ("Saisissez ou choisissez un nom de structure."));
		return;
	}

	const CWStructureType structureType = SelectedType (typePopup);
	const bool isCompositeOrProfile = (structureType == CWStructureType::Composite
									   || structureType == CWStructureType::Profile);
	const bool byLayers = isCompositeOrProfile && modePopup.GetSelectedItem () == 2;
	const bool ignored = ignoreCheck.IsChecked ();

	// Article : requis sauf en mode « Ses couches » ou « Ignorer ».
	GS::UniString articleId;
	if (!ignored && !byLayers) {
		const short articleSelection = articlePopup.GetSelectedItem ();
		if (articleSelection >= 1 && static_cast<UIndex> (articleSelection) <= articles.GetSize ())
			articleId = articles[static_cast<UIndex> (articleSelection) - 1].id;
		if (articleId.IsEmpty ()) {
			UpdateStatusText (FR ("Choisissez un article (ou « Ses couches » / « Ignorer »)."));
			return;
		}
	}

	CWMapRule rule;
	rule.structureType = structureType;
	rule.structureName = structureName;
	rule.articleId = articleId;
	rule.mode = byLayers ? CWQuantMode::Component : CWQuantMode::Element;
	rule.ignored = ignored;

	// Ajout ou mise à jour (clé = type + nom de structure).
	bool replaced = false;
	for (UIndex r = 0; r < rules.GetSize (); ++r) {
		if (rules[r].structureType == rule.structureType && rules[r].structureName == rule.structureName) {
			rules[r] = rule;
			replaced = true;
			break;
		}
	}
	if (!replaced)
		rules.Push (rule);
	selectedRuleIndex = static_cast<short> (rules.GetSize ());

	FillTable ();
	UpdateStatusText ((replaced ? FR ("Règle mise à jour : ") : FR ("Règle ajoutée : "))
					  + RuleLibrary::StructureTypeName (structureType) + FR (" — ") + rule.structureName);

	// Mode « Ses couches » : proposer immédiatement les articles des couches
	// qui n'en ont pas déjà (spec).
	if (!ignored && byLayers) {
		GS::Array<GS::UniString> layerMaterials;
		if (structureType == CWStructureType::Composite
			&& ModelReader::GetStructureLayerMaterialNames (structureName, layerMaterials)) {
			USize missing = 0;
			for (UIndex m = 0; m < layerMaterials.GetSize (); ++m) {
				const CWMapRule* materialRule = RuleLibrary::FindRule (rules, CWStructureType::BuildingMaterial,
																	  layerMaterials[m]);
				if (materialRule == nullptr || materialRule->articleId.IsEmpty () || materialRule->ignored)
					++missing;
			}
			if (missing > 0) {
				LayersDialog layersDialog (structureName, layerMaterials, articles, rules);
				layersDialog.Invoke ();
				FillTable ();
			} else {
				UpdateStatusText (FR ("Règle « ses couches » enregistrée — toutes les couches ont déjà un article."));
			}
		} else {
			UpdateStatusText (FR ("Règle « ses couches » enregistrée. Les matériaux des couches ")
							  + FR ("s'assignent via le type « Matériau » ou depuis la maquette ")
							  + FR ("(Éléments du projet)."));
		}
	}
}


void RulesManagerDialog::DeleteSelectedRule ()
{
	if (selectedRuleIndex < 1 || static_cast<UIndex> (selectedRuleIndex) > rules.GetSize ())
		return;

	const GS::UniString name = rules[static_cast<UIndex> (selectedRuleIndex) - 1].structureName;
	rules.Delete (static_cast<UIndex> (selectedRuleIndex) - 1);
	selectedRuleIndex = 0;
	FillTable ();
	UpdateStatusText (FR ("Règle supprimée : ") + name);
}


void RulesManagerDialog::BrowseStructure ()
{
	const CWStructureType structureType = SelectedType (typePopup);

	StructurePickerDialog picker (structureType);
	picker.Invoke ();
	if (picker.IsAccepted () && !picker.GetSelectedName ().IsEmpty ()) {
		structureEdit.SetText (picker.GetSelectedName ());
		UpdateStatusText (FR ("Structure choisie : ") + picker.GetSelectedName ());
	}
}


void RulesManagerDialog::ImportBase ()
{
	DG::FileDialog fileDialog (DG::FileDialog::OpenFile);
	fileDialog.SetTitle (FR ("Importer la base d'articles CostWaves (JSON)"));

	if (!fileDialog.Invoke ())
		return;

	GS::UniString path;
	if (fileDialog.GetSelectedFile ().ToPath (&path) != NoError || path.IsEmpty ()) {
		DG::WarningAlert (FR ("Import de la base"), FR ("Impossible de récupérer le fichier choisi."), FR ("OK"));
		return;
	}

	GS::Array<CWArticle> imported;
	GS::UniString error;
	if (!ArticleManager::ImportFromJsonFile (path, imported, error)) {
		DG::ErrorAlert (FR ("Échec de l'import de la base."), error, FR ("OK"));
		return;
	}

	articles = imported;

	// Persister dans Documents pour les prochaines sessions.
	GS::UniString saveError;
	if (!ArticleManager::SaveArticleBase (articles, saveError))
		DG::WarningAlert (FR ("Base importée, mais non enregistrée."), saveError, FR ("OK"));

	RefreshArticlePopup (GS::UniString ());
	FillTable ();
	UpdateStatusText (GS::ToUniString (std::to_wstring (static_cast<int> (articles.GetSize ())))
					  + FR (" article(s) chargés — base enregistrée dans Documents."));
}


void RulesManagerDialog::CreateArticle ()
{
	ArticleEditDialog editDialog;
	editDialog.Invoke ();
	if (!editDialog.IsAccepted ())
		return;

	const CWArticle article = editDialog.GetArticle ();
	GS::UniString saveError;
	if (!ArticleManager::SaveLocalArticle (article, saveError))
		DG::WarningAlert (FR ("Article créé, mais non enregistré."), saveError, FR ("OK"));

	// Fusionner à la base courante (remplace l'id s'il existe).
	bool replaced = false;
	for (UIndex a = 0; a < articles.GetSize (); ++a) {
		if (articles[a].id == article.id) {
			articles[a] = article;
			replaced = true;
			break;
		}
	}
	if (!replaced)
		articles.Push (article);

	RefreshArticlePopup (article.id);
	FillTable ();
	UpdateStatusText (FR ("Article créé : ") + article.id + FR (" — ") + article.name);
}


bool RulesManagerDialog::SaveLibrary ()
{
	GS::UniString error;
	if (!RuleLibrary::SaveRules (rules, error)) {
		DG::ErrorAlert (FR ("Échec de l'enregistrement de la bibliothèque."), error, FR ("OK"));
		return false;
	}
	return true;
}


void RulesManagerDialog::UpdateStatusText (const GS::UniString& message)
{
	statusText.SetText (message);
}


void RulesManagerDialog::ButtonClicked (const DG::ButtonClickEvent& ev)
{
	if (ev.GetSource () == &applyButton) {
		ApplyRuleFromControls ();
	} else if (ev.GetSource () == &deleteButton) {
		DeleteSelectedRule ();
	} else if (ev.GetSource () == &browseButton) {
		BrowseStructure ();
	} else if (ev.GetSource () == &importButton) {
		ImportBase ();
	} else if (ev.GetSource () == &createArticleButton) {
		CreateArticle ();
	} else if (ev.GetSource () == &saveButton) {
		if (SaveLibrary ())
			UpdateStatusText (FR ("Bibliothèque enregistrée (Documents/CostWaves-regles.json)."));
	} else if (ev.GetSource () == &closeButton) {
		SaveLibrary ();
		PostCloseRequest (DG::ModalDialog::Accept);
	}
}

} // namespace CostWaves
