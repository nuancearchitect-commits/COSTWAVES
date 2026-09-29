#ifndef COSTWAVES_ARTICLE_PICKER_DIALOG_HPP
#define COSTWAVES_ARTICLE_PICKER_DIALOG_HPP

#include "ACAPinc.h"
#include "DGModule.hpp"

#include "ResourceIds.hpp"
#include "DataTypes.hpp"

namespace CostWaves {

// Sélecteur de classe (style sélecteur d'attributs Archicad) : liste des
// classes « ID — Nom » avec barre de recherche en haut. La première entrée
// « — (aucune) » retire la correspondance (un matériau sans classe est déjà
// ignoré du métré). Double-clic ou « Choisir » valide.
class ArticlePickerDialog final :	public DG::ModalDialog,
									public DG::ButtonItemObserver,
									public DG::ListBoxObserver,
									public DG::SearchEditObserver
{
public:
	enum ItemIds {
		SearchEditId	= 1,
		ListId			= 2,
		ChooseButtonId	= 3,
		CancelButtonId	= 4
	};

	ArticlePickerDialog (const GS::Array<CWArticle>& inArticles);

	bool		IsAccepted () const { return accepted; }
	// 0 = « (aucune) » ; sinon index 1-based dans articles.
	short		GetSelectedArticleIndex () const { return selectedArticleIndex; }

private:
	// DG::ButtonItemObserver
	virtual void	ButtonClicked (const DG::ButtonClickEvent& ev) override;

	// DG::ListBoxObserver
	virtual void	ListBoxDoubleClicked (const DG::ListBoxDoubleClickEvent& ev) override;

	// DG::SearchEditObserver
	virtual void	SearchTextChanged (const DG::SearchEditChangeEvent& ev) override;

	void	FillList (const GS::UniString& filter);
	void	ChooseCurrent ();

	DG::SearchEdit		searchEdit;
	DG::MultiSelListBox	list;
	DG::Button			chooseButton;
	DG::Button			cancelButton;

	GS::Array<CWArticle>	articles;
	GS::Array<short>		visibleArticles;	// index 1-based affichés (hors « (aucune) »)

	short	selectedArticleIndex = 0;
	bool	accepted = false;
	bool	isFilling = false;
};

} // namespace CostWaves

#endif // COSTWAVES_ARTICLE_PICKER_DIALOG_HPP
