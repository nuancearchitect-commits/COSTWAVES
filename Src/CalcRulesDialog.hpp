#ifndef COSTWAVES_CALC_RULES_DIALOG_HPP
#define COSTWAVES_CALC_RULES_DIALOG_HPP

#include "ACAPinc.h"
#include "DGModule.hpp"

#include "ResourceIds.hpp"
#include "DataTypes.hpp"

namespace CostWaves {

// Fenêtre « Règles de calcul » (logique de calcul du métré) :
//  - liste les articles (import JSON ou classification) avec, pour chacun,
//    la quantité à adopter pour la facturation ;
//  - selon le type de l'article (son unité), le popup ne propose que les
//    quantités compatibles lues dans le projet : pour un article au m² ->
//    Surface nette, Surface brute, Surface projetée… ; au m³ -> Volume,
//    Volume conditionné… ; au ml -> Longueur, Périmètre, Circonférence… ;
//  - « Automatique (selon l'unité) » = première quantité de l'unité
//    (comportement historique) ;
//  - les articles à l'ensemble (ENS) facturent 1 par ligne : pas de règle.
// Fenêtre modale définie en ressource GRC (ID_ADDON_DLG_CALC).
class CalcRulesDialog final :	public DG::ModalDialog,
							public DG::ButtonItemObserver,
							public DG::ListBoxObserver,
							public DG::PopUpObserver
{
public:
	enum ItemIds {
		InfoTextId		= 1,
		TableId			= 2,
		QuantityLabelId	= 3,
		QuantityPopupId	= 4,
		OkButtonId		= 5,
		CancelButtonId	= 6,
		FormulaLabelId	= 7,
		FormulaEditId	= 8,
		FormulaHintId	= 9
	};

	CalcRulesDialog (const GS::Array<CWArticle>& inArticles, const GS::Array<CWElementRow>& rows);

	bool					IsAccepted () const { return accepted; }
	const GS::Array<CWArticle>&	GetArticles () const { return articles; }

private:
	// DG::ButtonItemObserver
	virtual void	ButtonClicked (const DG::ButtonClickEvent& ev) override;

	// DG::ListBoxObserver
	virtual void	ListBoxSelectionChanged (const DG::ListBoxSelectionEvent& ev) override;

	// DG::PopUpObserver
	virtual void	PopUpChanged (const DG::PopUpChangeEvent& ev) override;

	void	FillTable ();
	void	UpdateQuantityPopup ();
	void	CommitFormulaEdit ();

	DG::LeftText		infoText;
	DG::MultiSelListBox	table;
	DG::LeftText		quantityLabel;
	DG::PopUp			quantityPopup;
	DG::LeftText		formulaLabel;
	DG::TextEdit		formulaEdit;
	DG::LeftText		formulaHint;
	DG::Button			okButton;
	DG::Button			cancelButton;

	// Copie de travail : les règles ne sont appliquées qu'à l'OK.
	GS::Array<CWArticle>	articles;

	// Quantités candidates du scan courant (libellé + unité, distinctes,
	// ordre de première apparition), éléments ET skins.
	GS::Array<CWQuantity>	candidateQuantities;

	// Libellé pur de chaque item du popup (index 0 = « Automatique » ->
	// chaîne vide) : évite toute re-découpe du texte affiché.
	GS::Array<GS::UniString>	popupLabels;

	short	selectedArticleIndex = 0;	// 1-based (0 = aucune sélection)
	bool	accepted = false;
};

} // namespace CostWaves

#endif // COSTWAVES_CALC_RULES_DIALOG_HPP
