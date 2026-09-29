#include "CostWavesPrecompiledHeader.hpp"

#include "GdlItemPickerDialog.hpp"

namespace CostWaves {

namespace {

GS::UniString FR (const char* utf8Text)
{
	return GS::UniString (utf8Text, CC_UTF8);
}

} // namespace


GdlItemPickerDialog::GdlItemPickerDialog (short inDialogResourceId,
										  const GS::UniString& inHeader1, const GS::UniString& inHeader2,
										  const GS::Array<GS::Pair<GS::UniString, GS::UniString>>& inItems,
										  bool inAllowNone)
	:	DG::ModalDialog (ACAPI_GetOwnResModule (), inDialogResourceId, ACAPI_GetOwnResModule ()),
		searchEdit (GetReference (), SearchEditId),
		list (GetReference (), ListId),
		chooseButton (GetReference (), ChooseButtonId),
		cancelButton (GetReference (), CancelButtonId),
		items (inItems),
		header1 (inHeader1),
		header2 (inHeader2),
		allowNone (inAllowNone)
{
	FillList ();

	chooseButton.Attach (*this);
	cancelButton.Attach (*this);
	list.Attach (*this);
	searchEdit.Attach (*this);
}


void GdlItemPickerDialog::FillList ()
{
	isFilling = true;

	const short columnCount = 2;
	list.SetHeaderItemCount (columnCount);
	list.SetTabFieldCount (columnCount);
	list.SetHeaderItemText (1, header1);
	list.SetHeaderItemText (2, header2);

	const short widths[2] = { 300, 240 };
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

	visibleItems.Clear ();

	// Première entrée : « (aucune) » si autorisée.
	if (allowNone) {
		list.AppendItem ();
		list.SetTabItemText (1, 1, FR ("—"));
		list.SetTabItemText (1, 2, FR ("(aucune)"));
	}

	const GS::UniString needle = searchEdit.GetText ().ToUpperCase ();
	for (UIndex i = 0; i < items.GetSize (); ++i) {
		if (!needle.IsEmpty ()
			&& !items[i].first.ToUpperCase ().Contains (needle)
			&& !items[i].second.ToUpperCase ().Contains (needle))
			continue;

		list.AppendItem ();
		const short item = list.GetItemCount ();
		list.SetTabItemText (item, 1, items[i].first);
		list.SetTabItemText (item, 2, items[i].second);
		visibleItems.Push (static_cast<short> (i + 1));
	}

	if (list.GetItemCount () > 0)
		list.SelectItem (1);

	isFilling = false;
}


void GdlItemPickerDialog::ChooseCurrent ()
{
	const short item = list.GetSelectedItem ();
	if (item < 1 || item > list.GetItemCount ())
		return;

	if (allowNone && item == 1) {
		selectedItemIndex = 0;		// « (aucune) »
	} else {
		const short visibleIndex = static_cast<short> (item - (allowNone ? 1 : 0));
		if (visibleIndex < 1 || static_cast<UIndex> (visibleIndex) > visibleItems.GetSize ())
			return;
		selectedItemIndex = visibleItems[static_cast<UIndex> (visibleIndex) - 1];
	}

	accepted = true;
	PostCloseRequest (DG::ModalDialog::Accept);
}


void GdlItemPickerDialog::ButtonClicked (const DG::ButtonClickEvent& ev)
{
	if (ev.GetSource () == &chooseButton) {
		ChooseCurrent ();
	} else if (ev.GetSource () == &cancelButton) {
		PostCloseRequest (DG::ModalDialog::Cancel);
	}
}


void GdlItemPickerDialog::ListBoxDoubleClicked (const DG::ListBoxDoubleClickEvent& ev)
{
	if (ev.GetSource () != &list)
		return;

	ChooseCurrent ();
}


void GdlItemPickerDialog::SearchTextChanged (const DG::SearchEditChangeEvent& ev)
{
	if (ev.GetSource () != &searchEdit)
		return;

	FillList ();
}

} // namespace CostWaves
