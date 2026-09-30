#include "CostWavesPrecompiledHeader.hpp"

#include "InheritedArticlesDialog.hpp"

#include "ArticleManager.hpp"
#include "ArticlePickerDialog.hpp"
#include "CostWavesStyle.hpp"
#include "FormulaEditorDialog.hpp"
#include "GdlItemPickerDialog.hpp"
#include "KeyCatalog.hpp"
#include "ModelReader.hpp"
#include "RuleLibrary.hpp"

#include "UniStringWStringConversion.hpp"

namespace CostWaves {

namespace {

GS::UniString FR (const char* utf8Text)
{
	return GS::UniString (utf8Text, CC_UTF8);
}

// Les règles d'articles hérités partagent le type « objet GDL » mais se
// distinguent par leur clé « bool:<nom GDL du paramètre> ».
const char* kBoolKeyPrefix = "bool:";

} // namespace


InheritedArticlesDialog::InheritedArticlesDialog ()
	:	DG::ModalDialog (ACAPI_GetOwnResModule (), ID_ADDON_DLG_INHERITED, ACAPI_GetOwnResModule ()),
		infoText (GetReference (), InfoTextId),
		list (GetReference (), ListId),
		statusText (GetReference (), StatusTextId),
		addButton (GetReference (), AddButtonId),
		closeButton (GetReference (), CloseButtonId),
		systemLabel (GetReference (), SystemLabelId),
		systemPopup (GetReference (), SystemPopupId),
		deleteButton (GetReference (), DeleteButtonId)
{
	infoText.SetText (FR ("Un article hérité naît d'un paramètre BOOLÉEN activé (tablette, seuil,")
					  + FR (" volet…), quel que soit l'objet qui le porte. « Ajouter… » :")
					  + FR (" choisir un objet pour lister ses paramètres, puis le booléen,")
					  + FR (" puis l'article."));
	CostWavesStyle::ApplyHelp (infoText);
	CostWavesStyle::ApplyFieldLabel (systemLabel);
	CostWavesStyle::ApplyStatusChip (statusText);

	// Système de classification (les classes = articles proposés).
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
	FillList ();

	addButton.Attach (*this);
	closeButton.Attach (*this);
	systemPopup.Attach (*this);
	list.Attach (*this);
	deleteButton.Attach (*this);
}


void InheritedArticlesDialog::RefreshArticles ()
{
	articles.Clear ();
	const short systemSelection = systemPopup.GetSelectedItem ();
	if (systemSelection >= 1 && static_cast<UIndex> (systemSelection) <= systems.GetSize ())
		ArticleManager::CollectFromClassification (systems[static_cast<UIndex> (systemSelection) - 1].guid,
												   articles);
}


void InheritedArticlesDialog::FillList ()
{
	isFilling = true;

	// Colonnes : Paramètre (booléen) | Article hérité | Valeur clé | Unité |
	// Qté — PAS de mode de calcul ni de déductions pour les articles
	// hérités : la quantité est une FORMULE composée depuis les paramètres
	// GDL de l'objet porteur et les quantités Archicad.
	const short columnCount = 5;
	list.SetHeaderItemCount (columnCount);
	list.SetTabFieldCount (columnCount);
	list.SetHeaderItemText (1, FR ("Paramètre (booléen)"));
	list.SetHeaderItemText (2, FR ("Article hérité"));
	list.SetHeaderItemText (3, FR ("Valeur clé"));
	list.SetHeaderItemText (4, FR ("Unité"));
	list.SetHeaderItemText (5, FR ("Qté"));

	const short widths[5] = { 150, 160, 110, 56, 284 };
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
	visibleRules.Clear ();

	USize count = 0;
	for (UIndex r = 0; r < rules.GetSize (); ++r) {
		if (rules[r].structureType != CWStructureType::LibraryPartBool)
			continue;
		++count;

		list.AppendItem ();
		const short item = list.GetItemCount ();
		list.SetTabItemText (item, 1, rules[r].keyName.IsEmpty ()
			? rules[r].keyId : rules[r].keyName);

		const CWArticle* article = ArticleManager::FindArticle (articles, rules[r].articleId);
		list.SetTabItemText (item, 2, article != nullptr
			? rules[r].articleId + FR (" — ") + article->name
			: rules[r].articleId);
		CostWavesStyle::CellAccent (list, item, 2);

		list.SetTabItemText (item, 3, rules[r].valueKeyName.IsEmpty ()
			? FR ("—") : rules[r].valueKeyName);
		if (rules[r].valueKeyName.IsEmpty ())
			CostWavesStyle::CellMuted (list, item, 3);
		else
			CostWavesStyle::CellOk (list, item, 3);

		list.SetTabItemText (item, 4, CWUnitDisplay (rules[r].unit));
		list.SetTabItemText (item, 5, rules[r].quantityFormula.IsEmpty ()
			? FR ("— (auto)") : rules[r].quantityFormula);
		if (rules[r].quantityFormula.IsEmpty ())
			CostWavesStyle::CellMuted (list, item, 5);
		else
			CostWavesStyle::CellOk (list, item, 5);

		visibleRules.Push (r + 1);
	}

	if (selectedRule < 1 || static_cast<UIndex> (selectedRule) > visibleRules.GetSize ())
		selectedRule = visibleRules.GetSize () > 0 ? 1 : 0;
	if (selectedRule >= 1)
		list.SelectItem (selectedRule);

	isFilling = false;

	SetStatus (GS::ToUniString (std::to_wstring (static_cast<int> (count)))
			   + FR (" article(s) hérité(s)."));
}


void InheritedArticlesDialog::AddRule ()
{
	// 1) Un objet de bibliothèque — SIMPLE NAVIGATEUR pour découvrir les
	//    noms de booléens : la règle finale est GLOBALE (le même booléen
	//    se répète dans plusieurs objets).
	GS::Array<GS::UniString> objectNames;
	RuleLibrary::CollectAvailableStructures (CWStructureType::LibraryPart, objectNames);
	if (objectNames.IsEmpty ()) {
		SetStatus (FR ("Aucun objet de bibliothèque posable n'est chargé."));
		return;
	}

	GS::Array<CWGdlParam> objectItems;
	for (UIndex o = 0; o < objectNames.GetSize (); ++o) {
		CWGdlParam object;
		object.label = objectNames[o];
		objectItems.Push (object);
	}

	GdlItemPickerDialog objectPicker (ID_ADDON_DLG_OBJPICKER, FR ("Objet (pour lister ses paramètres)"), FR (""),
									  objectItems, false);
	objectPicker.Invoke ();
	if (!objectPicker.IsAccepted ())
		return;

	const short objectIndex = objectPicker.GetSelectedItemIndex ();
	if (objectIndex < 1 || static_cast<UIndex> (objectIndex) > objectNames.GetSize ())
		return;
	const GS::UniString objectName = objectNames[static_cast<UIndex> (objectIndex) - 1];

	// 2) Le paramètre BOOLÉEN de l'objet — celui dont l'activation fait
	//    naître l'article hérité (tablette, seuil, volet…). Le lecteur
	//    renvoie TOUS les paramètres avec leur type ; le DÉCLENCHEUR doit
	//    être un booléen -> filtre côté dialogue (colonne Type visible).
	GS::Array<CWGdlParam> allParams;
	GS::UniString paramNote;
	GS::UniString paramAlert;
	if (!ModelReader::GetLibraryPartBooleanParameters (objectName, allParams, paramNote, paramAlert)) {
		SetStatus (FR ("« ") + objectName + FR (" » : ") + paramNote);
		return;
	}
	if (!paramAlert.IsEmpty ())
		DG::WarningAlert (FR ("Paramètres booléens limités pour « ") + objectName + FR (" »"),
						  paramAlert, FR ("OK"));

	GS::Array<CWGdlParam> params;
	for (UIndex p = 0; p < allParams.GetSize (); ++p) {
		if (allParams[p].type == FR ("bool"))
			params.Push (allParams[p]);
	}
	if (params.IsEmpty ()) {
		SetStatus (FR ("« ") + objectName + FR (" » : aucun paramètre booléen (") + paramNote + FR (")"));
		return;
	}

	GdlItemPickerDialog paramPicker (ID_ADDON_DLG_PARAMPICKER, FR ("Paramètre booléen"), FR ("Nom GDL"),
									 params, false, FR ("bool"));
	paramPicker.Invoke ();
	if (!paramPicker.IsAccepted ())
		return;

	const short paramIndex = paramPicker.GetSelectedItemIndex ();
	if (paramIndex < 1 || static_cast<UIndex> (paramIndex) > params.GetSize ())
		return;
	const GS::UniString paramName = params[static_cast<UIndex> (paramIndex) - 1].name;		// nom GDL stable
	const GS::UniString paramLabel = params[static_cast<UIndex> (paramIndex) - 1].label;	// libellé lisible
	const GS::UniString boolKey = FR (kBoolKeyPrefix) + paramName;

	// 3) L'article hérité — « (aucune) » retire la règle.
	if (articles.IsEmpty ()) {
		SetStatus (FR ("Aucune classe — choisissez un système de classification."));
		return;
	}

	ArticlePickerDialog articlePicker (articles);
	articlePicker.Invoke ();
	if (!articlePicker.IsAccepted ())
		return;

	const short articleIndex = articlePicker.GetSelectedArticleIndex ();

	// 4) La valeur clé (facultative) : une variable GDL de TYPE LONGUEUR de
	//    l'objet — comme dans les correspondances objets GDL, elle
	//    différencie les variantes de l'article hérité (Ø125/Ø160, H8/H12…).
	//    « (aucune) » ou Annuler = sans clé (l'article hérité reste).
	GS::UniString valueKeyId;
	GS::UniString valueKeyName;
	if (articleIndex != 0) {
		// TOUS les paramètres GDL avec leur type (colonne Type, double
		// vérification), longueurs en tête et en gras.
		GS::Array<CWGdlParam> lengthParams;
		GS::UniString lengthNote;
		GS::UniString lengthAlert;
		if (ModelReader::GetLibraryPartParameters (objectName, lengthParams, lengthNote, lengthAlert)) {
			if (!lengthAlert.IsEmpty ())
				DG::WarningAlert (FR ("Valeurs clés limitées pour « ") + objectName + FR (" »"),
								  lengthAlert, FR ("OK"));
			if (!lengthParams.IsEmpty ()) {
				GdlItemPickerDialog valuePicker (ID_ADDON_DLG_PARAMPICKER, FR ("Valeur clé"), FR ("Nom GDL"),
												 lengthParams, true, FR ("longueur"));
				valuePicker.Invoke ();
				if (valuePicker.IsAccepted ()) {
					const short valueIndex = valuePicker.GetSelectedItemIndex ();
					if (valueIndex >= 1 && static_cast<UIndex> (valueIndex) <= lengthParams.GetSize ()) {
						valueKeyId = lengthParams[static_cast<UIndex> (valueIndex) - 1].name;		// nom GDL stable
						valueKeyName = lengthParams[static_cast<UIndex> (valueIndex) - 1].label;	// libellé lisible
					}
				}
			}
		}
	}

	if (articleIndex == 0) {
		for (UIndex r = 0; r < rules.GetSize (); ++r) {
			if (rules[r].structureType == CWStructureType::LibraryPartBool
				&& rules[r].keyId == boolKey) {
				rules.Delete (r);
				break;
			}
		}
		SetStatus (FR ("« ") + paramLabel + FR (" » : article hérité retiré."));
	} else {
		const CWArticle& article = articles[static_cast<UIndex> (articleIndex) - 1];

		CWMapRule rule;
		rule.structureType = CWStructureType::LibraryPartBool;
		rule.structureName = objectName;	// documentaire seulement : la règle est GLOBALE
		rule.articleId = article.id;
		rule.mode = CWQuantMode::Element;
		rule.keyId = boolKey;
		rule.keyName = paramLabel;
		rule.valueKeyId = valueKeyId;
		rule.valueKeyName = valueKeyName;

		// Règle unique par booléen, quel que soit l'objet où il a été
		// découvert : l'appariement ignore structureName.
		bool replaced = false;
		for (UIndex r = 0; r < rules.GetSize (); ++r) {
			if (rules[r].structureType == CWStructureType::LibraryPartBool
				&& rules[r].keyId == boolKey) {
				rules[r] = rule;
				replaced = true;
				break;
			}
		}
		if (!replaced)
			rules.Push (rule);

		SetStatus (FR ("« ") + paramLabel + FR (" » activé → ") + article.id + FR (" — ") + article.name
				   + FR (" — tout objet ayant ce paramètre")
				   + (valueKeyName.IsEmpty () ? GS::UniString () : FR (" · clé : ") + valueKeyName));
	}

	FillList ();
}


void InheritedArticlesDialog::SetStatus (const GS::UniString& message)
{
	statusText.SetText (message);
}


void InheritedArticlesDialog::PopUpChanged (const DG::PopUpChangeEvent& ev)
{
	if (isFilling || ev.GetSource () != &systemPopup)
		return;

	RefreshArticles ();
	FillList ();
}


void InheritedArticlesDialog::ButtonClicked (const DG::ButtonClickEvent& ev)
{
	if (ev.GetSource () == &addButton) {
		AddRule ();
	} else if (ev.GetSource () == &deleteButton) {
		DeleteSelectedRule ();
	} else if (ev.GetSource () == &closeButton) {
		// Fermer enregistre la bibliothèque (best effort).
		GS::UniString error;
		RuleLibrary::SaveRules (rules, error);
		PostCloseRequest (DG::ModalDialog::Accept);
	}
}


// Index 0-based de la règle affichée à la ligne sélectionnée (-1 si aucun).
static short InheritedSelectedRuleIndex (const GS::Array<UIndex>& visibleRules, short selectedRule)
{
	if (selectedRule < 1 || static_cast<UIndex> (selectedRule) > visibleRules.GetSize ())
		return -1;
	return static_cast<short> (visibleRules[static_cast<UIndex> (selectedRule) - 1] - 1);
}


void InheritedArticlesDialog::ListBoxClicked (const DG::ListBoxClickEvent& ev)
{
	if (ev.GetSource () != &list || isFilling)
		return;

	const short clicked = ev.GetListItem ();
	if (clicked < 1 || static_cast<UIndex> (clicked) > visibleRules.GetSize ())
		return;

	selectedRule = clicked;
	const short ruleIndex = InheritedSelectedRuleIndex (visibleRules, selectedRule);
	if (ruleIndex < 0)
		return;

	// Édition DANS la ligne : Article hérité -> change (0 = supprime) ;
	// Valeur clé -> objet puis paramètre longueur ; Unité -> valeur
	// suivante ; Qté -> formule (paramètres GDL + quantités Archicad).
	switch (ev.GetTabFieldIndex ()) {
		case 2:
			EditSelectedArticle ();
			return;
		case 3:
			EditSelectedValueKey ();
			return;
		case 4:
			EditSelectedUnit ();
			return;
		case 5:
			EditSelectedQuantity ();
			return;
		default:
			break;
	}
}


void InheritedArticlesDialog::EditSelectedArticle ()
{
	const short ruleIndex = InheritedSelectedRuleIndex (visibleRules, selectedRule);
	if (ruleIndex < 0)
		return;
	CWMapRule& rule = rules[ruleIndex];

	if (articles.IsEmpty ()) {
		SetStatus (FR ("Aucune classe — choisissez un système de classification."));
		return;
	}

	ArticlePickerDialog articlePicker (articles);
	articlePicker.Invoke ();
	if (!articlePicker.IsAccepted ())
		return;

	const short articleIndex = articlePicker.GetSelectedArticleIndex ();
	if (articleIndex == 0) {
		// « (aucune) » : SUPPRIME la règle.
		const GS::UniString boolName = rule.keyName.IsEmpty () ? rule.keyId : rule.keyName;
		rules.Delete (static_cast<UIndex> (ruleIndex));
		selectedRule = 0;
		FillList ();
		SetStatus (FR ("Règle du booléen « ") + boolName + FR (" » supprimée."));
		return;
	}
	if (articleIndex >= 1 && static_cast<UIndex> (articleIndex) <= articles.GetSize ()) {
		rule.articleId = articles[static_cast<UIndex> (articleIndex) - 1].id;
		FillList ();
		SetStatus (FR ("« ") + (rule.keyName.IsEmpty () ? rule.keyId : rule.keyName)
				   + FR (" » → ") + rule.articleId);
	}
}


void InheritedArticlesDialog::EditSelectedValueKey ()
{
	const short ruleIndex = InheritedSelectedRuleIndex (visibleRules, selectedRule);
	if (ruleIndex < 0)
		return;
	CWMapRule& rule = rules[ruleIndex];

	// La valeur clé se choisit depuis un objet : navigateur d'objets puis
	// paramètre de type longueur (le booléen, lui, reste inchangé).
	GS::Array<GS::UniString> objectNames;
	RuleLibrary::CollectAvailableStructures (CWStructureType::LibraryPart, objectNames);
	if (objectNames.IsEmpty ()) {
		SetStatus (FR ("Aucun objet de bibliothèque posable n'est chargé."));
		return;
	}

	GS::Array<CWGdlParam> objectItems;
	for (UIndex o = 0; o < objectNames.GetSize (); ++o) {
		CWGdlParam object;
		object.label = objectNames[o];
		objectItems.Push (object);
	}

	GdlItemPickerDialog objectPicker (ID_ADDON_DLG_OBJPICKER, FR ("Objet (pour lister ses paramètres)"), FR (""),
									 objectItems, false);
	objectPicker.Invoke ();
	if (!objectPicker.IsAccepted ())
		return;

	const short objectIndex = objectPicker.GetSelectedItemIndex ();
	if (objectIndex < 1 || static_cast<UIndex> (objectIndex) > objectNames.GetSize ())
		return;
	const GS::UniString objectName = objectNames[static_cast<UIndex> (objectIndex) - 1];

	GS::Array<CWGdlParam> lengthParams;
	GS::UniString lengthNote;
	GS::UniString lengthAlert;
	if (!ModelReader::GetLibraryPartParameters (objectName, lengthParams, lengthNote, lengthAlert)) {
		SetStatus (FR ("« ") + objectName + FR (" » : ") + lengthNote);
		return;
	}
	if (!lengthAlert.IsEmpty ())
		DG::WarningAlert (FR ("Valeurs clés limitées pour « ") + objectName + FR (" »"),
						  lengthAlert, FR ("OK"));
	if (lengthParams.IsEmpty ()) {
		SetStatus (FR ("« ") + objectName + FR (" » : aucun paramètre (") + lengthNote + FR (")"));
		return;
	}

	GdlItemPickerDialog valuePicker (ID_ADDON_DLG_PARAMPICKER, FR ("Valeur clé"), FR ("Nom GDL"),
									 lengthParams, true, FR ("longueur"));
	valuePicker.Invoke ();
	if (!valuePicker.IsAccepted ())
		return;

	const short valueIndex = valuePicker.GetSelectedItemIndex ();
	if (valueIndex == 0) {
		rule.valueKeyId.Clear ();
		rule.valueKeyName.Clear ();
	} else if (valueIndex >= 1 && static_cast<UIndex> (valueIndex) <= lengthParams.GetSize ()) {
		rule.valueKeyId = lengthParams[static_cast<UIndex> (valueIndex) - 1].name;
		rule.valueKeyName = lengthParams[static_cast<UIndex> (valueIndex) - 1].label;
	}

	FillList ();
	SetStatus (FR ("« ") + (rule.keyName.IsEmpty () ? rule.keyId : rule.keyName)
			   + (rule.valueKeyName.IsEmpty () ? FR (" » : valeur clé retirée.")
											   : FR (" » — valeur clé : ") + rule.valueKeyName));
}


void InheritedArticlesDialog::EditSelectedUnit ()
{
	const short ruleIndex = InheritedSelectedRuleIndex (visibleRules, selectedRule);
	if (ruleIndex < 0)
		return;
	CWMapRule& rule = rules[ruleIndex];

	rule.unit = CWNextUnit (rule.unit);
	FillList ();
	SetStatus (FR ("« ") + (rule.keyName.IsEmpty () ? rule.keyId : rule.keyName)
			   + FR (" » — unité : ") + CWUnitDisplay (rule.unit));
}


void InheritedArticlesDialog::EditSelectedQuantity ()
{
	const short ruleIndex = InheritedSelectedRuleIndex (visibleRules, selectedRule);
	if (ruleIndex < 0)
		return;
	CWMapRule& rule = rules[ruleIndex];
	const GS::UniString boolName = rule.keyName.IsEmpty () ? rule.keyId : rule.keyName;

	// Variables de la formule : paramètres GDL de l'objet d'ORIGINE de la
	// règle (documentaire — la règle reste globale) + quantités Archicad.
	// Si l'objet n'est plus chargé, seules les quantités Archicad sont
	// proposées (note affichée, jamais silencieux).
	GS::Array<CWKeyEntry> variables;
	GS::UniString note;
	KeyCatalog::CollectFormulaVariables (rule.structureName, variables, note);
	if (!note.IsEmpty ())
		SetStatus (note);

	FormulaEditorDialog editor (rule.structureName, variables, rule.quantityFormula);
	editor.Invoke ();
	if (!editor.IsAccepted ())
		return;

	rule.quantityFormula = editor.GetFormula ();
	FillList ();
	SetStatus (FR ("« ") + boolName
			   + (rule.quantityFormula.IsEmpty ()
					  ? FR (" » : quantité automatique (formule vidée).")
					  : FR (" » — qté : ") + rule.quantityFormula));
}


void InheritedArticlesDialog::DeleteSelectedRule ()
{
	const short ruleIndex = InheritedSelectedRuleIndex (visibleRules, selectedRule);
	if (ruleIndex < 0) {
		SetStatus (FR ("Sélectionnez une règle à supprimer."));
		return;
	}
	const CWMapRule& rule = rules[ruleIndex];
	const GS::UniString boolName = rule.keyName.IsEmpty () ? rule.keyId : rule.keyName;

	if (DG::WarningAlert (FR ("Supprimer la règle du booléen « ") + boolName + FR (" » ?"),
						  FR ("Article hérité : ") + rule.articleId, FR ("Supprimer"), FR ("Annuler"))
			!= DG::AlertResponse::Accept)
		return;

	rules.Delete (static_cast<UIndex> (ruleIndex));
	selectedRule = 0;
	FillList ();
	SetStatus (FR ("Règle supprimée."));
}

} // namespace CostWaves
