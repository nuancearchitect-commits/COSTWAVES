#include "CostWavesPrecompiledHeader.hpp"

#include "FormulaEditorDialog.hpp"

#include "CostWavesStyle.hpp"

namespace CostWaves {

namespace {

GS::UniString FR (const char* utf8Text)
{
	return GS::UniString (utf8Text, CC_UTF8);
}

} // namespace


FormulaEditorDialog::FormulaEditorDialog (const GS::UniString& inObjectName,
										  const GS::Array<CWKeyEntry>& inVariables,
										  const GS::UniString& inCurrentFormula)
	:	DG::ModalDialog (ACAPI_GetOwnResModule (), ID_ADDON_DLG_FORMULA, ACAPI_GetOwnResModule ()),
		infoText (GetReference (), InfoTextId),
		formulaLabel (GetReference (), FormulaLabelId),
		formulaEdit (GetReference (), FormulaEditId),
		varsLabel (GetReference (), VarsLabelId),
		list (GetReference (), ListId),
		clearButton (GetReference (), ClearButtonId),
		cancelButton (GetReference (), CancelButtonId),
		okButton (GetReference (), OkButtonId),
		variables (inVariables),
		formula (inCurrentFormula)
{
	GS::UniString intro;
	if (inObjectName.IsEmpty ())
		intro = FR ("Quantité de la règle : composez la formule depuis les quantités")
				+ FR (" Archicad de l'élément (une fenêtre porte celles de son")
				+ FR (" mur hôte).");
	else
		intro = FR ("Quantité de la règle : composez la formule depuis les paramètres")
				+ FR (" GDL de « ") + inObjectName + FR (" » et les quantités Archicad")
				+ FR (" de l'élément (une fenêtre porte celles de son mur hôte).");
	intro += FR (" Opérations + − * / et parenthèses ; le résultat est dans")
			 + FR (" l'unité de la règle (colonne « Unité »).");
	infoText.SetText (intro);
	CostWavesStyle::ApplyHelp (infoText);
	CostWavesStyle::ApplyFieldLabel (formulaLabel);
	CostWavesStyle::ApplyFieldLabel (varsLabel);

	formulaEdit.SetText (formula);

	FillList ();

	okButton.Attach (*this);
	cancelButton.Attach (*this);
	clearButton.Attach (*this);
	list.Attach (*this);
}


void FormulaEditorDialog::FillList ()
{
	// Colonnes : Libellé | Insérer | Origine — le texte inséré est le nom
	// GDL stable (paramètres) ou le libellé exact de la quantité Archicad.
	const short columnCount = 3;
	list.SetHeaderItemCount (columnCount);
	list.SetTabFieldCount (columnCount);
	list.SetHeaderItemText (1, FR ("Libellé"));
	list.SetHeaderItemText (2, FR ("Insérer"));
	list.SetHeaderItemText (3, FR ("Origine"));

	const short widths[3] = { 200, 170, 170 };
	short position = 0;
	for (short i = 1; i <= columnCount; ++i) {
		list.SetHeaderItemSize (i, widths[i - 1]);
		list.SetHeaderItemSizeableFlag (i, true);
		list.SetTabFieldProperties (i, position, static_cast<short> (position + widths[i - 1]),
									DG::ListBox::Left, DG::ListBox::MiddleTruncate, i > 1);
		position = static_cast<short> (position + widths[i - 1]);
	}

	while (list.GetItemCount () > 0)
		list.DeleteItem (1);

	for (UIndex v = 0; v < variables.GetSize (); ++v) {
		list.AppendItem ();
		const short item = list.GetItemCount ();
		list.SetTabItemText (item, 1, variables[v].name);
		list.SetTabItemText (item, 2, variables[v].id);
		CostWavesStyle::CellAccent (list, item, 2);
		list.SetTabItemText (item, 3, variables[v].group);
		CostWavesStyle::CellMuted (list, item, 3);
	}
}


void FormulaEditorDialog::InsertSelectedVariable ()
{
	const short item = list.GetSelectedItem ();
	if (item < 1 || static_cast<UIndex> (item) > variables.GetSize ())
		return;

	const GS::UniString& insertText = variables[static_cast<UIndex> (item) - 1].id;
	GS::UniString current = formulaEdit.GetText ();
	if (!current.IsEmpty () && !current.EndsWith (FR (" ")))
		current += FR (" ");
	current += insertText;
	formulaEdit.SetText (current);
}


void FormulaEditorDialog::Accept ()
{
	formula = formulaEdit.GetText ();
	accepted = true;
	PostCloseRequest (DG::ModalDialog::Accept);
}


void FormulaEditorDialog::ButtonClicked (const DG::ButtonClickEvent& ev)
{
	if (ev.GetSource () == &okButton) {
		Accept ();
	} else if (ev.GetSource () == &cancelButton) {
		PostCloseRequest (DG::ModalDialog::Cancel);
	} else if (ev.GetSource () == &clearButton) {
		// « Vider » efface la formule (quantité par défaut) ; il reste à
		// valider par « OK ».
		formulaEdit.SetText (GS::UniString ());
	}
}


void FormulaEditorDialog::ListBoxDoubleClicked (const DG::ListBoxDoubleClickEvent& ev)
{
	if (ev.GetSource () != &list)
		return;
	InsertSelectedVariable ();
}

} // namespace CostWaves
