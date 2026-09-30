#include "CostWavesPrecompiledHeader.hpp"

#include "ResourceIds.hpp"
#include "RS.hpp"

#include "CostWavesPalette.hpp"

namespace {

const GSResID	AddOnInfoID			= ID_ADDON_INFO;
const Int32		AddOnNameID			= 1;
const Int32		AddOnDescriptionID	= 2;

const short		AddOnMenuID			= ID_ADDON_MENU;
const Int32		CostWavesCommandID	= 1;	// COSTWAVES (palette)

GSErrCode MenuCommandHandler (const API_MenuParams* menuParams)
{
	switch (menuParams->menuItemRef.menuResID) {
		case AddOnMenuID:
			switch (menuParams->menuItemRef.itemIndex) {
				case CostWavesCommandID:
					{
						// Palette CostWaves (modeless) : bascule afficher/masquer.
						if (CostWaves::CostWavesPalette::HasInstance ()
							&& CostWaves::CostWavesPalette::Instance ().IsVisible ()) {
							CostWaves::CostWavesPalette::Instance ().HidePalette ();
						} else {
							CostWaves::CostWavesPalette::Instance ().ShowPalette ();
						}
					}
					break;
			}
			break;
	}
	return NoError;
}

} // namespace

API_AddonType CheckEnvironment (API_EnvirParams* envir)
{
	RSGetIndString (&envir->addOnInfo.name, AddOnInfoID, AddOnNameID, ACAPI_GetOwnResModule ());
	RSGetIndString (&envir->addOnInfo.description, AddOnInfoID, AddOnDescriptionID, ACAPI_GetOwnResModule ());

	return APIAddon_Normal;
}

GSErrCode RegisterInterface (void)
{
#ifdef ServerMainVers_2700
	return ACAPI_MenuItem_RegisterMenu (AddOnMenuID, 0, MenuCode_UserDef, MenuFlag_Default);
#else
	return ACAPI_Register_Menu (AddOnMenuID, 0, MenuCode_UserDef, MenuFlag_Default);
#endif
}

GSErrCode Initialize (void)
{
#ifdef ServerMainVers_2700
	GSErrCode err = ACAPI_MenuItem_InstallMenuHandler (AddOnMenuID, MenuCommandHandler);
#else
	GSErrCode err = ACAPI_Install_MenuHandler (AddOnMenuID, MenuCommandHandler);
#endif

	// Palette flottante enregistrée auprès d'Archicad (mémorisation de la
	// position dans l'environnement de travail).
	err |= CostWaves::CostWavesPalette::RegisterPalette ();

	return err;
}

GSErrCode FreeData (void)
{
	return NoError;
}
