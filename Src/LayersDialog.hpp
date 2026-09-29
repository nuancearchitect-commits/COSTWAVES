#ifndef COSTWAVES_LAYERS_DIALOG_HPP
#define COSTWAVES_LAYERS_DIALOG_HPP

#include "ACAPinc.h"
#include "DGModule.hpp"

#include "ResourceIds.hpp"
#include "DataTypes.hpp"

namespace CostWaves {

// Fenêtre « Couches de la structure » (nouvelle architecture) :
// liste les matériaux des couches d'un composite (ou les matériaux posés
// d'un profil) et permet de donner un article à chacun de ceux qui n'en ont
// pas déjà (« si ses couches, on les donne des articles s'ils n'en ont pas
// déjà »). Les règles matériau créées sont écrites dans ioRules (la
// bibliothèque est enregistrée par la fenêtre appelante).
class LayersDialog final :	public DG::ModalDialog,
							public DG::ButtonItemObserver,
							public DG::ListBoxObserver
{
public:
	enum ItemIds {
		InfoTextId		= 1,
		TableId			= 2,
		ArticleLabelId	= 3,
		ArticlePopupId	= 4,
		AssignButtonId	= 5,
		StatusTextId	= 6,
		CloseButtonId	= 7
	};

	// materialNames : couches à proposer. ioRules : bibliothèque de travail
	// (les règles matériau ajoutées/modifiées y sont écrites directement).
	LayersDialog (const GS::UniString& structureName,
				  const GS::Array<GS::UniString>& materialNames,
				  const GS::Array<CWArticle>& inArticles,
				  GS::Array<CWMapRule>& ioRules);

private:
	// DG::ButtonItemObserver
	virtual void	ButtonClicked (const DG::ButtonClickEvent& ev) override;

	// DG::ListBoxObserver
	virtual void	ListBoxSelectionChanged (const DG::ListBoxSelectionEvent& ev) override;

	void		FillTable ();
	void		AssignArticleToSelected ();
	void		UpdateStatus ();

	DG::LeftText		infoText;
	DG::MultiSelListBox	table;
	DG::LeftText		articleLabel;
	DG::PopUp			articlePopup;
	DG::Button			assignButton;
	DG::LeftText		statusText;
	DG::Button			closeButton;

	GS::UniString			structureName;
	GS::Array<GS::UniString>	materials;
	GS::Array<CWArticle>		articles;
	GS::Array<CWMapRule>&		rules;

	bool	isFilling = false;
};

} // namespace CostWaves

#endif // COSTWAVES_LAYERS_DIALOG_HPP
