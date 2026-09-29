#ifndef COSTWAVES_GDL_MAPPING_DIALOG_HPP
#define COSTWAVES_GDL_MAPPING_DIALOG_HPP

#include "ACAPinc.h"
#include "DGModule.hpp"

#include "ResourceIds.hpp"
#include "DataTypes.hpp"

namespace CostWaves {

// Fenêtre « Correspondances objets GDL » (bouton « Correspondance objets
// GDL » de la palette) : liste des correspondances objet de bibliothèque
// -> article + valeur clé (paramètre GDL). Autonome : système de
// classification choisi dans son popup, règles chargées/enregistrées
// dans la bibliothèque partagée (Documents/CostWaves-regles.json).
// « Ajouter… » enchaîne trois choix :
//  1) l'objet de bibliothèque (objets .gsm posables chargés, recherche) ;
//  2) sa classe (article, système sélectionné) ;
//  3) sa valeur clé : une variable GDL de TYPE LONGUEUR de l'objet
//     (épaisseur, hauteur, dimensions…), ou « (aucune) ».
// La classe « (aucune) » RETIRE la correspondance de l'objet.
// « Fermer » enregistre la bibliothèque (best effort).
class GdlMappingDialog final :	public DG::ModalDialog,
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

	GdlMappingDialog ();

private:
	// DG::ButtonItemObserver
	virtual void	ButtonClicked (const DG::ButtonClickEvent& ev) override;

	// DG::PopUpObserver
	virtual void	PopUpChanged (const DG::PopUpChangeEvent& ev) override;

	void	RefreshArticles ();		// classes du système choisi
	void	FillList ();
	void	AddRule ();				// objet -> article -> paramètre GDL
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

#endif // COSTWAVES_GDL_MAPPING_DIALOG_HPP
