#include "CostWavesPrecompiledHeader.hpp"

#include "ResourceIds.hpp"
#include "RS.hpp"

#include "CostWavesDialog.hpp"

namespace {

const GSResID	AddOnInfoID			= ID_ADDON_INFO;
const Int32		AddOnNameID			= 1;
const Int32		AddOnDescriptionID	= 2;

const short		AddOnMenuID			= ID_ADDON_MENU;
const Int32		AddOnCommandID		= 1;

GSErrCode MenuCommandHandler (const API_MenuParams* menuParams)
{
	switch (menuParams->menuItemRef.menuResID) {
		case AddOnMenuID:
			switch (menuParams->menuItemRef.itemIndex) {
				case AddOnCommandID:
					{
						// Palette (modeless) : bascule afficher/masquer.
						// Elle ne bloque ni la navigation ni la sélection
						// dans Archicad (phase 5).
						if (CostWaves::CostWavesDialog::HasInstance ()
							&& CostWaves::CostWavesDialog::Instance ().IsVisible ()) {
							CostWaves::CostWavesDialog::Instance ().HidePalette ();
						} else {
							CostWaves::CostWavesDialog::Instance ().ShowPalette ();
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
	return ACAPI_MenuItem_RegisterMenu (AddOnMenuID, 0, MenuCode_Tools, MenuFlag_Default);
#else
	return ACAPI_Register_Menu (AddOnMenuID, 0, MenuCode_Tools, MenuFlag_Default);
#endif
}

GSErrCode Initialize (void)
{
#ifdef ServerMainVers_2700
	GSErrCode err = ACAPI_MenuItem_InstallMenuHandler (AddOnMenuID, MenuCommandHandler);
#else
	GSErrCode err = ACAPI_Install_MenuHandler (AddOnMenuID, MenuCommandHandler);
#endif

	// Palette flottante enregistrée auprès d'Archicad (messages de gestion,
	// mémorisation de la position dans l'environnement de travail).
	err |= CostWaves::CostWavesDialog::RegisterPalette ();

	// Suivi de la sélection : la palette s'actualise quand la sélection
	// change dans le plan (si « Sélection uniquement » est cochée).
	err |= ACAPI_Notification_CatchSelectionChange (CostWaves::CostWavesDialog::SelectionChangeHandler);

	return err;
}

GSErrCode FreeData (void)
{
	ACAPI_Notification_CatchSelectionChange (nullptr);
	return NoError;
}
