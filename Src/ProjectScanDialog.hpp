#ifndef COSTWAVES_PROJECT_SCAN_DIALOG_HPP
#define COSTWAVES_PROJECT_SCAN_DIALOG_HPP

#include "ACAPinc.h"
#include "DGModule.hpp"

#include "ResourceIds.hpp"
#include "DataTypes.hpp"

namespace CostWaves {

// Fenêtre « Éléments du projet » (nouvelle architecture) : détecte ce qui est
// placé dans la maquette et l'évalue avec la bibliothèque de règles :
//  - chaque structure rencontrée (composite, profil, objet .gsm, matériau)
//    est regroupée en une ligne avec son nombre d'éléments ;
//  - les structures SANS article sont listées avec un triangle jaune ⚠ :
//    décider — « Assigner… » (donner un article) ou « Ignorer » ;
//  - « Assigner… » sur un composite/profil en mode « ses couches » ouvre la
//    fenêtre des couches pour donner des articles aux matériaux manquants.
// Chaque décision crée une règle dans la bibliothèque (valable pour les
// prochains projets).
class ProjectScanDialog final :	public DG::ModalDialog,
								public DG::ButtonItemObserver,
								public DG::ListBoxObserver
{
public:
	enum ItemIds {
		InfoTextId			= 1,
		TableId				= 2,
		RefreshButtonId		= 3,
		AssignButtonId		= 4,
		IgnoreButtonId		= 5,
		CreateArticleButtonId	= 6,
		StatusTextId		= 7,
		CloseButtonId		= 8
	};

	ProjectScanDialog ();

private:
	// DG::ButtonItemObserver
	virtual void	ButtonClicked (const DG::ButtonClickEvent& ev) override;

	// DG::ListBoxObserver
	virtual void	ListBoxSelectionChanged (const DG::ListBoxSelectionEvent& ev) override;

	// Une ligne = une structure rencontrée dans la maquette.
	struct ScanRow {
		CWStructureType			type = CWStructureType::Composite;
		GS::UniString			name;
		int						elementCount = 0;
		bool					hasRule = false;		// article résolu (ou couches complètes)
		bool					ignored = false;		// règle « Ignorer »
		GS::UniString			articleId;				// article « lui-même »
		GS::Array<GS::UniString>	layerMaterials;		// couches (mode « ses couches »)
		bool					layersKnown = false;
		bool					byLayers = false;		// règle en mode couches
		GS::Array<API_Guid>		elementGuids;			// pour découvrir les matériaux (profils)
	};

	void		ScanProject ();
	void		FillTable ();
	void		AssignSelected ();
	void		IgnoreSelected ();
	void		CreateArticle ();
	void		SaveLibrary ();
	void		UpdateStatus ();
	ScanRow*	SelectedRow ();

	DG::LeftText		infoText;
	DG::MultiSelListBox	table;
	DG::Button			refreshButton;
	DG::Button			assignButton;
	DG::Button			ignoreButton;
	DG::Button			createArticleButton;
	DG::LeftText		statusText;
	DG::Button			closeButton;

	GS::Array<CWMapRule>	rules;		// bibliothèque de travail
	GS::Array<CWArticle>	articles;	// base d'articles + locaux
	GS::Array<ScanRow>		scanRows;

	short	selectedRowIndex = 0;		// 1-based (0 = aucune)
	bool	isFilling = false;
};

} // namespace CostWaves

#endif // COSTWAVES_PROJECT_SCAN_DIALOG_HPP
