#ifndef COSTWAVES_SEND_DIALOG_HPP
#define COSTWAVES_SEND_DIALOG_HPP

#include "ACAPinc.h"
#include "DGModule.hpp"

#include "ResourceIds.hpp"
#include "CostWavesApi.hpp"

namespace CostWaves {

// Fenêtre « Envoyer vers CostWaves » (spéc. §12/§13) :
//  - URL du serveur (endpoint d'import) et clé API, mémorisés dans
//    CostWaves-settings.json à côté du PLN ;
//  - traitement des articles inconnus du serveur (spéc. §6) : ajouter au
//    projet uniquement (défaut), ajouter à la base + au projet, ignorer ;
//  - résumé de ce qui sera envoyé (éléments classés, skins classés,
//    articles du catalogue).
// L'envoi lui-même (bloquant) est réalisé par l'appelant après validation.
class SendDialog final :	public DG::ModalDialog,
							public DG::ButtonItemObserver
{
public:
	enum ItemIds {
		UrlLabelId			= 1,
		UrlEditId			= 2,
		ApiKeyLabelId		= 3,
		ApiKeyEditId		= 4,
		UnknownLabelId		= 5,
		UnknownPopupId		= 6,
		SummaryTextId		= 7,
		SendButtonId		= 8,
		CancelButtonId		= 9
	};

	SendDialog (const CWApiSettings& settings, USize classifiedElementCount,
				USize classifiedSkinCount, USize articleCount);

	bool			IsAccepted () const { return accepted; }
	CWApiSettings	GetSettings () const { return currentSettings; }

private:
	// DG::ButtonItemObserver
	virtual void	ButtonClicked (const DG::ButtonClickEvent& ev) override;

	DG::LeftText	urlLabel;
	DG::TextEdit	urlEdit;
	DG::LeftText	apiKeyLabel;
	DG::TextEdit	apiKeyEdit;
	DG::LeftText	unknownLabel;
	DG::PopUp		unknownPopup;
	DG::LeftText	summaryText;
	DG::Button		sendButton;
	DG::Button		cancelButton;

	CWApiSettings	currentSettings;
	bool			accepted = false;
};

} // namespace CostWaves

#endif // COSTWAVES_SEND_DIALOG_HPP
