#ifndef COSTWAVES_INHERITED_ARTICLES_DIALOG_HPP
#define COSTWAVES_INHERITED_ARTICLES_DIALOG_HPP

#include "ACAPinc.h"
#include "DGModule.hpp"

#include "ResourceIds.hpp"
#include "DataTypes.hpp"

namespace CostWaves {

// Fenêtre « Articles hérités » (bouton de la palette) : un article hérité
// naît d'un objet GDL dont un PARAMÈTRE BOOLÉEN est activé — tablette,
// seuil, volet… Exemple : une fenêtre avec « volet » coché produit, en
// plus de l'article de la fenêtre, l'article du volet.
// « Ajouter… » enchaîne trois choix :
//  1) l'objet de bibliothèque (.gsm posables chargés, recherche) ;
//  2) son paramètre BOOLÉEN (celui dont l'activation fait naître
//     l'article) ;
//  3) l'article CostWaves hérité (classe du système sélectionné) ;
//     « (aucune) » retire la règle.
// Règles enregistrées dans la bibliothèque partagée avec la clé
// « bool:<nom GDL> » (type « objectBool »). « Fermer » enregistre.
class InheritedArticlesDialog final :	public DG::ModalDialog,
										public DG::ButtonItemObserver,
										public DG::PopUpObserver
{
public:
	enum ItemIds {
		InfoTextId		= 1,
		ListId			= 2,
		StatusTextId	= 3,
		AddButtonId		= 4,
		CloseButtonId	= 5,
		SystemLabelId	= 6,
		SystemPopupId	= 7
	};

	InheritedArticlesDialog ();

private:
	// DG::ButtonItemObserver
	virtual void	ButtonClicked (const DG::ButtonClickEvent& ev) override;

	// DG::PopUpObserver
	virtual void	PopUpChanged (const DG::PopUpChangeEvent& ev) override;

	void	RefreshArticles ();		// classes du système choisi
	void	FillList ();
	void	AddRule ();				// objet -> paramètre booléen -> article
	void	SetStatus (const GS::UniString& message);

	DG::LeftText		infoText;
	DG::MultiSelListBox	list;
	DG::LeftText		statusText;
	DG::Button			addButton;
	DG::Button			closeButton;
	DG::LeftText		systemLabel;
	DG::PopUp			systemPopup;

	GS::Array<CWSystemInfo>	systems;		// systèmes de classification du projet
	GS::Array<CWArticle>	articles;		// classes du système sélectionné
	GS::Array<CWMapRule>	rules;			// bibliothèque de travail

	bool	isFilling = false;
};

} // namespace CostWaves

#endif // COSTWAVES_INHERITED_ARTICLES_DIALOG_HPP
