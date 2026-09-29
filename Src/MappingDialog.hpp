#ifndef COSTWAVES_MAPPING_DIALOG_HPP
#define COSTWAVES_MAPPING_DIALOG_HPP

#include "ACAPinc.h"
#include "DGModule.hpp"

#include "ResourceIds.hpp"
#include "DataTypes.hpp"

namespace CostWaves {

// Fenêtre « Correspondances matériaux » :
//  - filtre classification : le système choisi fournit les classes (articles) ;
//  - tableau des matériaux : Matériau | Classe | ▼ ;
//  - la sélection se fait DIRECTEMENT DANS LE TABLEAU : cliquer une ligne
//    (flèche ▼) ouvre sur place la liste des classes « ID — Nom » avec un
//    champ de recherche en tête (style sélecteur d'attributs Archicad) ;
//    double-clic ou « Valider » applique ; « — (aucune) » retire la
//    correspondance (un matériau sans classe est déjà ignoré du métré) ;
//  - « Enregistrer » écrit la bibliothèque dans
//    <Documents>/CostWaves-regles.json (réutilisable entre projets).
class MappingDialog final :	public DG::ModalDialog,
							public DG::ButtonItemObserver,
							public DG::ListBoxObserver,
							public DG::PopUpObserver,
							public DG::SearchEditObserver
{
public:
	enum ItemIds {
		InfoTextId		= 1,
		SystemLabelId	= 2,
		SystemPopupId	= 3,
		ListId			= 4,
		StatusTextId	= 5,
		SaveButtonId	= 6,
		CloseButtonId	= 7,

		EditorHeaderId	= 8,	// « Classe de « … » » (caché hors sélection)
		EditorSearchId	= 9,	// recherche dans les classes
		EditorListId	= 10,	// liste des classes (en place du tableau)
		EditorChooseId	= 11,	// Valider
		EditorCloseId	= 12		// Fermer la liste
	};

	MappingDialog ();

private:
	// DG::ButtonItemObserver
	virtual void	ButtonClicked (const DG::ButtonClickEvent& ev) override;

	// DG::ListBoxObserver
	virtual void	ListBoxSelectionChanged (const DG::ListBoxSelectionEvent& ev) override;
	virtual void	ListBoxDoubleClicked (const DG::ListBoxDoubleClickEvent& ev) override;

	// DG::PopUpObserver
	virtual void	PopUpChanged (const DG::PopUpChangeEvent& ev) override;

	// DG::SearchEditObserver
	virtual void	SearchTextChanged (const DG::SearchEditChangeEvent& ev) override;

	void	RefreshMaterials ();		// matériaux du projet -> tableau
	void	RefreshArticles ();		// classes du système choisi
	void	FillList ();				// tableau Matériau | Classe | ▼
	void	UpdateMaterialRow (short item);	// cellule Classe d'une ligne

	void	OpenEditorForSelection ();	// clic ligne -> liste des classes en place
	void	FillEditorList (const GS::UniString& filter);
	void	EditorChoose ();			// appliquer la classe choisie
	void	CloseEditor (bool applied);
	void	UpdateStatusCounter ();

	void	SetStatus (const GS::UniString& message);

	DG::LeftText		infoText;
	DG::LeftText		systemLabel;
	DG::PopUp			systemPopup;
	DG::MultiSelListBox	list;			// tableau des matériaux
	DG::LeftText		statusText;
	DG::Button			saveButton;
	DG::Button			closeButton;

	DG::LeftText		editorHeader;
	DG::SearchEdit		editorSearch;
	DG::MultiSelListBox	editorList;		// liste des classes (en place)
	DG::Button			editorChooseButton;
	DG::Button			editorCloseButton;

	GS::Array<CWSystemInfo>		systems;		// systèmes de classification du projet
	GS::Array<CWArticle>		articles;		// classes du système sélectionné
	GS::Array<GS::UniString>	materials;		// matériaux de construction du projet
	GS::Array<CWMapRule>		rules;			// bibliothèque de travail

	GS::Array<short>	visibleArticles;	// index 1-based des classes affichées (recherche)

	short	selectedMaterial = 0;		// ligne 1-based (0 = aucune)
	short	selectedClassItem = 0;		// item 1-based dans la liste des classes
	bool	editorOpen = false;
	bool	isFilling = false;
};

} // namespace CostWaves

#endif // COSTWAVES_MAPPING_DIALOG_HPP
