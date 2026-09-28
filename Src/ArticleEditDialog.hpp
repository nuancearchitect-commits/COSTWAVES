#ifndef COSTWAVES_ARTICLE_EDIT_DIALOG_HPP
#define COSTWAVES_ARTICLE_EDIT_DIALOG_HPP

#include "ACAPinc.h"
#include "DGModule.hpp"

#include "ResourceIds.hpp"
#include "DataTypes.hpp"

namespace CostWaves {

// Fenêtre « Créer un article » (spec §10) : création d'un article CostWaves
// directement depuis Archicad, lorsqu'une structure n'a pas d'article
// correspondant. L'article est local (utilisé immédiatement, persisté dans
// <Documents>/CostWaves-articles-locaux.json) ; l'envoi vers la base CostWaves
// viendra avec l'API (synchronisation ultérieure).
class ArticleEditDialog final :	public DG::ModalDialog,
								public DG::ButtonItemObserver
{
public:
	enum ItemIds {
		ChapterLabelId		= 1,
		ChapterEditId		= 2,
		IdLabelId			= 3,
		IdEditId			= 4,
		NameLabelId			= 5,
		NameEditId			= 6,
		UnitLabelId			= 7,
		UnitPopupId			= 8,
		ModeLabelId			= 9,
		ModePopupId			= 10,
		CreateButtonId		= 11,
		CancelButtonId		= 12
	};

	ArticleEditDialog ();

	bool		IsAccepted () const { return accepted; }
	CWArticle	GetArticle () const { return article; }

private:
	virtual void	ButtonClicked (const DG::ButtonClickEvent& ev) override;

	DG::LeftText	chapterLabel;
	DG::TextEdit	chapterEdit;
	DG::LeftText	idLabel;
	DG::TextEdit	idEdit;
	DG::LeftText	nameLabel;
	DG::TextEdit	nameEdit;
	DG::LeftText	unitLabel;
	DG::PopUp		unitPopup;
	DG::LeftText	modeLabel;
	DG::PopUp		modePopup;
	DG::Button		createButton;
	DG::Button		cancelButton;

	CWArticle	article;
	bool		accepted = false;
};

} // namespace CostWaves

#endif // COSTWAVES_ARTICLE_EDIT_DIALOG_HPP
