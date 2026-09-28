#ifndef COSTWAVES_SUMMARY_DIALOG_HPP
#define COSTWAVES_SUMMARY_DIALOG_HPP

#include "ACAPinc.h"
#include "DGModule.hpp"

#include "DataTypes.hpp"

namespace CostWaves {

// Fenêtre « Récapitulatif par article » (phase 4) :
//  - une ligne par article facturé : identifiant, libellé, unité,
//    nombre d'éléments et d'ensembles, quantité totale facturée
//  - fenêtre créée entièrement en code (pas de ressource GRC)
// Les articles à l'unité ENS (ou vide) sont facturés au forfait :
// quantité 1 par ligne facturée (élément ou ensemble).
class SummaryDialog final :	public DG::ModalDialog,
							public DG::ButtonItemObserver
{
public:
	// Identifiants libres (items créés programmatiquement).
	enum DialogItemIds {
		TableId		= 1,
		CloseButtonId	= 2
	};

	explicit SummaryDialog (const GS::Array<CWArticleSummary>& summary);

private:
	// DG::ButtonItemObserver
	virtual void	ButtonClicked (const DG::ButtonClickEvent& ev) override;

	void	InitTable ();
	void	Fill (const GS::Array<CWArticleSummary>& summary);

	DG::MultiSelListBox	table;
	DG::Button			closeButton;
};

} // namespace CostWaves

#endif // COSTWAVES_SUMMARY_DIALOG_HPP
