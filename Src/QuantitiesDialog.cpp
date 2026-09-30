#include "CostWavesPrecompiledHeader.hpp"

#include "QuantitiesDialog.hpp"

#include "ArticleManager.hpp"
#include "CostWavesStyle.hpp"
#include "ModelReader.hpp"
#include "RuleLibrary.hpp"

#include "UniStringWStringConversion.hpp"

#include <cwchar>

namespace CostWaves {

namespace {

GS::UniString FR (const char* utf8Text)
{
	return GS::UniString (utf8Text, CC_UTF8);
}

// Quantité affichée avec 2 décimales.
GS::UniString FormatQuantity (double value)
{
	wchar_t buffer[64];
	swprintf (buffer, 64, L"%.2f", value);
	return GS::ToUniString (std::wstring (buffer));
}

// Libellé du mode de calcul (colonne « Mode calcul »).
GS::UniString CalcModeLabel (const CWQuantityLine& line)
{
	return FR (CWCalcModeLabel (line.calcMode));
}

} // namespace


QuantitiesDialog::QuantitiesDialog ()
	:	DG::ModalDialog (ACAPI_GetOwnResModule (), ID_ADDON_DLG_QUANTITIES, ACAPI_GetOwnResModule ()),
		infoText (GetReference (), InfoTextId),
		systemLabel (GetReference (), SystemLabelId),
		systemPopup (GetReference (), SystemPopupId),
		list (GetReference (), ListId),
		settingsText (GetReference (), SettingsTextId),
		calcLabel (GetReference (), CalcLabelId),
		calcValueText (GetReference (), CalcValueTextId),
		retainedLabel (GetReference (), RetainedLabelId),
		retainedEdit (GetReference (), RetainedEditId),
		applyRetainedButton (GetReference (), ApplyRetainedId),
		resetRetainedButton (GetReference (), ResetRetainedId),
		hintText (GetReference (), HintTextId),
		recalcButton (GetReference (), RecalcButtonId),
		closeButton (GetReference (), CloseButtonId)
{
	infoText.SetText (FR ("Contrôle des quantités avant export : une ligne par article")
					  + FR (" effectivement quantifié. Cliquez la colonne « Source »")
					  + FR (" pour la traçabilité complète ; modifiez unité, mode de")
					  + FR (" calcul et déductions — la quantité se recalcule."));

	// Système de classification : fournit les articles (avec leur unité).
	systems = ModelReader::GetClassificationSystems ();
	if (systems.IsEmpty ()) {
		systemPopup.AppendItem ();
		systemPopup.SetItemText (1, FR ("(aucun système)"));
	} else {
		for (UIndex s = 0; s < systems.GetSize (); ++s) {
			systemPopup.AppendItem ();
			systemPopup.SetItemText (systemPopup.GetItemCount (), systems[s].name);
		}
	}
	systemPopup.SelectItem (1);

	// Bibliothèque de correspondances (règles matériaux/objets GDL/booléens).
	GS::UniString rulesError;
	if (!RuleLibrary::LoadRules (rules, rulesError) && !rulesError.IsEmpty ())
		DG::WarningAlert (FR ("La bibliothèque de correspondances n'a pas pu être lue."),
						  rulesError, FR ("OK"));

	RunScan ();
	RebuildLines ();
	FillTable ();

	recalcButton.Attach (*this);
	closeButton.Attach (*this);
	applyRetainedButton.Attach (*this);
	resetRetainedButton.Attach (*this);
	systemPopup.Attach (*this);
	list.Attach (*this);
}


API_Guid QuantitiesDialog::CurrentSystemGuid () const
{
	const short selection = systemPopup.GetSelectedItem ();
	if (selection >= 1 && static_cast<UIndex> (selection) <= systems.GetSize ())
		return systems[static_cast<UIndex> (selection) - 1].guid;
	return APINULLGuid;
}


void QuantitiesDialog::RunScan ()
{
	rows.Clear ();
	report = CWScanReport ();

	if (systems.IsEmpty ())
		return;

	// Articles du système choisi (+ articles locaux + règles de calcul).
	articles.Clear ();
	const API_Guid systemGuid = CurrentSystemGuid ();
	if (systemGuid != APINULLGuid) {
		ArticleManager::CollectFromClassification (systemGuid, articles);
		GS::UniString localError;
		ArticleManager::AppendLocalArticles (articles, localError);	// best effort
		ArticleManager::LoadCalcRules (articles, localError);		// best effort
	}

	// Propriété intégrée « Element ID » (affichée dans la traçabilité).
	API_Guid elemIdPropGuid = APINULLGuid;
	GS::UniString elemIdNote;
	ModelReader::ResolveElementIdPropertyGuid (elemIdPropGuid, elemIdNote);

	const GSErrCode err = ModelReader::Scan (systemGuid, elemIdPropGuid, APINULLGuid, true,
											 rules, nullptr, rows, report);
	if (err != NoError) {
		DG::ErrorAlert (FR ("Lecture de la maquette impossible."),
						FR ("Le projet n'a pas pu être analysé (erreur ")
							+ GS::ToUniString (std::to_wstring (static_cast<int> (err)))
							+ FR (")."),
						FR ("OK"));
	}
}


void QuantitiesDialog::RebuildLines ()
{
	// Les réglages (unité, mode, déductions) et corrections manuelles de
	// l'affichage précédent sont conservés par article — la quantité est
	// toujours recalculée depuis la maquette.
	GS::Array<CWQuantityLine> previous = lines;
	ArticleManager::BuildQuantityLines (rows, articles, rules, CWQuantMode::Element,
										previous, lines);
}


CWQuantityLine* QuantitiesDialog::SelectedLine ()
{
	if (selectedRow < 1 || static_cast<UIndex> (selectedRow) > lines.GetSize ())
		return nullptr;
	return &lines[static_cast<UIndex> (selectedRow) - 1];
}


const CWQuantityLine* QuantitiesDialog::SelectedLine () const
{
	if (selectedRow < 1 || static_cast<UIndex> (selectedRow) > lines.GetSize ())
		return nullptr;
	return &lines[static_cast<UIndex> (selectedRow) - 1];
}


void QuantitiesDialog::FillTable ()
{
	isFilling = true;

	const short columnCount = 5;
	list.SetHeaderItemCount (columnCount);
	list.SetTabFieldCount (columnCount);
	list.SetHeaderItemText (1, FR ("Article"));
	list.SetHeaderItemText (2, FR ("Source"));
	list.SetHeaderItemText (3, FR ("Unité"));
	list.SetHeaderItemText (4, FR ("Mode calcul"));
	list.SetHeaderItemText (5, FR ("Quantité"));

	const short widths[5] = { 260, 250, 60, 110, 80 };
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

	for (UIndex l = 0; l < lines.GetSize (); ++l) {
		const CWQuantityLine& line = lines[l];
		list.AppendItem ();
		const short row = list.GetItemCount ();
		list.SetTabItemText (row, 1, line.articleId + FR (" — ") + line.articleName);

		// Source : la première origine + le nombre d'autres origines.
		if (line.sources.GetSize () > 0) {
			GS::UniString source = line.sources[0].text;
			if (line.sources.GetSize () > 1) {
				source += FR ("  (+") + GS::ToUniString (std::to_wstring (
					static_cast<int> (line.sources.GetSize () - 1))) + FR (")");
			}
			list.SetTabItemText (row, 2, source);
		} else {
			list.SetTabItemText (row, 2, FR ("—"));
		}

		list.SetTabItemText (row, 3, line.unit);
		list.SetTabItemText (row, 4, CalcModeLabel (line));
		// « * » = quantité corrigée manuellement (la calculée reste affichée
		// dans le panneau).
		list.SetTabItemText (row, 5, FormatQuantity (line.retainedQuantity)
										  + (line.manualOverride ? FR (" *") : GS::UniString ()));
	}

	if (selectedRow < 1 || selectedRow > list.GetItemCount ())
		selectedRow = list.GetItemCount () > 0 ? 1 : 0;
	if (selectedRow >= 1)
		list.SelectItem (selectedRow);

	isFilling = false;

	UpdateDetailPanel ();
}


void QuantitiesDialog::UpdateDetailPanel ()
{
	const CWQuantityLine* line = SelectedLine ();

	// Articles hérités (booléens activés) : comptage toujours visible ;
	// un échec de lecture des paramètres GDL n'est JAMAIS silencieux.
	GS::UniString inheritedNote;
	if (report.inheritedArticleRows > 0)
		inheritedNote += FR (" · ")
						 + GS::ToUniString (std::to_wstring (static_cast<int> (report.inheritedArticleRows)))
						 + FR (" article(s) hérité(s)");
	if (report.inheritedParamErrors > 0)
		inheritedNote += FR (" · ⚠ ")
						 + GS::ToUniString (std::to_wstring (static_cast<int> (report.inheritedParamErrors)))
						 + FR (" échec(s) de lecture des paramètres GDL");

	if (line == nullptr) {
		settingsText.SetText (GS::UniString ());
		calcValueText.SetText (FR ("—"));
		retainedEdit.SetText (GS::UniString ());
		hintText.SetText (FR ("Aucun article quantifié — vérifiez les correspondances")
						  + FR (" et la classification du projet.") + inheritedNote);
		return;
	}

	// Réglages de calcul : ils viennent de la CORRESPONDANCE (colonnes
	// Unité / Mode calcul / Déduit fenêtres / Déduit trous de la ligne,
	// fenêtres Matériaux, objets GDL, articles hérités) — affichage en
	// lecture seule ici.
	GS::UniString settings = FR ("Réglages de la correspondance — unité : ") + line->unit
		+ FR (" · mode : ") + FR (CWCalcModeLabel (line->calcMode));
	if (line->dimension == CWQtyDimension::Surface)
		settings += FR (" · déduit fenêtres : ") + (line->deductOpenings ? FR ("oui") : FR ("non"))
				 + FR (" · déduit trous : ") + (line->deductHoles ? FR ("oui") : FR ("non"));
	else if (line->dimension != CWQtyDimension::Unitary)
		settings += FR (" · déductions : surfaces uniquement");
	settingsText.SetText (settings);
	CostWavesStyle::ApplyHelp (settingsText);

	// Quantité calculée (jamais perdue) + note du mode courant.
	GS::UniString calc = FormatQuantity (line->calculatedQuantity) + FR (" ") + line->unit;
	if (!line->note.IsEmpty ())
		calc += FR ("   ·   ") + line->note;
	calcValueText.SetText (calc);

	retainedEdit.SetText (FormatQuantity (line->retainedQuantity));

	// Aide + comptage.
	hintText.SetText (GS::ToUniString (std::to_wstring (static_cast<int> (lines.GetSize ())))
					  + FR (" article(s) quantifié(s) · ")
					  + GS::ToUniString (std::to_wstring (static_cast<int> (line->elementCount)))
					  + FR (" élément(s) pour la sélection · cliquez la colonne")
					  + FR (" « Source » pour la traçabilité complète.") + inheritedNote);
}


void QuantitiesDialog::ShowTraceability (const CWQuantityLine& line)
{
	// Chaîne de traçabilité (spec : Article → Mapping → Source Archicad →
	// Paramètres → Mode de calcul → Quantité) — POURQUOI cette quantité.
	GS::UniString chain;
	chain += FR ("ARTICLE COSTWAVES\n");
	chain += FR ("  ") + line.articleId + FR (" — ") + line.articleName
		   + FR ("   (") + line.unit + FR (")\n");
	chain += FR ("\nMAPPING → SOURCES ARCHICAD\n");
	for (UIndex s = 0; s < line.sources.GetSize (); ++s) {
		const CWQuantitySource& source = line.sources[s];
		chain += FR ("  • ") + source.text;
		if (!source.detail.IsEmpty ())
			chain += FR ("   ·   ") + source.detail;
		chain += FR (" : ") + GS::ToUniString (std::to_wstring (static_cast<int> (source.count)))
			   + FR (" élément(s), ") + FormatQuantity (source.subtotal) + FR (" ") + line.unit
			   + FR ("\n");
	}
	chain += FR ("\nMODE DE CALCUL\n");
	chain += FR ("  ") + CalcModeLabel (line);
	if (!line.note.IsEmpty ())
		chain += FR ("   ·   ") + line.note;
	chain += FR ("\n");
	if (line.dimension == CWQtyDimension::Surface) {
		chain += FR ("\nPARAMÈTRES DE CALCUL\n");
		chain += FR ("  Déduire les ouvertures (fenêtres, portes) : ")
			   + (line.deductOpenings ? FR ("oui") : FR ("non")) + FR ("\n");
		chain += FR ("  Déduire les trous : ")
			   + (line.deductHoles ? FR ("oui") : FR ("non")) + FR ("\n");
	}
	chain += FR ("\nQUANTITÉS\n");
	chain += FR ("  Calculée : ") + FormatQuantity (line.calculatedQuantity)
		   + FR (" ") + line.unit + FR ("\n");
	chain += FR ("  Retenue : ") + FormatQuantity (line.retainedQuantity)
		   + FR (" ") + line.unit
		   + (line.manualOverride ? FR ("  (corrigée manuellement)")
								  : FR ("  (= calculée)"));

	DG::InformationAlert (FR ("Traçabilité — pourquoi cette quantité ?"), chain, FR ("OK"));
}


void QuantitiesDialog::ApplyRetainedValue ()
{
	CWQuantityLine* line = SelectedLine ();
	if (line == nullptr)
		return;

	// Correction manuelle : la valeur calculée d'origine reste conservée
	// (calculatedQuantity) et affichée dans le panneau.
	std::wstring text = GS::ToWString (retainedEdit.GetText ());
	for (wchar_t& ch : text) {
		if (ch == L',')
			ch = L'.';
	}

	wchar_t* end = nullptr;
	const double value = wcstod (text.c_str (), &end);
	if (text.empty () || end == text.c_str ()) {
		DG::WarningAlert (FR ("Quantité invalide."),
						  FR ("Saisissez un nombre (ex. 245.60 ou 245,60)."),
						  FR ("OK"));
		return;
	}

	line->manualOverride = true;
	line->retainedQuantity = value;

	FillTable ();
	UpdateDetailPanel ();
}


void QuantitiesDialog::ButtonClicked (const DG::ButtonClickEvent& ev)
{
	if (ev.GetSource () == &closeButton) {
		PostCloseRequest (DG::ModalDialog::Accept);
	} else if (ev.GetSource () == &recalcButton) {
		RunScan ();
		RebuildLines ();
		FillTable ();
	} else if (ev.GetSource () == &applyRetainedButton) {
		ApplyRetainedValue ();
	} else if (ev.GetSource () == &resetRetainedButton) {
		// Retour à la quantité calculée (la correction est retirée).
		CWQuantityLine* line = SelectedLine ();
		if (line != nullptr) {
			line->manualOverride = false;
			line->retainedQuantity = line->calculatedQuantity;
			FillTable ();
			UpdateDetailPanel ();
		}
	}
}


void QuantitiesDialog::ListBoxSelectionChanged (const DG::ListBoxSelectionEvent& ev)
{
	if (ev.GetSource () != &list || isFilling)
		return;

	const short newSelection = list.GetSelectedItem ();
	if (newSelection >= 1 && static_cast<UIndex> (newSelection) <= lines.GetSize ())
		selectedRow = newSelection;

	UpdateDetailPanel ();
}


void QuantitiesDialog::ListBoxClicked (const DG::ListBoxClickEvent& ev)
{
	if (ev.GetSource () != &list || isFilling)
		return;

	const short clicked = ev.GetListItem ();
	if (clicked < 1 || static_cast<UIndex> (clicked) > lines.GetSize ())
		return;

	selectedRow = clicked;

	// Colonne « Source » : traçabilité complète de la quantité.
	if (ev.GetTabFieldIndex () == 2) {
		ShowTraceability (lines[static_cast<UIndex> (clicked) - 1]);
		return;
	}

	UpdateDetailPanel ();
}


void QuantitiesDialog::PopUpChanged (const DG::PopUpChangeEvent& ev)
{
	if (isFilling)
		return;

	if (ev.GetSource () == &systemPopup) {
		// Changement de système : articles différents -> relecture complète.
		selectedRow = 0;
		RunScan ();
		RebuildLines ();
		FillTable ();
	}
}


} // namespace CostWaves
