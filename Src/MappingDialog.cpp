#include "CostWavesPrecompiledHeader.hpp"

#include "MappingDialog.hpp"

#include "ArticleManager.hpp"
#include "ArticleEditDialog.hpp"
#include "RuleLibrary.hpp"

namespace CostWaves {

namespace {

// Littéral UTF-8 -> GS::UniString (les sources sont compilees avec /utf-8).
GS::UniString FR (const char* utf8Text)
{
	return GS::UniString (utf8Text, CC_UTF8);
}

// Sélecteur de structure : réutilise la ressource du dialogue « Article »
// (texte d'info + popup + deux boutons) pour lister les structures disponibles
// d'un type donné dans l'environnement Archicad courant.
class StructurePickerDialog final :\tpublic DG::ModalDialog,
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
		SetTitle (FR ("CostWaves — Choisir une structure"));
		structureLabel.SetText (FR ("Structure :"));
		chooseButton.SetText (FR ("Choisir"));

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

		chooseButton.Attach (*this);
		cancelButton.Attach (*this);
	}

	bool				IsAccepted () const { return accepted; }
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

} // namespace


MappingDialog::MappingDialog (const GS::Array<CWArticle>& inArticles, const GS::Array<CWMapRule>& inRules)
	:	DG::ModalDialog (ACAPI_GetOwnResModule (), ID_ADDON_DLG_MAPPING, ACAPI_GetOwnResModule ()),
		infoText (GetReference (), InfoTextId),
		table (GetReference (), TableId),
		typeLabel (GetReference (), TypeLabelId),
		typePopup (GetReference (), TypePopupId),
		structureLabel (GetReference (), StructureLabelId),
		structureEdit (GetReference (), StructureEditId),
		browseButton (GetReference (), BrowseButtonId),
		articleLabel (GetReference (), ArticleLabelId),
		articlePopup (GetReference (), ArticlePopupId),
		modeLabel (GetReference (), ModeLabelId),
		modePopup (GetReference (), ModePopupId),
		quantityLabel (GetReference (), QuantityLabelId),
		quantityEdit (GetReference (), QuantityEditId),
		ignoreCheck (GetReference (), IgnoreCheckId),
		applyButton (GetReference (), ApplyButtonId),
		deleteButton (GetReference (), DeleteButtonId),
		statusText (GetReference (), StatusTextId),
		createArticleButton (GetReference (), CreateArticleButtonId),
		saveButton (GetReference (), SaveButtonId),
		closeButton (GetReference (), CloseButtonId),
		rules (inRules),
		articles (inArticles)
{
	infoText.SetText (FR ("Reliez les structures Archicad (matériaux, composites, profils, favoris, objets de ")
					  + FR ("bibliothèque) aux articles CostWaves et choisissez le mode de métré. ")
					  + FR ("La bibliothèque est enregistrée dans Documents et réutilisable entre les projets."));

	// Popup des types de structures.
	const CWStructureType types[5] = { CWStructureType::Composite,
									   CWStructureType::BuildingMaterial,
									   CWStructureType::Profile,
									   CWStructureType::LibraryPart,
									   CWStructureType::Favorite };
	for (short t = 0; t < 5; ++t) {
		typePopup.AppendItem ();
		typePopup.SetItemText (static_cast<short> (t + 1), RuleLibrary::StructureTypeName (types[t]));
	}
	typePopup.SelectItem (1);

	// Mode de métré (spec §11) : élément ou composants — jamais les deux.
	modePopup.AppendItem ();
	modePopup.SetItemText (1, FR ("Élément"));
	modePopup.AppendItem ();
	modePopup.SetItemText (2, FR ("Composants (skins)"));
	modePopup.SelectItem (1);

	RefreshArticlePopup (GS::UniString ());
	FillTable ();

	applyButton.Attach (*this);
	deleteButton.Attach (*this);
	browseButton.Attach (*this);
	createArticleButton.Attach (*this);
	saveButton.Attach (*this);
	closeButton.Attach (*this);
	table.Attach (*this);
	typePopup.Attach (*this);
}


void MappingDialog::RefreshArticlePopup (const GS::UniString& preferredArticleId)
{
	isFilling = true;

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

	isFilling = false;
}


void MappingDialog::FillTable ()
{
	isFilling = true;

	const short columnCount = 6;
	table.SetHeaderItemCount (columnCount);
	table.SetTabFieldCount (columnCount);
	table.SetHeaderItemText (1, FR ("Type"));
	table.SetHeaderItemText (2, FR ("Structure"));
	table.SetHeaderItemText (3, FR ("Article"));
	table.SetHeaderItemText (4, FR ("Mode"));
	table.SetHeaderItemText (5, FR ("Quantité"));
	table.SetHeaderItemText (6, FR ("État"));

	const short widths[6] = { 130, 190, 170, 90, 120, 130 };
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

		// État (spec §9) : l'article référencé existe-t-il toujours dans la base ?
		GS::UniString state;
		if (rule.ignored) {
			state = FR ("Ignoré");
		} else if (ArticleManager::FindArticle (articles, rule.articleId) != nullptr) {
			state = FR ("OK");
		} else {
			state = FR ("⚠ Article introuvable");
		}

		if (rule.ignored) {
			table.SetTabItemText (item, 3, FR ("—"));
			table.SetTabItemText (item, 4, FR ("—"));
			table.SetTabItemText (item, 5, FR ("—"));
		} else {
			const CWArticle* article = ArticleManager::FindArticle (articles, rule.articleId);
			table.SetTabItemText (item, 3, article != nullptr
				? rule.articleId + FR (" — ") + article->name
				: rule.articleId);
			table.SetTabItemText (item, 4, rule.mode == CWQuantMode::Component
				? FR ("Composants") : FR ("Élément"));
			table.SetTabItemText (item, 5, rule.quantity.IsEmpty () ? FR ("Automatique") : rule.quantity);
		}
		table.SetTabItemText (item, 6, state);
	}

	if (table.GetItemCount () > 0) {
		table.SelectItem (1);
		selectedRuleIndex = 1;
		LoadSelectedRuleToControls ();
	} else {
		selectedRuleIndex = 0;
	}

	isFilling = false;
}


void MappingDialog::LoadSelectedRuleToControls ()
{
	if (selectedRuleIndex < 1 || static_cast<UIndex> (selectedRuleIndex) > rules.GetSize ())
		return;
	const CWMapRule& rule = rules[static_cast<UIndex> (selectedRuleIndex) - 1];

	// Type : les 5 types dans l'ordre du popup (Composite, Matériau, Profil,
	// Objet de bibliothèque, Favori).
	const CWStructureType types[5] = { CWStructureType::Composite,
									   CWStructureType::BuildingMaterial,
									   CWStructureType::Profile,
									   CWStructureType::LibraryPart,
									   CWStructureType::Favorite };
	for (short t = 0; t < 5; ++t) {
		if (types[t] == rule.structureType)
			typePopup.SelectItem (static_cast<short> (t + 1));
	}

	structureEdit.SetText (rule.structureName);
	RefreshArticlePopup (rule.articleId);
	modePopup.SelectItem (rule.mode == CWQuantMode::Component ? 2 : 1);
	quantityEdit.SetText (rule.quantity);
	ignoreCheck.SetState (rule.ignored);
}


void MappingDialog::ListBoxSelectionChanged (const DG::ListBoxSelectionEvent& ev)
{
	if (ev.GetSource () != &table || isFilling)
		return;

	selectedRuleIndex = table.GetSelectedItem ();
	LoadSelectedRuleToControls ();
}


void MappingDialog::PopUpChanged (const DG::PopUpChangeEvent& ev)
{
	// Rien à propager en direct : les valeurs sont lues à l'application.
	(void) ev;
}


void MappingDialog::ApplyRuleFromControls ()
{
	const GS::UniString structureName = structureEdit.GetText ();
	if (structureName.IsEmpty ()) {
		UpdateStatusText (FR ("Saisissez ou choisissez un nom de structure."));
		return;
	}

	const CWStructureType types[5] = { CWStructureType::Composite,
									   CWStructureType::BuildingMaterial,
									   CWStructureType::Profile,
									   CWStructureType::LibraryPart,
									   CWStructureType::Favorite };
	const short typeSelection = typePopup.GetSelectedItem ();
	const CWStructureType structureType = types[typeSelection >= 1 && typeSelection <= 5 ? typeSelection - 1 : 0];

	const bool ignored = ignoreCheck.IsChecked ();
	GS::UniString articleId;
	if (!ignored) {
		const short articleSelection = articlePopup.GetSelectedItem ();
		if (articleSelection >= 1 && static_cast<UIndex> (articleSelection) <= articles.GetSize ())
			articleId = articles[static_cast<UIndex> (articleSelection) - 1].id;
		if (articleId.IsEmpty ()) {
			UpdateStatusText (FR ("Choisissez un article (ou cochez « Ignorer »)."));
			return;
		}
	}

	CWMapRule rule;
	rule.structureType = structureType;
	rule.structureName = structureName;
	rule.articleId = articleId;
	rule.mode = (modePopup.GetSelectedItem () == 2) ? CWQuantMode::Component : CWQuantMode::Element;
	rule.quantity = quantityEdit.GetText ();
	rule.ignored = ignored;

	// Ajout ou mise à jour (clé = type + nom de structure, spec §8).
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

	FillTable ();
	UpdateStatusText (replaced ? FR ("Règle mise à jour : ") + rule.structureName
								: FR ("Règle ajoutée : ") + rule.structureName);
}


void MappingDialog::DeleteSelectedRule ()
{
	if (selectedRuleIndex < 1 || static_cast<UIndex> (selectedRuleIndex) > rules.GetSize ())
		return;

	const GS::UniString name = rules[static_cast<UIndex> (selectedRuleIndex) - 1].structureName;
	rules.Delete (static_cast<UIndex> (selectedRuleIndex) - 1);
	selectedRuleIndex = 0;
	FillTable ();
	UpdateStatusText (FR ("Règle supprimée : ") + name);
}


bool MappingDialog::SaveLibrary ()
{
	GS::UniString error;
	if (!RuleLibrary::SaveRules (rules, error)) {
		DG::ErrorAlert (FR ("Échec de l'enregistrement de la bibliothèque."), error, FR ("OK"));
		return false;
	}
	return true;
}


void MappingDialog::UpdateStatusText (const GS::UniString& message)
{
	statusText.SetText (message);
}


void MappingDialog::ButtonClicked (const DG::ButtonClickEvent& ev)
{
	if (ev.GetSource () == &applyButton) {
		ApplyRuleFromControls ();
	} else if (ev.GetSource () == &deleteButton) {
		DeleteSelectedRule ();
	} else if (ev.GetSource () == &browseButton) {
		// « Parcourir… » : structures du type choisi dans l'environnement
		// Archicad courant (sans maquette : saisie manuelle).
		const CWStructureType types[5] = { CWStructureType::Composite,
										   CWStructureType::BuildingMaterial,
										   CWStructureType::Profile,
										   CWStructureType::LibraryPart,
										   CWStructureType::Favorite };
		const short typeSelection = typePopup.GetSelectedItem ();
		const CWStructureType structureType = types[typeSelection >= 1 && typeSelection <= 5 ? typeSelection - 1 : 0];

		StructurePickerDialog picker (structureType);
		picker.Invoke ();
		if (picker.IsAccepted () && !picker.GetSelectedName ().IsEmpty ()) {
			structureEdit.SetText (picker.GetSelectedName ());
			UpdateStatusText (FR ("Structure choisie : ") + picker.GetSelectedName ());
		}
	} else if (ev.GetSource () == &createArticleButton) {
		// Créer un article depuis Archicad (spec §10) : local, réutilisable.
		ArticleEditDialog editDialog;
		editDialog.Invoke ();
		if (editDialog.IsAccepted ()) {
			const CWArticle article = editDialog.GetArticle ();
			if (ArticleManager::FindArticle (articles, article.id) == nullptr)
				articles.Push (article);
			GS::UniString saveError;
			if (!ArticleManager::SaveLocalArticle (article, saveError))
				DG::WarningAlert (FR ("Article créé localement, mais non enregistré."), saveError, FR ("OK"));
			RefreshArticlePopup (article.id);
			UpdateStatusText (FR ("Article créé : ") + article.id + FR (" — ") + article.name);
		}
	} else if (ev.GetSource () == &saveButton) {
		if (SaveLibrary ())
			UpdateStatusText (FR ("Bibliothèque enregistrée (CostWaves-regles.json)."));
	} else if (ev.GetSource () == &closeButton) {
		// Fermer enregistre la bibliothèque (spec §8 : les modifications
		// s'appliquent aux futurs projets).
		SaveLibrary ();
		accepted = true;
		PostCloseRequest (DG::ModalDialog::Accept);
	}
}

} // namespace CostWaves
