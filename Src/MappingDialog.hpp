#ifndef COSTWAVES_MAPPING_DIALOG_HPP
#define COSTWAVES_MAPPING_DIALOG_HPP

#include "ACAPinc.h"
#include "DGModule.hpp"

#include "ResourceIds.hpp"
#include "DataTypes.hpp"

namespace CostWaves {

// Fenêtre « Correspondances » (étape courante) :
//  - filtre par type d'attribut Archicad : Matériau / Composite / Profil
//    complexe — la liste des attributs du projet s'affiche ;
//  - filtre par système de classification : les classes du système choisi
//    sont les articles proposés par le sélecteur ;
//  - devant chaque attribut, un sélecteur (style sélecteur d'attributs
//    Archicad : popup avec liste hiérarchique indentée) pour choisir
//    l'article (classe) ou « Ignorer » ;
//  - « Enregistrer » écrit la bibliothèque dans
//    <Documents>/CostWaves-regles.json (réutilisable entre projets).
class MappingDialog final :	public DG::ModalDialog,
							public DG::ButtonItemObserver,
							public DG::ListBoxObserver,
							public DG::PopUpObserver
{
public:
	enum ItemIds {
		InfoTextId		= 1,
		TypeLabelId		= 2,
		TypePopupId		= 3,
		SystemLabelId	= 4,
		SystemPopupId	= 5,
		ListId			= 6,
		SelectorLabelId	= 7,
		ArticlePopupId	= 8,
		SaveButtonId	= 9,
		CloseButtonId	= 10
	};

	MappingDialog ();

private:
	// DG::ButtonItemObserver
	virtual void	ButtonClicked (const DG::ButtonClickEvent& ev) override;

	// DG::ListBoxObserver
	virtual void	ListBoxSelectionChanged (const DG::ListBoxSelectionEvent& ev) override;

	// DG::PopUpObserver
	virtual void	PopUpChanged (const DG::PopUpChangeEvent& ev) override;

	CWStructureType	CurrentType () const;

	void	RefreshAttributes ();		// attributs du type choisi -> liste
	void	RefreshArticles ();		// classes du système choisi -> sélecteur
	void	FillList ();				// attribut + article courant
	void	RefreshSelectorForSelection ();
	void	ApplyArticleSelection ();	// sélecteur -> règle de l'attribut sélectionné
	void	SetStatus (const GS::UniString& message);

	DG::LeftText		infoText;
	DG::LeftText		typeLabel;
	DG::PopUp			typePopup;
	DG::LeftText		systemLabel;
	DG::PopUp			systemPopup;
	DG::MultiSelListBox	list;
	DG::LeftText		selectorLabel;
	DG::PopUp			articlePopup;
	DG::Button			saveButton;
	DG::Button			closeButton;

	GS::Array<CWSystemInfo>		systems;		// systèmes de classification du projet
	GS::Array<CWArticle>		articles;		// classes du système sélectionné
	GS::Array<GS::UniString>	attributes;		// attributs du type choisi
	GS::Array<CWMapRule>		rules;			// bibliothèque de travail

	short	selectedAttribute = 0;		// 1-based (0 = aucune)
	bool	isFilling = false;
};

} // namespace CostWaves

#endif // COSTWAVES_MAPPING_DIALOG_HPP
