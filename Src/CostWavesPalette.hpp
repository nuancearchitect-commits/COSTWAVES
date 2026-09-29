#ifndef COSTWAVES_PALETTE_HPP
#define COSTWAVES_PALETTE_HPP

#include "ACAPinc.h"
#include "DGModule.hpp"

#include "ResourceIds.hpp"

namespace CostWaves {

// Palette CostWaves (menu NUANCE BIM > COSTWAVES) : point d'entrée de
// l'add-on. Deux boutons : « Correspondance matériaux » (fenêtre des
// correspondances attributs Archicad -> articles) et « Correspondance
// objets GDL » (objets de bibliothèque -> article + valeur clé GDL).
class CostWavesPalette final :	public DG::Palette,
								public DG::PanelObserver,
								public DG::ButtonItemObserver
{
public:
	enum ItemIds {
		InfoTextId		= 1,
		MappingButtonId	= 2,
		StatusTextId	= 3,
		GdlButtonId		= 4,
		InheritedButtonId = 5
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

	DG::LeftText	infoText;
	DG::Button		mappingButton;
	DG::Button		gdlButton;
	DG::Button		inheritedButton;
	DG::LeftText	statusText;

	static GS::Ref<CostWavesPalette>	instance;
	static const GS::Guid				paletteGuid;
};

} // namespace CostWaves

#endif // COSTWAVES_PALETTE_HPP
