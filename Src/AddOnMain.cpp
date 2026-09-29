#include "CostWavesPrecompiledHeader.hpp"

#include "ResourceIds.hpp"
#include "RS.hpp"

#include "ProjectScanDialog.hpp"
#include "RulesManagerDialog.hpp"

namespace {

const GSResID	AddOnInfoID			= ID_ADDON_INFO;
const Int32		AddOnNameID			= 1;
const Int32		AddOnDescriptionID	= 2;

const short		AddOnMenuID			= ID_ADDON_MENU;
const Int32		ManagerCommandID	= 1;	// Gestionnaire de correspondances
const Int32		ScanCommandID		= 2;	// Éléments du projet

GSErrCode MenuCommandHandler (const API_MenuParams* menuParams)
{
	switch (menuParams->menuItemRef.menuResID) {
		case AddOnMenuID:
			switch (menuParams->menuItemRef.itemIndex) {
				case ManagerCommandID:
					{
						// Préparation (sans maquette ouverte) : composites et
						// profils (eux-mêmes ou leurs couches), objets .gsm,
						// matériaux -> articles CostWaves.
						CostWaves::RulesManagerDialog dialog;
						dialog.Invoke ();
					}
					break;

				case ScanCommandID:
					{
						// Projet : détection des éléments placés, ⚠ sur les
						// structures sans article -> assigner ou ignorer.
						CostWaves::ProjectScanDialog dialog;
						dialog.Invoke ();
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
	return ACAPI_MenuItem_InstallMenuHandler (AddOnMenuID, MenuCommandHandler);
#else
	return ACAPI_Install_MenuHandler (AddOnMenuID, MenuCommandHandler);
#endif
}

GSErrCode FreeData (void)
{
	return NoError;
}
