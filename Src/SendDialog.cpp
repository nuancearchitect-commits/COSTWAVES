#include "CostWavesPrecompiledHeader.hpp"

#include "SendDialog.hpp"

#include "UniStringWStringConversion.hpp"

#include <cwchar>

namespace CostWaves {

namespace {

// Littéral UTF-8 -> GS::UniString (les sources sont compilees avec /utf-8).
GS::UniString FR (const char* utf8Text)
{
	return GS::UniString (utf8Text, CC_UTF8);
}

} // namespace


SendDialog::SendDialog (const CWApiSettings& settings, USize classifiedElementCount,
						USize classifiedSkinCount, USize articleCount)
	:	DG::ModalDialog (ACAPI_GetOwnResModule (), ID_ADDON_DLG_SEND, ACAPI_GetOwnResModule ()),
		urlLabel (GetReference (), UrlLabelId),
		urlEdit (GetReference (), UrlEditId),
		apiKeyLabel (GetReference (), ApiKeyLabelId),
		apiKeyEdit (GetReference (), ApiKeyEditId),
		unknownLabel (GetReference (), UnknownLabelId),
		unknownPopup (GetReference (), UnknownPopupId),
		summaryText (GetReference (), SummaryTextId),
		sendButton (GetReference (), SendButtonId),
		cancelButton (GetReference (), CancelButtonId),
		currentSettings (settings)
{
	if (currentSettings.unknownArticleMode.IsEmpty ())
		currentSettings.unknownArticleMode = FR ("project_only");

	urlEdit.SetText (currentSettings.endpointUrl);
	apiKeyEdit.SetText (currentSettings.apiKey);

	// Articles inconnus du serveur (spéc. §6) — défaut : projet uniquement.
	unknownPopup.AppendItem ();
	unknownPopup.SetItemText (1, FR ("Ajouter au projet uniquement (recommandé)"));
	unknownPopup.AppendItem ();
	unknownPopup.SetItemText (2, FR ("Ajouter à la base + au projet"));
	unknownPopup.AppendItem ();
	unknownPopup.SetItemText (3, FR ("Ignorer"));
	short modeItem = 1;
	if (currentSettings.unknownArticleMode == FR ("base_and_project"))
		modeItem = 2;
	else if (currentSettings.unknownArticleMode == FR ("ignore"))
		modeItem = 3;
	unknownPopup.SelectItem (modeItem);

	summaryText.SetText (
		FR ("Contenu de l'envoi : ")
		+ GS::ToUniString (std::to_wstring (static_cast<int> (classifiedElementCount)))
		+ FR (" élément(s)/ensemble(s)/groupe(s) classé(s), ")
		+ GS::ToUniString (std::to_wstring (static_cast<int> (classifiedSkinCount)))
		+ FR (" skin(s) classé(s), ")
		+ GS::ToUniString (std::to_wstring (static_cast<int> (articleCount)))
		+ FR (" article(s) au catalogue.\n")
		+ FR ("Les membres d'ensembles/groupes ne sont pas facturés individuellement (spéc. §8/§9).\n")
		+ FR ("L'envoi est bloquant pendant quelques secondes (délai max 30 s)."));

	sendButton.Attach (*this);
	cancelButton.Attach (*this);
}


void SendDialog::ButtonClicked (const DG::ButtonClickEvent& ev)
{
	if (ev.GetSource () == &sendButton) {
		currentSettings.endpointUrl = urlEdit.GetText ();
		currentSettings.apiKey = apiKeyEdit.GetText ();

		switch (unknownPopup.GetSelectedItem ()) {
			case 2:		currentSettings.unknownArticleMode = FR ("base_and_project"); break;
			case 3:		currentSettings.unknownArticleMode = FR ("ignore"); break;
			default:	currentSettings.unknownArticleMode = FR ("project_only"); break;
		}

		if (currentSettings.endpointUrl.IsEmpty ()) {
			DG::WarningAlert (FR ("L'URL du serveur est vide."),
							  FR ("Saisissez l'adresse de l'API d'import CostWaves (ex. https://app.costwaves.com/api/archicad/import)."),
							  FR ("OK"));
			return;
		}

		accepted = true;
		PostCloseRequest (DG::ModalDialog::Accept);
	} else if (ev.GetSource () == &cancelButton) {
		PostCloseRequest (DG::ModalDialog::Cancel);
	}
}

} // namespace CostWaves
