#ifndef COSTWAVES_MAPPING_DIALOG_HPP
#define COSTWAVES_MAPPING_DIALOG_HPP

#include "ACAPinc.h"
#include "DGModule.hpp"

#include "ResourceIds.hpp"
#include "DataTypes.hpp"

namespace CostWaves {

// Fenêtre « Gestionnaire de correspondances » (nouvelle architecture, spec §3–§8) :
//  - définit les règles SANS avoir besoin d'une maquette ouverte (les noms de
//    structures sont saisissables à la main ; « Parcourir… » liste les
//    structures de l'environnement Archicad courant) ;
//  - une règle : structure native (matériau, composite, profil, favori, objet
//    de bibliothèque) → article CostWaves → mode de métré (élément ou
//    composant) → quantité à adopter, ou « Ignorer » ;
//  - l'état signale les articles introuvables dans la base (remappage, spec §9) ;
//  - « Créer un article… » ajoute un article local (spec §10) ;
//  - enregistrement dans <Documents>/CostWaves-regles.json, réutilisable
//    entre les projets.
class MappingDialog final :\tpublic DG::ModalDialog,
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
		ModeLabelId			= 10,
		ModePopupId			= 11,
		QuantityLabelId		= 12,
		QuantityEditId		= 13,
		IgnoreCheckId		= 14,
		ApplyButtonId		= 15,
		DeleteButtonId		= 16,
		StatusTextId		= 17,
		CreateArticleButtonId	= 18,
		SaveButtonId		= 19,
		CloseButtonId		= 20
	};

	MappingDialog (const GS::Array<CWArticle>& inArticles, const GS::Array<CWMapRule>& inRules);

	bool						IsAccepted () const { return accepted; }
	const GS::Array<CWMapRule>&	GetRules () const { return rules; }
	const GS::Array<CWArticle>&	GetArticles () const { return articles; }

private:
	// DG::ButtonItemObserver
	virtual void	ButtonClicked (const DG::ButtonClickEvent& ev) override;

	// DG::ListBoxObserver
	virtual void	ListBoxSelectionChanged (const DG::ListBoxSelectionEvent& ev) override;

	// DG::PopUpObserver
	virtual void	PopUpChanged (const DG::PopUpChangeEvent& ev) override;

	void	FillTable ();
	void	LoadSelectedRuleToControls ();
	void	ApplyRuleFromControls ();
	void	DeleteSelectedRule ();
	void	RefreshArticlePopup (const GS::UniString& preferredArticleId);
	bool	SaveLibrary ();
	void	UpdateStatusText (const GS::UniString& message);

	DG::LeftText		infoText;
	DG::MultiSelListBox	table;
	DG::LeftText		typeLabel;
	DG::PopUp			typePopup;
	DG::LeftText		structureLabel;
	DG::TextEdit		structureEdit;
	DG::Button			browseButton;
	DG::LeftText		articleLabel;
	DG::PopUp			articlePopup;
	DG::LeftText		modeLabel;
	DG::PopUp			modePopup;
	DG::LeftText		quantityLabel;
	DG::TextEdit		quantityEdit;
	DG::CheckBox		ignoreCheck;
	DG::Button			applyButton;
	DG::Button			deleteButton;
	DG::LeftText		statusText;
	DG::Button			createArticleButton;
	DG::Button			saveButton;
	DG::Button			closeButton;

	GS::Array<CWMapRule>	rules;		// copie de travail (sauvegarde à l'enregistrement)
	GS::Array<CWArticle>	articles;	// catalogue (complété par « Créer un article… »)

	short	selectedRuleIndex = 0;		// 1-based (0 = aucune)
	bool	accepted = false;
	bool	isFilling = false;
};

} // namespace CostWaves

#endif // COSTWAVES_MAPPING_DIALOG_HPP
