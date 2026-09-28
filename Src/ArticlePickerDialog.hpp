#ifndef COSTWAVES_ARTICLE_PICKER_DIALOG_HPP
#define COSTWAVES_ARTICLE_PICKER_DIALOG_HPP

#include "ACAPinc.h"
#include "DGModule.hpp"

#include "ResourceIds.hpp"
#include "DataTypes.hpp"

namespace CostWaves {

// Fenêtre de choix de l'article (classe de classification) pour « Créer un
// ensemble » / « Créer un groupe » :
//  - affiche le nombre d'éléments sélectionnés dans le plan ;
//  - liste les articles connus de la palette (import JSON ou classification),
//    hiérarchiquement (indentés comme la classification).
// Fenêtre modale définie en ressource GRC (ID_ADDON_DLG_PICKER).
class ArticlePickerDialog final :	public DG::ModalDialog,
									public DG::ButtonItemObserver
{
public:
	enum ItemIds {
		InfoTextId			= 1,
		ArticleLabelId		= 2,
		ArticlePopupId		= 3,
		CreateButtonId		= 4,
		CancelButtonId		= 5
	};

	ArticlePickerDialog (const GS::Array<CWArticle>& articles, USize selectedElementCount, bool numbered);

	bool		IsAccepted () const { return accepted; }
	CWArticle	GetSelectedArticle () const { return selectedArticle; }

private:
	// DG::ButtonItemObserver
	virtual void	ButtonClicked (const DG::ButtonClickEvent& ev) override;

	DG::LeftText	infoText;
	DG::LeftText	articleLabel;
	DG::PopUp		articlePopup;
	DG::Button		createButton;
	DG::Button		cancelButton;

	GS::Array<CWArticle>	articles;
	CWArticle				selectedArticle;
	bool					accepted = false;
};

} // namespace CostWaves

#endif // COSTWAVES_ARTICLE_PICKER_DIALOG_HPP
