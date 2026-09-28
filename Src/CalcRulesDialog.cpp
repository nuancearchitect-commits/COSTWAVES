#include "CostWavesPrecompiledHeader.hpp"

#include "CalcRulesDialog.hpp"

#include "ArticleManager.hpp"

#include <cwchar>

namespace CostWaves {

namespace {

// Littéral UTF-8 -> GS::UniString (les sources sont compilees avec /utf-8).
GS::UniString FR (const char* utf8Text)
{
	return GS::UniString (utf8Text, CC_UTF8);
}

// La quantité (libellé + unité) est-elle déjà dans la liste ?
bool ContainsCandidate (const GS::Array<CWQuantity>& candidates, const CWQuantity& quantity)
{
	for (UIndex i = 0; i < candidates.GetSize (); ++i) {
		if (candidates[i].label == quantity.label && candidates[i].unit == quantity.unit)
			return true;
	}
	return false;
}

} // namespace


CalcRulesDialog::CalcRulesDialog (const GS::Array<CWArticle>& inArticles, const GS::Array<CWElementRow>& rows)
	:	DG::ModalDialog (ACAPI_GetOwnResModule (), ID_ADDON_DLG_CALC, ACAPI_GetOwnResModule ()),
		infoText (GetReference (), InfoTextId),
		table (GetReference (), TableId),
		quantityLabel (GetReference (), QuantityLabelId),
		quantityPopup (GetReference (), QuantityPopupId),
		formulaLabel (GetReference (), FormulaLabelId),
		formulaEdit (GetReference (), FormulaEditId),
		formulaHint (GetReference (), FormulaHintId),
		okButton (GetReference (), OkButtonId),
		cancelButton (GetReference (), CancelButtonId),
		articles (inArticles)
{
	infoText.SetText (FR ("Pour chaque article, choisissez la quantité à adopter pour le calcul du métré ")
					  + FR ("(Surface nette, Surface brute, Volume conditionné, Surface projetée…). ")
					  + FR ("Selon l'unité de l'article, seules les quantités lues dans le projet et compatibles sont proposées."));

	// Quantités candidates : union des libellés lus dans le projet (éléments
	// et skins classés), ordre de première apparition.
	for (UIndex r = 0; r < rows.GetSize (); ++r) {
		for (UIndex q = 0; q < rows[r].quantities.GetSize (); ++q) {
			if (!ContainsCandidate (candidateQuantities, rows[r].quantities[q]))
				candidateQuantities.Push (rows[r].quantities[q]);
		}
		for (UIndex c = 0; c < rows[r].components.GetSize (); ++c) {
			for (UIndex q = 0; q < rows[r].components[c].quantities.GetSize (); ++q) {
				if (!ContainsCandidate (candidateQuantities, rows[r].components[c].quantities[q]))
					candidateQuantities.Push (rows[r].components[c].quantities[q]);
			}
		}
	}

	FillTable ();

	okButton.Attach (*this);		// ButtonItemObserver
	cancelButton.Attach (*this);	// ButtonItemObserver
	table.Attach (*this);			// ListBoxObserver
	quantityPopup.Attach (*this);	// PopUpObserver
}


void CalcRulesDialog::FillTable ()
{
	const short columnCount = 4;

	table.SetHeaderItemCount (columnCount);
	table.SetTabFieldCount (columnCount);
	table.SetHeaderItemText (1, FR ("Article"));
	table.SetHeaderItemText (2, FR ("Libellé"));
	table.SetHeaderItemText (3, FR ("Unité"));
	table.SetHeaderItemText (4, FR ("Quantité à facturer"));

	const short widths[4] = { 130, 210, 60, 200 };
	short position = 0;
	for (short i = 1; i <= columnCount; ++i) {
		table.SetHeaderItemSize (i, widths[i - 1]);
		table.SetHeaderItemSizeableFlag (i, true);
		table.SetTabFieldProperties (i, position, static_cast<short> (position + widths[i - 1]),
									 DG::ListBox::Left, DG::ListBox::MiddleTruncate, i > 1);
		position = static_cast<short> (position + widths[i - 1]);
	}

	for (UIndex a = 0; a < articles.GetSize (); ++a) {
		const CWArticle& article = articles[a];
		table.AppendItem ();
		const short item = table.GetItemCount ();
		table.SetTabItemText (item, 1, article.id);
		table.SetTabItemText (item, 2, article.name);
		table.SetTabItemText (item, 3, article.unit.IsEmpty () ? FR ("ENS") : article.unit);
		table.SetTabItemText (item, 4, !article.calcFormula.IsEmpty ()
			? article.calcFormula
			: (article.calcQuantity.IsEmpty ()
				? FR ("Automatique (selon l'unité)")
				: article.calcQuantity));
	}

	if (table.GetItemCount () > 0) {
		table.SelectItem (1);
		selectedArticleIndex = 1;
	}

	UpdateQuantityPopup ();
}


void CalcRulesDialog::UpdateQuantityPopup ()
{
	while (quantityPopup.GetItemCount () > 0)
		quantityPopup.DeleteItem (1);
	popupLabels.Clear ();

	if (selectedArticleIndex < 1 || static_cast<UIndex> (selectedArticleIndex) > articles.GetSize ()) {
		quantityPopup.Disable ();
		return;
	}

	const CWArticle& article = articles[static_cast<UIndex> (selectedArticleIndex) - 1];

	// Formule dérivée de l'article courant.
	formulaEdit.SetText (article.calcFormula);

	// Articles facturés à l'ensemble : 1 par ligne, aucune quantité à choisir.
	if (ArticleManager::IsEnsUnit (article.unit)) {
		quantityPopup.AppendItem ();
		quantityPopup.SetItemText (1, FR ("(comptage : 1 par ligne / groupe)"));
		quantityPopup.SelectItem (1);
		quantityPopup.Disable ();
		formulaEdit.Disable ();
		return;
	}

	quantityPopup.Enable ();
	formulaEdit.Enable ();

	// Choix 1 : comportement automatique (première quantité de l'unité).
	quantityPopup.AppendItem ();
	quantityPopup.SetItemText (1, FR ("Automatique (selon l'unité)"));
	popupLabels.Push (GS::UniString ());

	// Puis les quantités lues dans le projet, compatibles avec l'unité de
	// l'article (« selon le type » : m² -> surfaces, m³ -> volumes,
	// m -> longueurs, périmètres…).
	const GS::UniString wantedUnit = ArticleManager::NormalizedUnit (article.unit);
	short itemForRule = 0;
	short item = 1;
	for (UIndex q = 0; q < candidateQuantities.GetSize (); ++q) {
		if (ArticleManager::NormalizedUnit (candidateQuantities[q].unit) != wantedUnit)
			continue;
		++item;
		quantityPopup.AppendItem ();
		quantityPopup.SetItemText (item, candidateQuantities[q].label
			+ FR (" (") + candidateQuantities[q].unit + FR (")"));
		popupLabels.Push (candidateQuantities[q].label);
		if (candidateQuantities[q].label == article.calcQuantity)
			itemForRule = item;
	}

	// La règle actuelle n'a pas été lue dans le projet : la conserver
	// néanmoins dans la liste pour ne pas la perdre silencieusement.
	if (!article.calcQuantity.IsEmpty () && itemForRule == 0) {
		++item;
		quantityPopup.AppendItem ();
		quantityPopup.SetItemText (item, article.calcQuantity + FR (" — non lue dans le projet"));
		popupLabels.Push (article.calcQuantity);
		itemForRule = item;
	}

	quantityPopup.SelectItem (itemForRule > 0 ? itemForRule : 1);
}


void CalcRulesDialog::ListBoxSelectionChanged (const DG::ListBoxSelectionEvent& ev)
{
	if (ev.GetSource () != &table)
		return;

	// La formule saisie appartient à l'article affiché jusque-là.
	CommitFormulaEdit ();

	selectedArticleIndex = table.GetSelectedItem ();
	UpdateQuantityPopup ();
}


void CalcRulesDialog::CommitFormulaEdit ()
{
	if (selectedArticleIndex < 1 || static_cast<UIndex> (selectedArticleIndex) > articles.GetSize ())
		return;
	articles[static_cast<UIndex> (selectedArticleIndex) - 1].calcFormula = formulaEdit.GetText ();
}


void CalcRulesDialog::PopUpChanged (const DG::PopUpChangeEvent& ev)
{
	if (ev.GetSource () != &quantityPopup)
		return;

	if (selectedArticleIndex < 1 || static_cast<UIndex> (selectedArticleIndex) > articles.GetSize ())
		return;

	CWArticle& article = articles[static_cast<UIndex> (selectedArticleIndex) - 1];
	if (ArticleManager::IsEnsUnit (article.unit))
		return;

	const short selection = quantityPopup.GetSelectedItem ();
	if (selection < 1 || static_cast<UIndex> (selection) > popupLabels.GetSize ())
		return;

	// Libellé porté par l'item (vide pour « Automatique »).
	article.calcQuantity = popupLabels[static_cast<UIndex> (selection) - 1];

	table.SetTabItemText (selectedArticleIndex, 4, !article.calcFormula.IsEmpty ()
		? article.calcFormula
		: (article.calcQuantity.IsEmpty ()
			? FR ("Automatique (selon l'unité)")
			: article.calcQuantity));
}


void CalcRulesDialog::ButtonClicked (const DG::ButtonClickEvent& ev)
{
	if (ev.GetSource () == &okButton) {
		CommitFormulaEdit ();

		// Valider chaque formule dérivée contre les quantités du projet :
		// une formule invalide bloque la fermeture avec un message clair.
		for (UIndex a = 0; a < articles.GetSize (); ++a) {
			if (articles[a].calcFormula.IsEmpty ())
				continue;
			GS::UniString formulaError;
			if (!ArticleManager::ValidateFormula (articles[a].calcFormula, candidateQuantities, formulaError)) {
				DG::WarningAlert (FR ("Formule invalide pour l'article ") + articles[a].id + FR (" :"),
								  formulaError
									  + FR ("\nLibellés disponibles : les quantités lues dans le projet ")
									  + FR ("(ex. « Contour ouverture * Épaisseur mur hôte »)."),
								  FR ("OK"));
				return;
			}
		}

		accepted = true;
		PostCloseRequest (DG::ModalDialog::Accept);
	} else if (ev.GetSource () == &cancelButton) {
		PostCloseRequest (DG::ModalDialog::Cancel);
	}
}

} // namespace CostWaves
