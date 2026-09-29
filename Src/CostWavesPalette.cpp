#include "CostWavesPrecompiledHeader.hpp"

#include "CostWavesPalette.hpp"

#include "MappingDialog.hpp"

namespace CostWaves {

namespace {

GS::UniString FR (const char* utf8Text)
{
	return GS::UniString (utf8Text, CC_UTF8);
}

} // namespace


const GS::Guid CostWavesPalette::paletteGuid ("{2F7C4D18-A5B3-4E69-9C0D-71E8B4A2D935");
GS::Ref<CostWavesPalette> CostWavesPalette::instance;


bool CostWavesPalette::HasInstance ()
{
	return instance != nullptr;
}


CostWavesPalette& CostWavesPalette::Instance ()
{
	if (!HasInstance ())
		instance = new CostWavesPalette ();
	return *instance;
}


GSErrCode CostWavesPalette::PaletteControlCallBack (Int32 /*paletteId*/, API_PaletteMessageID messageID, GS::IntPtr param)
{
	switch (messageID) {
		case APIPalMsg_OpenPalette:
			Instance ().ShowPalette ();
			break;

		case APIPalMsg_ClosePalette:
			if (HasInstance ())
				Instance ().HidePalette ();
			break;

		case APIPalMsg_HidePalette_Begin:
			if (HasInstance () && Instance ().IsVisible ())
				Instance ().HidePalette ();
			break;

		case APIPalMsg_HidePalette_End:
			if (HasInstance () && !Instance ().IsVisible ())
				Instance ().ShowPalette ();
			break;

		case APIPalMsg_IsPaletteVisible:
			*(reinterpret_cast<bool*> (param)) = HasInstance () && Instance ().IsVisible ();
			break;

		default:
			break;
	}

	return NoError;
}


GSErrCode CostWavesPalette::RegisterPalette ()
{
	return ACAPI_RegisterModelessWindow (
				static_cast<Int32> (GS::CalculateHashValue (paletteGuid)),
				PaletteControlCallBack,
				API_PalEnabled_FloorPlan + API_PalEnabled_Section + API_PalEnabled_Elevation +
				API_PalEnabled_InteriorElevation + API_PalEnabled_3D + API_PalEnabled_Detail +
				API_PalEnabled_Worksheet + API_PalEnabled_Layout + API_PalEnabled_DocumentFrom3D,
				GSGuid2APIGuid (paletteGuid));
}


void CostWavesPalette::ShowPalette ()
{
	DG::Palette::Show ();
}


void CostWavesPalette::HidePalette ()
{
	DG::Palette::Hide ();
}


CostWavesPalette::CostWavesPalette ()
	:	DG::Palette (ACAPI_GetOwnResModule (), ID_ADDON_DLG, ACAPI_GetOwnResModule (), paletteGuid),
		infoText (GetReference (), InfoTextId),
		mappingButton (GetReference (), MappingButtonId),
		statusText (GetReference (), StatusTextId)
{
	infoText.SetText (FR ("CostWaves — correspondances entre les attributs Archicad ")
					  + FR ("et les articles CostWaves."));
	statusText.SetText (FR ("Ouvrez « Correspondance » pour préparer les règles."));

	mappingButton.Attach (*this);		// ButtonItemObserver
}


CostWavesPalette::~CostWavesPalette ()
{
}


void CostWavesPalette::PanelCloseRequested (const DG::PanelCloseRequestEvent& /*ev*/, bool* accepted)
{
	*accepted = true;
	HidePalette ();
}


void CostWavesPalette::ButtonClicked (const DG::ButtonClickEvent& ev)
{
	if (ev.GetSource () == &mappingButton) {
		MappingDialog dialog;
		dialog.Invoke ();
	}
}

} // namespace CostWaves
