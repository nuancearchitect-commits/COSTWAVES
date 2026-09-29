#ifndef COSTWAVES_GDL_MAPPING_DIALOG_HPP
#define COSTWAVES_GDL_MAPPING_DIALOG_HPP

#include "ACAPinc.h"
#include "DGModule.hpp"

#include "ResourceIds.hpp"
#include "DataTypes.hpp"

namespace CostWaves {

// Fenêtre « Correspondances objets GDL » (ouverte par le bouton
// « Objets GDL… » des correspondances) : liste des correspondances
// objet de bibliothèque -> article + valeur clé (paramètre GDL).
// « Ajouter… » enchaîne trois choix :
//  1) l'objet de bibliothèque (objets .gsm posables chargés, recherche) ;
//  2) sa classe (article, système de classification courant) ;
//  3) sa valeur clé : un PARAMÈTRE GDL de l'objet (ex. épaisseur,
//     hauteur — libellé + nom GDL), ou « (aucune) ».
// La classe « (aucune) » RETIRE la correspondance de l'objet.
// Les règles sont partagées par référence avec la fenêtre des
// correspondances ; « Fermer » enregistre la bibliothèque (best effort).
class GdlMappingDialog final :	public DG::ModalDialog,
								public DG::ButtonItemObserver
{
public:
	enum ItemIds {
		InfoTextId		= 1,
		ListId			= 2,
		StatusTextId	= 3,
		AddButtonId		= 4,
		CloseButtonId	= 5
	};

	GdlMappingDialog (GS::Array<CWMapRule>& ioRules, const GS::Array<CWArticle>& inArticles);

private:
	// DG::ButtonItemObserver
	virtual void	ButtonClicked (const DG::ButtonClickEvent& ev) override;

	void	FillList ();
	void	AddRule ();		// objet -> article -> paramètre GDL
	void	SetStatus (const GS::UniString& message);

	DG::LeftText		infoText;
	DG::MultiSelListBox	list;
	DG::LeftText		statusText;
	DG::Button			addButton;
	DG::Button			closeButton;

	GS::Array<CWMapRule>&		rules;		// bibliothèque partagée (par référence)
	const GS::Array<CWArticle>&	articles;	// classes du système sélectionné

	bool	isFilling = false;
};

} // namespace CostWaves

#endif // COSTWAVES_GDL_MAPPING_DIALOG_HPP
