#include "CostWavesPrecompiledHeader.hpp"

#include "GdlMappingDialog.hpp"

#include "ArticleManager.hpp"
#include "ArticlePickerDialog.hpp"
#include "CostWavesStyle.hpp"
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

} // namespace


GdlMappingDialog::GdlMappingDialog ()
	:	DG::ModalDialog (ACAPI_GetOwnResModule (), ID_ADDON_DLG_GDL, ACAPI_GetOwnResModule ()),
		infoText (GetReference (), InfoTextId),
		list (GetReference (), ListId),
		statusText (GetReference (), StatusTextId),
		addButton (GetReference (), AddButtonId),
		closeButton (GetReference (), CloseButtonId),
		systemLabel (GetReference (), SystemLabelId),
		systemPopup (GetReference (), SystemPopupId)
{
	infoText.SetText (FR ("Choisissez le système de classification, puis « Ajouter… » :")
					  + FR (" l'objet de bibliothèque, sa classe (article) et sa valeur clé")
					  + FR (" (variable GDL de type longueur de l'objet)."));
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
}


void GdlMappingDialog::RefreshArticles ()
{
	articles.Clear ();
	const short systemSelection = systemPopup.GetSelectedItem ();
	if (systemSelection >= 1 && static_cast<UIndex> (systemSelection) <= systems.GetSize ())
		ArticleManager::CollectFromClassification (systems[static_cast<UIndex> (systemSelection) - 1].guid,
												   articles);
}


void GdlMappingDialog::FillList ()
{
	isFilling = true;

	const short columnCount = 3;
	list.SetHeaderItemCount (columnCount);
	list.SetTabFieldCount (columnCount);
	list.SetHeaderItemText (1, FR ("Objet"));
	list.SetHeaderItemText (2, FR ("Article"));
	list.SetHeaderItemText (3, FR ("Valeur clé"));

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
		if (rules[r].structureType != CWStructureType::LibraryPart)
			continue;
		++count;

		list.AppendItem ();
		const short item = list.GetItemCount ();
		list.SetTabItemText (item, 1, rules[r].structureName);

		const CWArticle* article = ArticleManager::FindArticle (articles, rules[r].articleId);
		list.SetTabItemText (item, 2, article != nullptr
			? rules[r].articleId + FR (" — ") + article->name
			: rules[r].articleId);
		CostWavesStyle::CellAccent (list, item, 2);

		list.SetTabItemText (item, 3, rules[r].keyName.IsEmpty () ? FR ("—") : rules[r].keyName);
		if (rules[r].keyName.IsEmpty ())
			CostWavesStyle::CellMuted (list, item, 3);
		else
			CostWavesStyle::CellOk (list, item, 3);
	}

	isFilling = false;

	SetStatus (GS::ToUniString (std::to_wstring (static_cast<int> (count)))
			   + FR (" objet(s) GDL en correspondance."));
}


void GdlMappingDialog::AddRule ()
{
	// 1) L'objet de bibliothèque (objets .gsm posables chargés).
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

	GdlItemPickerDialog objectPicker (ID_ADDON_DLG_OBJPICKER, FR ("Objet"), FR (""),
									  objectItems, false);
	objectPicker.Invoke ();
	if (!objectPicker.IsAccepted ())
		return;

	const short objectIndex = objectPicker.GetSelectedItemIndex ();
	if (objectIndex < 1 || static_cast<UIndex> (objectIndex) > objectNames.GetSize ())
		return;
	const GS::UniString objectName = objectNames[static_cast<UIndex> (objectIndex) - 1];

	// 2) La classe (article) — « (aucune) » retire la correspondance.
	if (articles.IsEmpty ()) {
		SetStatus (FR ("Aucune classe — choisissez un système de classification."));
		return;
	}

	ArticlePickerDialog articlePicker (articles);
	articlePicker.Invoke ();
	if (!articlePicker.IsAccepted ())
		return;

	const short articleIndex = articlePicker.GetSelectedArticleIndex ();

	// 3) La valeur clé : une variable GDL de TYPE LONGUEUR de l'objet
	//    (épaisseur, hauteur, dimensions…), facultative. Annuler le choix
	//    = pas de clé (l'article choisi reste enregistré). Le comptage
	//    (paramètres lus / de type longueur / source) est TOUJOURS
	//    affiché dans la ligne d'état ; une ALERTE explique toute liste
	//    vide ou réduite aux paramètres fixes (répartition des types) —
	//    le diagnostic complet va dans Documents/CostWaves-diagnostic.txt.
	GS::UniString keyId;
	GS::UniString keyName;
	GS::UniString paramNote;
	GS::UniString paramAlert;
	if (articleIndex != 0) {
		// TOUS les paramètres GDL avec leur type : le sélecteur montre la
		// colonne Type (double vérification), longueurs en tête et en gras.
		GS::Array<CWGdlParam> params;
		if (ModelReader::GetLibraryPartParameters (objectName, params, paramNote, paramAlert)) {
			if (!paramAlert.IsEmpty ())
				DG::WarningAlert (FR ("Valeurs clés limitées pour « ") + objectName + FR (" »"),
								  paramAlert, FR ("OK"));
			if (!params.IsEmpty ()) {
				GdlItemPickerDialog paramPicker (ID_ADDON_DLG_PARAMPICKER, FR ("Paramètre"), FR ("Nom GDL"),
												 params, true, FR ("longueur"));
				paramPicker.Invoke ();
				if (paramPicker.IsAccepted ()) {
					const short paramIndex = paramPicker.GetSelectedItemIndex ();
					if (paramIndex >= 1 && static_cast<UIndex> (paramIndex) <= params.GetSize ()) {
						keyId = params[static_cast<UIndex> (paramIndex) - 1].name;		// nom GDL stable
						keyName = params[static_cast<UIndex> (paramIndex) - 1].label;	// libellé lisible
					}
				}
			}
		}
	}

	// 4) Enregistrer / retirer la règle.
	if (articleIndex == 0) {
		for (UIndex r = 0; r < rules.GetSize (); ++r) {
			if (rules[r].structureType == CWStructureType::LibraryPart
				&& rules[r].structureName == objectName) {
				rules.Delete (r);
				break;
			}
		}
		SetStatus (FR ("« ") + objectName + FR (" » : correspondance retirée."));
	} else {
		const CWArticle& article = articles[static_cast<UIndex> (articleIndex) - 1];

		CWMapRule rule;
		rule.structureType = CWStructureType::LibraryPart;
		rule.structureName = objectName;
		rule.articleId = article.id;
		rule.mode = CWQuantMode::Element;
		rule.keyId = keyId;
		rule.keyName = keyName;

		bool replaced = false;
		for (UIndex r = 0; r < rules.GetSize (); ++r) {
			if (rules[r].structureType == CWStructureType::LibraryPart
				&& rules[r].structureName == objectName) {
				rules[r] = rule;
				replaced = true;
				break;
			}
		}
		if (!replaced)
			rules.Push (rule);

		SetStatus (FR ("« ") + objectName + FR (" » → ") + article.id + FR (" — ") + article.name
				   + (keyName.IsEmpty () ? GS::UniString () : FR (" · clé : ") + keyName)
				   + (paramNote.IsEmpty () ? GS::UniString () : FR (" · ") + paramNote));
	}

	FillList ();
}


void GdlMappingDialog::SetStatus (const GS::UniString& message)
{
	statusText.SetText (message);
}


void GdlMappingDialog::PopUpChanged (const DG::PopUpChangeEvent& ev)
{
	if (isFilling || ev.GetSource () != &systemPopup)
		return;

	RefreshArticles ();
	FillList ();
}


void GdlMappingDialog::ButtonClicked (const DG::ButtonClickEvent& ev)
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
