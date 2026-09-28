#include "CostWavesPrecompiledHeader.hpp"

#include "SummaryDialog.hpp"

#include "UniStringWStringConversion.hpp"

#include <cwchar>

namespace CostWaves {

namespace {

// Littéral UTF-8 -> GS::UniString (les sources sont compilees avec /utf-8).
GS::UniString FR (const char* utf8Text)
{
	return GS::UniString (utf8Text, CC_UTF8);
}

// "0.1234" (séparateur point, zéros finaux retirés).
GS::UniString FormatValue (double value)
{
	wchar_t buffer[64];
	swprintf (buffer, 64, L"%.4f", value);

	std::wstring text (buffer);
	while (text.length () > 1 && text.back () == L'0')
		text.pop_back ();
	if (text.length () > 1 && text.back () == L'.')
		text.pop_back ();

	return GS::ToUniString (text);
}

// GUID de disposition de la fenêtre (fixe, pour la mémorisation écran).
const char* kSummaryDialogGuidString = "6F4A9C1E-8B2D-4E7A-9C3F-5D8E1B2A4C6D";

} // namespace


SummaryDialog::SummaryDialog (const GS::Array<CWArticleSummary>& summary)
	:	DG::ModalDialog (DG::NativePoint (80, 80), 600, 360,
						APIGuid2GSGuid (APIGuidFromString (kSummaryDialogGuidString)),
						DG::ModalDialog::NoGrow, DG::ModalDialog::TopCaption,
						DG::ModalDialog::NormalFrame),
		table (GetReference (), DG::Rect (10, 10, 590, 310),
			   DG::MultiSelListBox::VScroll, DG::MultiSelListBox::PartialItems,
			   DG::MultiSelListBox::Header, 21, DG::MultiSelListBox::Frame),
		closeButton (GetReference (), DG::Rect (490, 322, 590, 345))
{
	SetTitle (FR ("CostWaves — Récapitulatif par article"));
	closeButton.SetText (FR ("Fermer"));
	closeButton.Attach (*this);

	InitTable ();
	Fill (summary);
}


void SummaryDialog::InitTable ()
{
	const short columnCount = 5;

	table.SetHeaderItemCount (columnCount);
	table.SetHeaderItemText (1, FR ("Article"));
	table.SetHeaderItemText (2, FR ("Libellé"));
	table.SetHeaderItemText (3, FR ("Unité"));
	table.SetHeaderItemText (4, FR ("Éléments"));
	table.SetHeaderItemText (5, FR ("Quantité totale"));

	const short tableWidth = table.GetWidth ();
	const short proportions[columnCount] = { 14, 34, 10, 14, 28 };

	const short totalProportion = 100;
	short position = 0;
	for (short i = 1; i <= columnCount; ++i) {
		const short width = static_cast<short> ((tableWidth * proportions[i - 1]) / totalProportion);
		table.SetHeaderItemSize (i, width);
		table.SetHeaderItemSizeableFlag (i, true);
		table.SetTabFieldProperties (i, position, position + width,
									 DG::ListBox::Left, DG::ListBox::MiddleTruncate, i > 1);
		position = static_cast<short> (position + width);
	}
}


void SummaryDialog::Fill (const GS::Array<CWArticleSummary>& summary)
{
	for (UIndex i = 0; i < summary.GetSize (); ++i) {
		const CWArticleSummary& entry = summary[i];

		table.AppendItem ();
		const short itemIndex = table.GetItemCount ();
		table.SetTabItemText (itemIndex, 1, entry.articleId);
		table.SetTabItemText (itemIndex, 2, entry.articleName);
		table.SetTabItemText (itemIndex, 3, entry.unit);
		table.SetTabItemText (itemIndex, 4,
			GS::ToUniString (std::to_wstring (static_cast<int> (entry.elementCount)))
			+ (entry.groupCount > 0
				? FR (" + ") + GS::ToUniString (std::to_wstring (static_cast<int> (entry.groupCount))) + FR (" ens.")
				: GS::UniString ()));
		table.SetTabItemText (itemIndex, 5, FormatValue (entry.totalQuantity));
	}
}


void SummaryDialog::ButtonClicked (const DG::ButtonClickEvent& ev)
{
	if (ev.GetSourceId () == CloseButtonId)
		PostCloseRequest (DG::ModalDialog::Cancel);
}

} // namespace CostWaves
