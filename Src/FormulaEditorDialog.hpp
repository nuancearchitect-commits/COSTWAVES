#ifndef COSTWAVES_FORMULA_EDITOR_DIALOG_HPP
#define COSTWAVES_FORMULA_EDITOR_DIALOG_HPP

#include "ACAPinc.h"
#include "DGModule.hpp"

#include "ResourceIds.hpp"
#include "DataTypes.hpp"

namespace CostWaves {

// Éditeur de la formule de QUANTITÉ d'une règle (objets GDL, articles
// hérités — ces types n'ont NI mode de calcul NI déductions : leur
// quantité est une formule). L'utilisateur compose l'expression depuis
// les variables proposées (paramètres GDL de l'objet + quantités
// Archicad de l'élément — une fenêtre porte celles de son mur hôte) ;
// le double-clic sur une variable l'insère dans la formule.
// Opérations acceptées : + − * / (et × · ÷), parenthèses, nombres.
// Le résultat est dans l'unité de la règle (colonne « Unité ») ;
// une formule vide = quantité par défaut (première de l'unité / comptage).
class FormulaEditorDialog final :	public DG::ModalDialog,
								public DG::ButtonItemObserver,
								public DG::ListBoxObserver
{
public:
	enum ItemIds {
		InfoTextId		= 1,
		FormulaLabelId	= 2,
		FormulaEditId	= 3,
		VarsLabelId		= 4,
		ListId			= 5,
		ClearButtonId	= 6,
		CancelButtonId	= 7,
		OkButtonId		= 8
	};

	// inObjectName : objet dont les paramètres GDL sont proposés (peut être
	// vide si l'objet d'origine n'est plus chargé — seules les quantités
	// Archicad sont proposées). inVariables : variables prêtes à insérer
	// (id = texte inséré, group = origine, name = libellé lisible).
	FormulaEditorDialog (const GS::UniString& inObjectName,
						 const GS::Array<CWKeyEntry>& inVariables,
						 const GS::UniString& inCurrentFormula);

	bool			IsAccepted () const { return accepted; }
	GS::UniString	GetFormula () const { return formula; }

private:
	// DG::ButtonItemObserver
	virtual void	ButtonClicked (const DG::ButtonClickEvent& ev) override;

	// DG::ListBoxObserver
	virtual void	ListBoxDoubleClicked (const DG::ListBoxDoubleClickEvent& ev) override;

	void	FillList ();
	void	InsertSelectedVariable ();
	void	Accept ();

	DG::LeftText		infoText;
	DG::LeftText		formulaLabel;
	DG::TextEdit		formulaEdit;
	DG::LeftText		varsLabel;
	DG::MultiSelListBox	list;
	DG::Button			clearButton;
	DG::Button			cancelButton;
	DG::Button			okButton;

	GS::Array<CWKeyEntry>	variables;
	GS::UniString			formula;
	bool					accepted = false;
};

} // namespace CostWaves

#endif // COSTWAVES_FORMULA_EDITOR_DIALOG_HPP
