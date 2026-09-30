#include "CostWavesPrecompiledHeader.hpp"

#include "CostWavesPalette.hpp"

#include "CostWavesStyle.hpp"
#include "GdlMappingDialog.hpp"
#include "InheritedArticlesDialog.hpp"
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
		titleText (GetReference (), TitleId),
		subtitleText (GetReference (), SubtitleId),
		sectionLabel (GetReference (), SectionLabelId),
		mappingButton (GetReference (), MappingButtonId),
		mappingDesc (GetReference (), MappingDescId),
		gdlButton (GetReference (), GdlButtonId),
		gdlDesc (GetReference (), GdlDescId),
		inheritedButton (GetReference (), InheritedButtonId),
		inheritedDesc (GetReference (), InheritedDescId),
		statusText (GetReference (), StatusTextId)
{
	Attach (*this);					// PanelObserver
	mappingButton.Attach (*this);	// ButtonItemObserver
	gdlButton.Attach (*this);
	inheritedButton.Attach (*this);

	// Couleurs de la maquette V4 : bande de titre sombre « rail » avec
	// titre blanc, sous-titre et descriptions gris, libellé de section
	// accent, ligne d'état en pastille (fond soft, texte accent).
	titleText.SetText (FR ("COSTWAVES"));
	CostWavesStyle::ApplyBand (titleText);
	subtitleText.SetText (FR ("Correspondances entre les attributs Archicad")
						  + FR (" et les articles CostWaves."));
	CostWavesStyle::ApplyHelp (subtitleText);
	sectionLabel.SetText (FR ("CORRESPONDANCES"));
	CostWavesStyle::ApplySectionLabel (sectionLabel);
	mappingDesc.SetText (FR ("Règles par attribut + valeur clé"));
	CostWavesStyle::ApplyHelp (mappingDesc);
	gdlDesc.SetText (FR ("Règles par objet + paramètre de longueur"));
	CostWavesStyle::ApplyHelp (gdlDesc);
	inheritedDesc.SetText (FR ("Booléen activé → article + valeur clé"));
	CostWavesStyle::ApplyHelp (inheritedDesc);
	statusText.SetText (FR ("Ouvrez une correspondance pour préparer les règles."));
	CostWavesStyle::ApplyStatusChip (statusText);

	// Active la distribution des événements aux observers attachés — SANS cet
	// appel, les clics sur les contrôles de la palette ne déclenchent RIEN.
	BeginEventProcessing ();
}


CostWavesPalette::~CostWavesPalette ()
{
}


void CostWavesPalette::PanelCloseRequested (const DG::PanelCloseRequestEvent& /*ev*/, bool* accepted)
{
	*accepted = true;
	HidePalette ();
}


void CostWavesPalette::PanelResized (const DG::PanelResizeEvent& ev)
{
	// La bande de titre et la pastille d'état suivent la largeur de la
	// palette (le reste reste ancré en haut à gauche).
	titleText.Resize (ev.GetHorizontalChange (), 0);
	statusText.Resize (ev.GetHorizontalChange (), 0);
}


void CostWavesPalette::ButtonClicked (const DG::ButtonClickEvent& ev)
{
	if (ev.GetSource () == &mappingButton) {
		MappingDialog dialog;
		dialog.Invoke ();
	} else if (ev.GetSource () == &gdlButton) {
		GdlMappingDialog dialog;
		dialog.Invoke ();
	} else if (ev.GetSource () == &inheritedButton) {
		InheritedArticlesDialog dialog;
		dialog.Invoke ();
	}
}

} // namespace CostWaves
