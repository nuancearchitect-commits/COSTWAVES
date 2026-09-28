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

} // namespace


SummaryDialog::SummaryDialog (const GS::Array<CWArticleSummary>& summary)
	:	DG::ModalDialog (ACAPI_GetOwnResModule (), ID_ADDON_DLG_SUMMARY, ACAPI_GetOwnResModule ()),
		table (GetReference (), TableId),
		closeButton (GetReference (), CloseButtonId)
{
	closeButton.Attach (*this);

	InitTable (summary);
	Fill (summary);
}


void SummaryDialog::InitTable (const GS::Array<CWArticleSummary>& summary)
{
	const short columnCount = 8;

	table.SetHeaderItemCount (columnCount);
	table.SetHeaderItemText (1, FR ("Article"));
	table.SetHeaderItemText (2, FR ("Libellé"));
	table.SetHeaderItemText (3, FR ("Unité"));
	table.SetHeaderItemText (4, FR ("Éléments"));
	table.SetHeaderItemText (5, FR ("Ensembles"));
	table.SetHeaderItemText (6, FR ("Groupes"));
	table.SetHeaderItemText (7, FR ("Skins"));
	table.SetHeaderItemText (8, FR ("Quantité totale"));

	// Le GRC ne définit pas les colonnes : créer les champs de tabulation
	// (sans cela, seule la première colonne s'affiche).
	table.SetTabFieldCount (columnCount);

	// Largeurs automatiques (contenus + en-têtes) ; le total peut dépasser
	// la largeur du contrôle → scroll horizontal (HVScroll dans le GRC).
	std::vector<short> columnMax (static_cast<size_t> (columnCount) + 1, 0);
	auto trackWidth = [&columnMax, columnCount] (short column, const GS::UniString& text) {
		const short estimated = static_cast<short> (text.GetLength () * 7 + 18);
		if (column >= 1 && column <= columnCount && estimated > columnMax[column])
			columnMax[column] = estimated;
	};

	// Parcours des lignes pour mesurer les contenus.
	for (UIndex i = 0; i < summary.GetSize (); ++i) {
		const CWArticleSummary& entry = summary[i];
		trackWidth (1, entry.articleId);
		trackWidth (2, entry.articleName);
		trackWidth (3, entry.unit);
		trackWidth (4, GS::ToUniString (std::to_wstring (static_cast<int> (entry.elementCount))));
		trackWidth (5, GS::ToUniString (std::to_wstring (static_cast<int> (entry.groupCount))));
		trackWidth (6, GS::ToUniString (std::to_wstring (static_cast<int> (entry.numberedGroupCount))));
		trackWidth (7, GS::ToUniString (std::to_wstring (static_cast<int> (entry.skinCount))));
		trackWidth (8, FormatValue (entry.totalQuantity));
	}
	for (short i = 1; i <= columnCount; ++i) {
		trackWidth (i, table.GetHeaderItemText (i));
		if (columnMax[i] < 45)
			columnMax[i] = 45;
		if (columnMax[i] > 340)
			columnMax[i] = 340;
	}

	short position = 0;
	for (short i = 1; i <= columnCount; ++i) {
		const short width = columnMax[i];
		table.SetHeaderItemSize (i, width);
		table.SetHeaderItemSizeableFlag (i, true);
		table.SetTabFieldProperties (i, position, static_cast<short> (position + width),
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
			GS::ToUniString (std::to_wstring (static_cast<int> (entry.elementCount))));
		table.SetTabItemText (itemIndex, 5,
			GS::ToUniString (std::to_wstring (static_cast<int> (entry.groupCount))));
		table.SetTabItemText (itemIndex, 6,
			GS::ToUniString (std::to_wstring (static_cast<int> (entry.numberedGroupCount))));
		table.SetTabItemText (itemIndex, 7,
			GS::ToUniString (std::to_wstring (static_cast<int> (entry.skinCount))));
		table.SetTabItemText (itemIndex, 8, FormatValue (entry.totalQuantity));
	}
}


void SummaryDialog::ButtonClicked (const DG::ButtonClickEvent& ev)
{
	if (ev.GetSource () == &closeButton)
		PostCloseRequest (DG::ModalDialog::Cancel);
}

} // namespace CostWaves
