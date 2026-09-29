#include "CostWavesPrecompiledHeader.hpp"

#include "KeyPickerDialog.hpp"

namespace CostWaves {

namespace {

GS::UniString FR (const char* utf8Text)
{
	return GS::UniString (utf8Text, CC_UTF8);
}

} // namespace


KeyPickerDialog::KeyPickerDialog (const GS::Array<CWKeyEntry>& inKeys, const GS::UniString& inCurrentKeyId)
	:	DG::ModalDialog (ACAPI_GetOwnResModule (), ID_ADDON_DLG_KEYPICKER, ACAPI_GetOwnResModule ()),
		searchEdit (GetReference (), SearchEditId),
		list (GetReference (), ListId),
		chooseButton (GetReference (), ChooseButtonId),
		cancelButton (GetReference (), CancelButtonId),
		keys (inKeys),
		currentKeyId (inCurrentKeyId)
{
	FillList (GS::UniString ());

	chooseButton.Attach (*this);
	cancelButton.Attach (*this);
	list.Attach (*this);
	searchEdit.Attach (*this);
}


void KeyPickerDialog::FillList (const GS::UniString& filter)
{
	isFilling = true;

	const short columnCount = 2;
	list.SetHeaderItemCount (columnCount);
	list.SetTabFieldCount (columnCount);
	list.SetHeaderItemText (1, FR ("Groupe"));
	list.SetHeaderItemText (2, FR ("Valeur clé"));

	const short widths[2] = { 170, 370 };
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

	visibleKeys.Clear ();

	// Première entrée : « (aucune) » = pas de valeur clé (l'article ne
	// diffère pas par un paramètre).
	list.AppendItem ();
	list.SetTabItemText (1, 1, FR ("—"));
	list.SetTabItemText (1, 2, FR ("(aucune)"));

	const GS::UniString needle = filter.ToUpperCase ();
	for (UIndex k = 0; k < keys.GetSize (); ++k) {
		if (!needle.IsEmpty ()
			&& !keys[k].group.ToUpperCase ().Contains (needle)
			&& !keys[k].name.ToUpperCase ().Contains (needle))
			continue;

		list.AppendItem ();
		const short item = list.GetItemCount ();
		list.SetTabItemText (item, 1, keys[k].group);
		list.SetTabItemText (item, 2, keys[k].name);
		visibleKeys.Push (static_cast<short> (k + 1));
	}

	// Présélection : la clé déjà choisie si elle reste visible, sinon « (aucune) ».
	short selection = 1;
	for (short item = 2; item <= list.GetItemCount (); ++item) {
		const short visibleIndex = static_cast<short> (item - 1);
		if (visibleIndex >= 1 && static_cast<UIndex> (visibleIndex) <= visibleKeys.GetSize ()
			&& keys[static_cast<UIndex> (visibleKeys[static_cast<UIndex> (visibleIndex) - 1]) - 1].id == currentKeyId) {
			selection = item;
			break;
		}
	}
	list.SelectItem (selection);

	isFilling = false;
}


void KeyPickerDialog::ChooseCurrent ()
{
	const short item = list.GetSelectedItem ();
	if (item < 1 || item > list.GetItemCount ())
		return;

	if (item == 1) {
		selectedKeyIndex = 0;		// « (aucune) »
	} else {
		const short visibleIndex = static_cast<short> (item - 1);
		if (visibleIndex < 1 || static_cast<UIndex> (visibleIndex) > visibleKeys.GetSize ())
			return;
		selectedKeyIndex = visibleKeys[static_cast<UIndex> (visibleIndex) - 1];
	}

	accepted = true;
	PostCloseRequest (DG::ModalDialog::Accept);
}


void KeyPickerDialog::ButtonClicked (const DG::ButtonClickEvent& ev)
{
	if (ev.GetSource () == &chooseButton) {
		ChooseCurrent ();
	} else if (ev.GetSource () == &cancelButton) {
		PostCloseRequest (DG::ModalDialog::Cancel);
	}
}


void KeyPickerDialog::ListBoxDoubleClicked (const DG::ListBoxDoubleClickEvent& ev)
{
	if (ev.GetSource () != &list)
		return;

	ChooseCurrent ();
}


void KeyPickerDialog::SearchTextChanged (const DG::SearchEditChangeEvent& ev)
{
	if (ev.GetSource () != &searchEdit)
		return;

	FillList (searchEdit.GetText ());
}

} // namespace CostWaves
