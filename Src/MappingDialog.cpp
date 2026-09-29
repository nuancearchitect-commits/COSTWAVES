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

// Types du filtre, dans l'ordre du popup.
const CWStructureType kTypes[3] = {
	CWStructureType::BuildingMaterial,
	CWStructureType::Composite,
	CWStructureType::Profile
};

// Indentation hiérarchique du sélecteur (style classification Archicad).
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
		typeLabel (GetReference (), TypeLabelId),
		typePopup (GetReference (), TypePopupId),
		systemLabel (GetReference (), SystemLabelId),
		systemPopup (GetReference (), SystemPopupId),
		list (GetReference (), ListId),
		selectorLabel (GetReference (), SelectorLabelId),
		articlePopup (GetReference (), ArticlePopupId),
		saveButton (GetReference (), SaveButtonId),
		closeButton (GetReference (), CloseButtonId)
{
	infoText.SetText (FR ("Choisissez le type d'attribut et le système de classification ")
					  + FR ("(les classes = articles), puis affectez un article ou « Ignorer » ")
					  + FR ("à chaque attribut."));

	// Filtre type d'attribut.
	typePopup.AppendItem ();
	typePopup.SetItemText (1, FR ("Matériau"));
	typePopup.AppendItem ();
	typePopup.SetItemText (2, FR ("Composite"));
	typePopup.AppendItem ();
	typePopup.SetItemText (3, FR ("Profil complexe"));
	typePopup.SelectItem (1);

	// Filtre système de classification.
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

	// Bibliothèque de règles existante (Documents/CostWaves-regles.json).
	GS::UniString rulesError;
	RuleLibrary::LoadRules (rules, rulesError);		// absente = bibliothèque vide

	RefreshArticles ();		// classes du système sélectionné -> sélecteur
	RefreshAttributes ();	// attributs du type choisi -> liste

	saveButton.Attach (*this);
	closeButton.Attach (*this);
	typePopup.Attach (*this);
	systemPopup.Attach (*this);
	articlePopup.Attach (*this);
	list.Attach (*this);
}


CWStructureType MappingDialog::CurrentType () const
{
	const short selection = typePopup.GetSelectedItem ();
	if (selection >= 1 && selection <= 3)
		return kTypes[selection - 1];
	return CWStructureType::BuildingMaterial;
}


void MappingDialog::RefreshAttributes ()
{
	isFilling = true;
	attributes.Clear ();
	RuleLibrary::CollectAvailableStructures (CurrentType (), attributes);
	isFilling = false;

	selectedAttribute = 0;
	FillList ();
}


void MappingDialog::RefreshArticles ()
{
	isFilling = true;

	articles.Clear ();
	const short systemSelection = systemPopup.GetSelectedItem ();
	if (systemSelection >= 1 && static_cast<UIndex> (systemSelection) <= systems.GetSize ())
		ArticleManager::CollectFromClassification (systems[static_cast<UIndex> (systemSelection) - 1].guid,
												   articles);

	// Sélecteur d'article (style sélecteur d'attributs Archicad) :
	// « — », « Ignorer », puis les classes indentées par profondeur.
	while (articlePopup.GetItemCount () > 0)
		articlePopup.DeleteItem (1);

	articlePopup.AppendItem ();
	articlePopup.SetItemText (1, FR ("— (aucun)"));
	articlePopup.AppendItem ();
	articlePopup.SetItemText (2, FR ("Ignorer"));
	for (UIndex a = 0; a < articles.GetSize (); ++a) {
		articlePopup.AppendItem ();
		articlePopup.SetItemText (articlePopup.GetItemCount (),
								  Indent (articles[a].depth) + articles[a].id + FR (" — ") + articles[a].name);
	}
	articlePopup.SelectItem (1);

	isFilling = false;
}


void MappingDialog::FillList ()
{
	isFilling = true;

	const short columnCount = 2;
	list.SetHeaderItemCount (columnCount);
	list.SetTabFieldCount (columnCount);
	list.SetHeaderItemText (1, FR ("Attribut"));
	list.SetHeaderItemText (2, FR ("Article (classe)"));

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

	for (UIndex a = 0; a < attributes.GetSize (); ++a) {
		list.AppendItem ();
		const short item = list.GetItemCount ();
		list.SetTabItemText (item, 1, attributes[a]);

		const CWMapRule* rule = RuleLibrary::FindRule (rules, CurrentType (), attributes[a]);
		if (rule != nullptr && rule->ignored) {
			list.SetTabItemText (item, 2, FR ("Ignorer"));
		} else if (rule != nullptr && !rule->articleId.IsEmpty ()) {
			const CWArticle* article = ArticleManager::FindArticle (articles, rule->articleId);
			list.SetTabItemText (item, 2, article != nullptr
				? rule->articleId + FR (" — ") + article->name
				: rule->articleId);
		} else {
			list.SetTabItemText (item, 2, FR ("—"));
		}
	}

	if (list.GetItemCount () > 0) {
		if (selectedAttribute < 1 || static_cast<UIndex> (selectedAttribute) > list.GetItemCount ())
			selectedAttribute = 1;
		list.SelectItem (selectedAttribute);
	} else {
		selectedAttribute = 0;
	}

	isFilling = false;

	RefreshSelectorForSelection ();
}


void MappingDialog::RefreshSelectorForSelection ()
{
	if (selectedAttribute < 1 || static_cast<UIndex> (selectedAttribute) > attributes.GetSize ()) {
		selectorLabel.SetText (FR ("Sélectionnez un attribut dans la liste."));
		if (articlePopup.GetItemCount () > 0)
			articlePopup.SelectItem (1);
		return;
	}

	const GS::UniString& attributeName = attributes[static_cast<UIndex> (selectedAttribute) - 1];
	selectorLabel.SetText (FR ("Article de « ") + attributeName + FR (" » :"));

	// Positionner le sélecteur sur la règle courante de cet attribut.
	const CWMapRule* rule = RuleLibrary::FindRule (rules, CurrentType (), attributeName);
	short item = 1;		// « — (aucun) »
	if (rule != nullptr) {
		if (rule->ignored) {
			item = 2;	// « Ignorer »
		} else if (!rule->articleId.IsEmpty ()) {
			for (UIndex a = 0; a < articles.GetSize (); ++a) {
				if (articles[a].id == rule->articleId) {
					item = static_cast<short> (a + 3);
					break;
				}
			}
		}
	}
	if (item > articlePopup.GetItemCount ())
		item = 1;
	articlePopup.SelectItem (item);
}


void MappingDialog::ApplyArticleSelection ()
{
	if (selectedAttribute < 1 || static_cast<UIndex> (selectedAttribute) > attributes.GetSize ())
		return;

	const GS::UniString attributeName = attributes[static_cast<UIndex> (selectedAttribute) - 1];
	const CWStructureType structureType = CurrentType ();
	const short selection = articlePopup.GetSelectedItem ();

	if (selection == 1) {
		// « — (aucun) » : retirer la règle.
		for (UIndex r = 0; r < rules.GetSize (); ++r) {
			if (rules[r].structureType == structureType && rules[r].structureName == attributeName) {
				rules.Delete (r);
				break;
			}
		}
		SetStatus (FR ("« ") + attributeName + FR (" » : sans correspondance."));
	} else if (selection == 2) {
		// « Ignorer ».
		CWMapRule rule;
		rule.structureType = structureType;
		rule.structureName = attributeName;
		rule.ignored = true;
		bool replaced = false;
		for (UIndex r = 0; r < rules.GetSize (); ++r) {
			if (rules[r].structureType == structureType && rules[r].structureName == attributeName) {
				rules[r] = rule;
				replaced = true;
				break;
			}
		}
		if (!replaced)
			rules.Push (rule);
		SetStatus (FR ("« ") + attributeName + FR (" » : ignoré."));
	} else if (selection >= 3 && static_cast<UIndex> (selection - 2) <= articles.GetSize ()) {
		const CWArticle& article = articles[static_cast<UIndex> (selection - 3)];
		CWMapRule rule;
		rule.structureType = structureType;
		rule.structureName = attributeName;
		rule.articleId = article.id;
		rule.mode = CWQuantMode::Element;
		bool replaced = false;
		for (UIndex r = 0; r < rules.GetSize (); ++r) {
			if (rules[r].structureType == structureType && rules[r].structureName == attributeName) {
				rules[r] = rule;
				replaced = true;
				break;
			}
		}
		if (!replaced)
			rules.Push (rule);
		SetStatus (FR ("« ") + attributeName + FR (" » → ") + article.id + FR (" — ") + article.name);
	}

	FillList ();
}


void MappingDialog::SetStatus (const GS::UniString& message)
{
	infoText.SetText (message);
}


void MappingDialog::ListBoxSelectionChanged (const DG::ListBoxSelectionEvent& ev)
{
	if (ev.GetSource () != &list || isFilling)
		return;

	selectedAttribute = list.GetSelectedItem ();
	RefreshSelectorForSelection ();
}


void MappingDialog::PopUpChanged (const DG::PopUpChangeEvent& ev)
{
	if (isFilling)
		return;

	if (ev.GetSource () == &typePopup) {
		RefreshAttributes ();
	} else if (ev.GetSource () == &systemPopup) {
		RefreshArticles ();
		FillList ();
	} else if (ev.GetSource () == &articlePopup) {
		ApplyArticleSelection ();
	}
}


void MappingDialog::ButtonClicked (const DG::ButtonClickEvent& ev)
{
	if (ev.GetSource () == &saveButton) {
		GS::UniString error;
		if (RuleLibrary::SaveRules (rules, error)) {
			SetStatus (FR ("Correspondances enregistrées (Documents/CostWaves-regles.json)."));
		} else {
			DG::ErrorAlert (FR ("Échec de l'enregistrement."), error, FR ("OK"));
		}
	} else if (ev.GetSource () == &closeButton) {
		// Fermer enregistre la bibliothèque (silencieux, best effort).
		GS::UniString error;
		RuleLibrary::SaveRules (rules, error);
		PostCloseRequest (DG::ModalDialog::Accept);
	}
}

} // namespace CostWaves
