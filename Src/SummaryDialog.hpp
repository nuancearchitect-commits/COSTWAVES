#ifndef COSTWAVES_SUMMARY_DIALOG_HPP
#define COSTWAVES_SUMMARY_DIALOG_HPP

#include "ACAPinc.h"
#include "DGModule.hpp"

#include "ResourceIds.hpp"
#include "DataTypes.hpp"

namespace CostWaves {

// Fenêtre « Récapitulatif par article » :
//  - une ligne par article facturé : identifiant, libellé, unité,
//    nombre d'éléments, d'ensembles, de groupes et de skins classés,
//    quantité totale facturée
// Les articles à l'unité ENS (ou vide) sont facturés au forfait :
// quantité 1 par ligne facturée (élément ou ensemble). Les groupes
// numérotés comptent 1 par groupe ; les skins classés sont facturés sur
// l'article de leur matériau.
// Fenêtre modale définie en ressource GRC (ID_ADDON_DLG_SUMMARY).
class SummaryDialog final :	public DG::ModalDialog,
								public DG::ButtonItemObserver
{
public:
	enum ItemIds {
		TableId		= 1,
		CloseButtonId	= 2
	};

	SummaryDialog (const GS::Array<CWArticleSummary>& summary);

private:
	// DG::ButtonItemObserver
	virtual void	ButtonClicked (const DG::ButtonClickEvent& ev) override;

	void	InitTable (const GS::Array<CWArticleSummary>& summary);
	void	Fill (const GS::Array<CWArticleSummary>& summary);

	DG::MultiSelListBox	table;
	DG::Button			closeButton;
};

} // namespace CostWaves

#endif // COSTWAVES_SUMMARY_DIALOG_HPP
