#include "CostWavesPrecompiledHeader.hpp"

#include "InheritedArticlesDialog.hpp"

#include "ArticleManager.hpp"
#include "ArticlePickerDialog.hpp"
#include "GdlItemPickerDialog.hpp"
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
		systemPopup (GetReference (), SystemPopupId)
{
	infoText.SetText (FR ("Un article hérité naît d'un paramètre BOOLÉEN activé d'un objet")
					  + FR (" (tablette, seuil, volet…). « Ajouter… » : l'objet,")
					  + FR (" son paramètre booléen, puis l'article hérité."));

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

	const short columnCount = 3;
	list.SetHeaderItemCount (columnCount);
	list.SetTabFieldCount (columnCount);
	list.SetHeaderItemText (1, FR ("Objet"));
	list.SetHeaderItemText (2, FR ("Paramètre (booléen)"));
	list.SetHeaderItemText (3, FR ("Article hérité"));

	const short widths[3] = { 200, 170, 170 };
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

	USize count = 0;
	for (UIndex r = 0; r < rules.GetSize (); ++r) {
		if (rules[r].structureType != CWStructureType::LibraryPartBool)
			continue;
		++count;

		list.AppendItem ();
		const short item = list.GetItemCount ();
		list.SetTabItemText (item, 1, rules[r].structureName);

		GS::UniString paramLabel = rules[r].keyName;
		list.SetTabItemText (item, 2, paramLabel.IsEmpty ()
			? rules[r].keyId : paramLabel);

		const CWArticle* article = ArticleManager::FindArticle (articles, rules[r].articleId);
		list.SetTabItemText (item, 3, article != nullptr
			? rules[r].articleId + FR (" — ") + article->name
			: rules[r].articleId);
	}

	isFilling = false;

	SetStatus (GS::ToUniString (std::to_wstring (static_cast<int> (count)))
			   + FR (" article(s) hérité(s)."));
}


void InheritedArticlesDialog::AddRule ()
{
	// 1) L'objet de bibliothèque (objets .gsm posables chargés).
	GS::Array<GS::UniString> objectNames;
	RuleLibrary::CollectAvailableStructures (CWStructureType::LibraryPart, objectNames);
	if (objectNames.IsEmpty ()) {
		SetStatus (FR ("Aucun objet de bibliothèque posable n'est chargé."));
		return;
	}

	GS::Array<GS::Pair<GS::UniString, GS::UniString>> objectItems;
	for (UIndex o = 0; o < objectNames.GetSize (); ++o)
		objectItems.Push (GS::Pair<GS::UniString, GS::UniString> (objectNames[o], GS::UniString ()));

	GdlItemPickerDialog objectPicker (ID_ADDON_DLG_OBJPICKER, FR ("Objet"), FR (""),
									  objectItems, false);
	objectPicker.Invoke ();
	if (!objectPicker.IsAccepted ())
		return;

	const short objectIndex = objectPicker.GetSelectedItemIndex ();
	if (objectIndex < 1 || static_cast<UIndex> (objectIndex) > objectNames.GetSize ())
		return;
	const GS::UniString objectName = objectNames[static_cast<UIndex> (objectIndex) - 1];

	// 2) Le paramètre BOOLÉEN de l'objet — celui dont l'activation fait
	//    naître l'article hérité (tablette, seuil, volet…).
	GS::Array<GS::Pair<GS::UniString, GS::UniString>> params;
	GS::UniString paramNote;
	GS::UniString paramAlert;
	if (!ModelReader::GetLibraryPartBooleanParameters (objectName, params, paramNote, paramAlert)) {
		SetStatus (FR ("« ") + objectName + FR (" » : ") + paramNote);
		return;
	}
	if (!paramAlert.IsEmpty ())
		DG::WarningAlert (FR ("Paramètres booléens limités pour « ") + objectName + FR (" »"),
						  paramAlert, FR ("OK"));
	if (params.IsEmpty ()) {
		SetStatus (FR ("« ") + objectName + FR (" » : ") + paramNote);
		return;
	}

	GdlItemPickerDialog paramPicker (ID_ADDON_DLG_PARAMPICKER, FR ("Paramètre booléen"), FR ("Nom GDL"),
									 params, false);
	paramPicker.Invoke ();
	if (!paramPicker.IsAccepted ())
		return;

	const short paramIndex = paramPicker.GetSelectedItemIndex ();
	if (paramIndex < 1 || static_cast<UIndex> (paramIndex) > params.GetSize ())
		return;
	const GS::UniString paramName = params[static_cast<UIndex> (paramIndex) - 1].second;		// nom GDL stable
	const GS::UniString paramLabel = params[static_cast<UIndex> (paramIndex) - 1].first;	// libellé lisible
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

	if (articleIndex == 0) {
		for (UIndex r = 0; r < rules.GetSize (); ++r) {
			if (rules[r].structureType == CWStructureType::LibraryPartBool
				&& rules[r].structureName == objectName
				&& rules[r].keyId == boolKey) {
				rules.Delete (r);
				break;
			}
		}
		SetStatus (FR ("« ") + objectName + FR (" » / ") + paramLabel + FR (" » : article hérité retiré."));
	} else {
		const CWArticle& article = articles[static_cast<UIndex> (articleIndex) - 1];

		CWMapRule rule;
		rule.structureType = CWStructureType::LibraryPartBool;
		rule.structureName = objectName;
		rule.articleId = article.id;
		rule.mode = CWQuantMode::Element;
		rule.keyId = boolKey;
		rule.keyName = paramLabel;

		bool replaced = false;
		for (UIndex r = 0; r < rules.GetSize (); ++r) {
			if (rules[r].structureType == CWStructureType::LibraryPartBool
				&& rules[r].structureName == objectName
				&& rules[r].keyId == boolKey) {
				rules[r] = rule;
				replaced = true;
				break;
			}
		}
		if (!replaced)
			rules.Push (rule);

		SetStatus (FR ("« ") + objectName + FR (" » + ") + paramLabel
				   + FR (" activé → ") + article.id + FR (" — ") + article.name);
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
	} else if (ev.GetSource () == &closeButton) {
		// Fermer enregistre la bibliothèque (best effort).
		GS::UniString error;
		RuleLibrary::SaveRules (rules, error);
		PostCloseRequest (DG::ModalDialog::Accept);
	}
}

} // namespace CostWaves
