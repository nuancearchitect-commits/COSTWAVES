#ifndef COSTWAVES_PALETTE_HPP
#define COSTWAVES_PALETTE_HPP

#include "ACAPinc.h"
#include "DGModule.hpp"

#include "ResourceIds.hpp"

namespace CostWaves {

// Palette CostWaves (menu NUANCE BIM > COSTWAVES) : point d'entrée de
// l'add-on, dans l'esprit de la maquette CostWaves V4 — titre, sous-titre,
// label de section, boutons avec description, ligne d'état. Trois actions :
//  - « Matériaux, composites et profils… » : fenêtre des correspondances
//    attributs Archicad -> articles (classes) + valeurs clés ;
//  - « Objets GDL… » : correspondances objet de bibliothèque -> article
//    + valeur clé (paramètre GDL longueur) ;
//  - « Articles hérités… » : booléen activé (global) -> article
//    + valeur clé.
class CostWavesPalette final :	public DG::Palette,
								public DG::PanelObserver,
								public DG::ButtonItemObserver
{
public:
	enum ItemIds {
		TitleId				= 1,
		SubtitleId			= 2,
		SectionLabelId		= 3,
		MappingButtonId		= 4,
		MappingDescId		= 5,
		GdlButtonId			= 6,
		GdlDescId			= 7,
		InheritedButtonId	= 8,
		InheritedDescId		= 9,
		StatusTextId		= 10
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
	DG::LeftText	statusText;

	static GS::Ref<CostWavesPalette>	instance;
	static const GS::Guid				paletteGuid;
};

} // namespace CostWaves

#endif // COSTWAVES_PALETTE_HPP
