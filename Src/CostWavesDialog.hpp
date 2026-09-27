#ifndef COSTWAVES_COSTWAVES_DIALOG_HPP
#define COSTWAVES_COSTWAVES_DIALOG_HPP

#include "ACAPinc.h"
#include "DGModule.hpp"

#include "ResourceIds.hpp"
#include "DataTypes.hpp"

namespace CostWaves {

// Fenêtre principale (phase 1) :
//  - choix du système de classification
//  - tableau : Type | GUID | ID élément | Étage | Classe | Quantités
//  - panneau de détails (toutes les quantités + propriétés de composant)
//  - exports JSON / CSV
class CostWavesDialog final :	public DG::ModalDialog,
								public DG::PanelObserver,
								public DG::ButtonItemObserver,
								public DG::PopUpObserver,
								public DG::ListBoxObserver
{
public:
	enum DialogResourceIds {
		DialogResourceId	= ID_ADDON_DLG,

		SystemLabelId		= 1,
		SystemPopupId		= 2,
		RefreshButtonId		= 3,
		StatusTextId		= 4,
		TableId				= 5,
		DetailsGroupId		= 6,
		DetailText1Id		= 7,
		DetailText2Id		= 8,
		DetailText3Id		= 9,
		DetailText4Id		= 10,
		DetailText5Id		= 11,
		ExportJsonButtonId	= 12,
		ExportCsvButtonId	= 13,
		CloseButtonId		= 14
	};

	CostWavesDialog ();

private:
	// DG::PanelObserver
	virtual void	PanelResized (const DG::PanelResizeEvent& ev) override;

	// DG::ButtonItemObserver
	virtual void	ButtonClicked (const DG::ButtonClickEvent& ev) override;

	// DG::PopUpObserver
	virtual void	PopUpChanged (const DG::PopUpChangeEvent& ev) override;

	// DG::ListBoxObserver
	virtual void	ListBoxSelectionChanged (const DG::ListBoxSelectionEvent& ev) override;

	void	InitTable ();
	void	LoadSystems ();
	void	RefreshData ();
	void	FillTable ();
	void	UpdateDetails (short listItem);
	void	UpdateStatus ();
	void	Export (bool jsonFormat);
	void	SetDetailLine (short lineIndex, const GS::UniString& text);
	void	ClearDetails ();

	// Ligne d'affichage : référence vers un élément ou un de ses composants.
	struct DisplayRow {
		RowKind		kind = RowKind::Element;
		UIndex		elementIndex = 0;
		UIndex		componentIndex = 0;
	};

	DG::PopUp			systemPopup;
	DG::Button			refreshButton;
	DG::LeftText		statusText;
	DG::MultiSelListBox	table;
	DG::GroupBox		detailsGroup;
	DG::LeftText		detail1;
	DG::LeftText		detail2;
	DG::LeftText		detail3;
	DG::LeftText		detail4;
	DG::LeftText		detail5;
	DG::Button			exportJsonButton;
	DG::Button			exportCsvButton;
	DG::Button			closeButton;

	GS::Array<CWSystemInfo>	systems;
	API_Guid				selectedSystem = APINULLGuid;
	API_Guid				elemIdPropGuid = APINULLGuid;
	GS::UniString			elemIdPropNote;

	GS::Array<CWElementRow>	rows;
	CWScanReport			report;
	GS::Array<DisplayRow>	displayRows;

	bool					isFilling = false;
};

} // namespace CostWaves

#endif // COSTWAVES_COSTWAVES_DIALOG_HPP
