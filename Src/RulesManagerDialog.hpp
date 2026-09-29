#ifndef COSTWAVES_RULES_MANAGER_DIALOG_HPP
#define COSTWAVES_RULES_MANAGER_DIALOG_HPP

#include "ACAPinc.h"
#include "DGModule.hpp"

#include "ResourceIds.hpp"
#include "DataTypes.hpp"

namespace CostWaves {

// Gestionnaire de correspondances (nouvelle architecture) — LA fenêtre de
// préparation, utilisable SANS maquette ouverte :
//
//  - Composites et profils : métré « Lui-même » (1 article pour la structure,
//    quantités de l'élément) ou « Ses couches » (quantités des matériaux) ;
//    dans ce dernier cas, les couches reçoivent des articles si elles n'en
//    ont pas déjà (fenêtre Couches).
//  - Objets de bibliothèque (.gsm posables, pas les macros) : 1 article.
//  - Matériaux : 1 article (utilisé par le mode « Ses couches »).
//  - Une structure peut être « Ignorée ».
//
// La bibliothèque est enregistrée dans <Documents>/CostWaves-regles.json,
// indépendante des projets et réutilisable. La base d'articles est chargée
// automatiquement depuis <Documents>/CostWaves-base.json (+ articles locaux).
class RulesManagerDialog final :	public DG::ModalDialog,
									public DG::ButtonItemObserver,
									public DG::ListBoxObserver,
									public DG::PopUpObserver
{
public:
	enum ItemIds {
		InfoTextId			= 1,
		TableId				= 2,
		TypeLabelId			= 3,
		TypePopupId			= 4,
		StructureLabelId	= 5,
		StructureEditId		= 6,
		BrowseButtonId		= 7,
		ArticleLabelId		= 8,
		ArticlePopupId		= 9,
		CreateArticleButtonId	= 10,
		ModeLabelId			= 11,
		ModePopupId			= 12,
		IgnoreCheckId		= 13,
		ApplyButtonId		= 14,
		DeleteButtonId		= 15,
		StatusTextId		= 16,
		ImportButtonId		= 17,
		SaveButtonId		= 18,
		CloseButtonId		= 19
	};

	RulesManagerDialog ();

private:
	// DG::ButtonItemObserver
	virtual void	ButtonClicked (const DG::ButtonClickEvent& ev) override;

	// DG::ListBoxObserver
	virtual void	ListBoxSelectionChanged (const DG::ListBoxSelectionEvent& ev) override;

	// DG::PopUpObserver
	virtual void	PopUpChanged (const DG::PopUpChangeEvent& ev) override;

	void	FillTable ();
	void	LoadSelectedRuleToControls ();
	void	RefreshArticlePopup (const GS::UniString& preferredArticleId);
	void	ApplyRuleFromControls ();
	void	DeleteSelectedRule ();
	void	BrowseStructure ();
	void	ImportBase ();
	void	CreateArticle ();
	bool	SaveLibrary ();
	void	UpdateStatusText (const GS::UniString& message);
	void	UpdateModePopupState ();

	DG::LeftText		infoText;
	DG::MultiSelListBox	table;
	DG::LeftText		typeLabel;
	DG::PopUp			typePopup;
	DG::LeftText		structureLabel;
	DG::TextEdit		structureEdit;
	DG::Button			browseButton;
	DG::LeftText		articleLabel;
	DG::PopUp			articlePopup;
	DG::Button			createArticleButton;
	DG::LeftText		modeLabel;
	DG::PopUp			modePopup;
	DG::CheckBox		ignoreCheck;
	DG::Button			applyButton;
	DG::Button			deleteButton;
	DG::LeftText		statusText;
	DG::Button			importButton;
	DG::Button			saveButton;
	DG::Button			closeButton;

	GS::Array<CWMapRule>	rules;		// copie de travail (enregistrée à la demande)
	GS::Array<CWArticle>	articles;	// base d'articles + locaux

	short	selectedRuleIndex = 0;		// 1-based (0 = aucune)
	bool	isFilling = false;
};

} // namespace CostWaves

#endif // COSTWAVES_RULES_MANAGER_DIALOG_HPP
