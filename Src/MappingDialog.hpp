#ifndef COSTWAVES_MAPPING_DIALOG_HPP
#define COSTWAVES_MAPPING_DIALOG_HPP

#include "ACAPinc.h"
#include "DGModule.hpp"

#include "ResourceIds.hpp"
#include "DataTypes.hpp"

namespace CostWaves {

// Fenêtre « Correspondances matériaux » :
//  - filtre classification : le système choisi fournit les classes (articles) ;
//  - liste des matériaux de construction du projet ;
//  - la sélection se fait DANS le tableau : cliquer un matériau ouvre la
//    liste des classes « ID — Nom » avec barre de recherche (style sélecteur
//    d'attributs Archicad) ; « (aucune) » = pas de correspondance (un
//    matériau sans classe est déjà ignoré du métré) — pas d'option Ignorer ;
//  - colonne « Valeur clé » : cliquer la cellule ouvre le choix d'un
//    paramètre différenciant (ex. épaisseur) — clés calculées par COSTWAVES
//    ou propriété Archicad — pour créer des articles qui varient par
//    épaisseur, dimension… ;
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
		StatusTextId	= 7,
		SaveButtonId	= 8,
		CloseButtonId	= 9,
		GdlButtonId		= 10
	};

	MappingDialog ();

private:
	// DG::ButtonItemObserver
	virtual void	ButtonClicked (const DG::ButtonClickEvent& ev) override;

	// DG::ListBoxObserver
	virtual void	ListBoxSelectionChanged (const DG::ListBoxSelectionEvent& ev) override;
	virtual void	ListBoxClicked (const DG::ListBoxClickEvent& ev) override;

	// DG::PopUpObserver
	virtual void	PopUpChanged (const DG::PopUpChangeEvent& ev) override;

	// Type affiché : Matériau (1), Composite (2), Profil (3).
	bool			IsMaterialMode () const;
	CWStructureType	CurrentType () const;
	void	RefreshMaterials ();		// matériaux du projet -> liste
	void	RefreshArticles ();		// classes du système choisi
	void	FillList ();
	void	OpenPickerForSelection ();	// clic sur un matériau -> liste des classes
	void	OpenKeyPickerForSelection ();	// clic « Valeur clé » -> catalogue de clés
	void	SetStatus (const GS::UniString& message);

	DG::LeftText		infoText;
	DG::LeftText		typeLabel;
	DG::PopUp		typePopup;
	DG::LeftText		systemLabel;
	DG::PopUp		systemPopup;
	DG::MultiSelListBox	list;
	DG::LeftText		statusText;
	DG::Button		saveButton;
	DG::Button		closeButton;
	DG::Button		gdlButton;		// « Objets GDL… » : correspondances objets GDL

	GS::Array<CWSystemInfo>		systems;		// systèmes de classification du projet
	GS::Array<CWArticle>		articles;		// classes du système sélectionné
	GS::Array<GS::UniString>	materials;		// matériaux de construction du projet
	GS::Array<CWMapRule>		rules;			// bibliothèque de travail

	short	selectedMaterial = 0;		// 1-based (0 = aucune)
	bool	isFilling = false;
};

} // namespace CostWaves

#endif // COSTWAVES_MAPPING_DIALOG_HPP
