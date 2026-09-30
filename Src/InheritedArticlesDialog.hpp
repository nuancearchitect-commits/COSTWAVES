#ifndef COSTWAVES_INHERITED_ARTICLES_DIALOG_HPP
#define COSTWAVES_INHERITED_ARTICLES_DIALOG_HPP

#include "ACAPinc.h"
#include "DGModule.hpp"

#include "ResourceIds.hpp"
#include "DataTypes.hpp"

namespace CostWaves {

// Fenêtre « Articles hérités » (bouton de la palette) : un article hérité
// naît d'un PARAMÈTRE BOOLÉEN ACTIVÉ — tablette, seuil, volet… — quel que
// soit l'objet qui le porte : le même nom de paramètre se répète dans
// plusieurs objets, la règle est donc GLOBALE (booléen -> article, sans
// lien d'objet). L'objet n'est utilisé que pour LISTER les paramètres.
// « Ajouter… » enchaîne trois choix :
//  1) un objet de bibliothèque (.gsm posables chargés, recherche) —
//     simple navigateur pour découvrir les noms de booléens ;
//  2) le paramètre BOOLÉEN (celui dont l'activation fait naître
//     l'article) ;
//  3) l'article CostWaves hérité (classe du système sélectionné) ;
//     « (aucune) » retire la règle ;
//  4) la valeur clé (FACULTATIVE) : une variable GDL de type longueur
//     de l'objet — comme dans les correspondances objets GDL, elle
//     différencie les variantes de l'article hérité (Ø125/Ø160, H8/H12…) ;
//  5) la formule de QUANTITÉ (colonne Qté) : composée depuis les
//     paramètres GDL de l'objet d'origine et les quantités Archicad —
//     PAS de mode de calcul ni de déductions pour ce type.
// Règles enregistrées dans la bibliothèque partagée avec la clé
// « bool:<nom GDL> » (type « objectBool ») + la valeur clé ; l'objet
// d'origine n'est conservé qu'à titre documentaire. « Fermer » enregistre.
class InheritedArticlesDialog final :	public DG::ModalDialog,
										public DG::ButtonItemObserver,
										public DG::ListBoxObserver,
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
		SystemPopupId	= 7,
		DeleteButtonId	= 8
	};

	InheritedArticlesDialog ();

private:
	// DG::ButtonItemObserver
	virtual void	ButtonClicked (const DG::ButtonClickEvent& ev) override;

	// DG::ListBoxObserver
	virtual void	ListBoxClicked (const DG::ListBoxClickEvent& ev) override;

	// DG::PopUpObserver
	virtual void	PopUpChanged (const DG::PopUpChangeEvent& ev) override;

	void	RefreshArticles ();		// classes du système choisi
	void	FillList ();
	void	AddRule ();				// objet -> paramètre booléen -> article
	void	EditSelectedArticle ();	// clic cellule Article hérité
	void	EditSelectedValueKey ();	// clic cellule Valeur clé (objet -> paramètre)
	void	EditSelectedUnit ();			// clic cellule Unité (cycle auto/m²/ml/m³/u/kg)
	void	EditSelectedQuantity ();	// clic cellule Qté (formule de la règle)
	void	DeleteSelectedRule ();	// bouton « Supprimer »
	void	SetStatus (const GS::UniString& message);

	DG::LeftText		infoText;
	DG::MultiSelListBox	list;
	DG::LeftText		statusText;
	DG::Button			addButton;
	DG::Button			closeButton;
	DG::LeftText		systemLabel;
	DG::PopUp			systemPopup;
	DG::Button			deleteButton;

	GS::Array<CWSystemInfo>	systems;		// systèmes de classification du projet
	GS::Array<CWArticle>	articles;		// classes du système sélectionné
	GS::Array<CWMapRule>	rules;			// bibliothèque de travail
	GS::Array<UIndex>		visibleRules;	// index 1-based des règles affichées

	short	selectedRule = 0;
	bool	isFilling = false;
};

} // namespace CostWaves

#endif // COSTWAVES_INHERITED_ARTICLES_DIALOG_HPP
