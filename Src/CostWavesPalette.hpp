#ifndef COSTWAVES_PALETTE_HPP
#define COSTWAVES_PALETTE_HPP

#include "ACAPinc.h"
#include "DGModule.hpp"

#include "ResourceIds.hpp"

namespace CostWaves {

// Palette CostWaves (menu NUANCE BIM > COSTWAVES) : point d'entrée de
// l'add-on, aux COULEURS de la maquette CostWaves V4 — bande de titre
// sombre « rail » (#101826) avec titre blanc, libellé de section accent
// (#155eef), descriptions gris, ligne d'état en pastille « chip » (fond
// soft #e8f0fe, texte accent). La bande et la pastille suivent la
// largeur quand la palette est redimensionnée. Trois actions :
//  - « Matériaux, composites et profils… » : fenêtre des correspondances
//    attributs Archicad -> articles (classes) + valeurs clés ;
//  - « Objets GDL… » : correspondances objet de bibliothèque -> article
//    + valeur clé (paramètre GDL longueur) ;
//  - « Articles hérités… » : booléen activé (global) -> article
//    + valeur clé ;
//  - « Quantitatif… » : fenêtre de contrôle et calcul des quantités
//    avant export (tableau Article | Source | Unité | Mode calcul |
//    Quantité, traçabilité par article, mode Brute/Conditionnelle/Nette,
//    correction manuelle).
class CostWavesPalette final :	public DG::Palette,
					public DG::PanelObserver,
					public DG::ButtonItemObserver
{
public:
	enum ItemIds {
		TitleId					= 1,
		SubtitleId				= 2,
		SectionLabelId			= 3,
		MappingButtonId			= 4,
		MappingDescId			= 5,
		GdlButtonId				= 6,
		GdlDescId				= 7,
		InheritedButtonId		= 8,
		InheritedDescId			= 9,
		StatusTextId			= 10,
		QuantitativeButtonId	= 11,
		QuantitativeDescId		= 12
	};

	// Singleton : la palette vit aussi longtemps que l'add-on.
	static bool					HasInstance ();
	static CostWavesPalette&	Instance ();
	static GSErrCode			RegisterPalette ();		// ACAPI_RegisterModelessWindow (Initialize)

	void	ShowPalette ();
	void	HidePalette ();

	// Public : la suppression est opérée par GS::Ref (singleton statique).
	~CostWavesPalette ();

private:
	CostWavesPalette ();

	// DG::PanelObserver
	virtual void	PanelCloseRequested (const DG::PanelCloseRequestEvent& ev, bool* accepted) override;
	virtual void	PanelResized (const DG::PanelResizeEvent& ev) override;

	// DG::ButtonItemObserver
	virtual void	ButtonClicked (const DG::ButtonClickEvent& ev) override;

	static GSErrCode	PaletteControlCallBack (Int32 paletteId, API_PaletteMessageID messageID, GS::IntPtr param);

	DG::LeftText	titleText;
	DG::LeftText	subtitleText;
	DG::LeftText	sectionLabel;
	DG::Button		mappingButton;
	DG::LeftText	mappingDesc;
	DG::Button		gdlButton;
	DG::LeftText	gdlDesc;
	DG::Button		inheritedButton;
	DG::LeftText	inheritedDesc;
	DG::Button		quantitativeButton;
	DG::LeftText	quantitativeDesc;
	DG::LeftText	statusText;

	static GS::Ref<CostWavesPalette>	instance;
	static const GS::Guid				paletteGuid;
};

} // namespace CostWaves

#endif // COSTWAVES_PALETTE_HPP
