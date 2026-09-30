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
										  const GS::Array<CWGdlParam>& inItems,
										  bool inAllowNone,
										  const GS::UniString& inHighlightType)
	:	DG::ModalDialog (ACAPI_GetOwnResModule (), inDialogResourceId, ACAPI_GetOwnResModule ()),
		searchEdit (GetReference (), SearchEditId),
		list (GetReference (), ListId),
		chooseButton (GetReference (), ChooseButtonId),
		cancelButton (GetReference (), CancelButtonId),
		items (inItems),
		header1 (inHeader1),
		header2 (inHeader2),
		highlightType (inHighlightType),
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

	// Colonne « Type » uniquement si les items en ont un (choix d'un
	// paramètre GDL) ; le choix d'un objet reste à 2 colonnes.
	bool hasTypes = false;
	for (UIndex i = 0; i < items.GetSize (); ++i) {
		if (!items[i].type.IsEmpty ()) {
			hasTypes = true;
			break;
		}
	}

	const short columnCount = hasTypes ? 3 : 2;
	list.SetHeaderItemCount (columnCount);
	list.SetTabFieldCount (columnCount);
	list.SetHeaderItemText (1, header1);
	list.SetHeaderItemText (2, header2);
	if (hasTypes)
		list.SetHeaderItemText (3, FR ("Type"));

	const short widths2[2] = { 300, 240 };
	const short widths3[3] = { 230, 150, 160 };
	short position = 0;
	for (short i = 1; i <= columnCount; ++i) {
		const short width = hasTypes ? widths3[i - 1] : widths2[i - 1];
		list.SetHeaderItemSize (i, width);
		list.SetHeaderItemSizeableFlag (i, true);
		list.SetTabFieldProperties (i, position, static_cast<short> (position + width),
									DG::ListBox::Left, DG::ListBox::MiddleTruncate, i > 1);
		position = static_cast<short> (position + width);
	}

	while (list.GetItemCount () > 0)
		list.DeleteItem (1);

	visibleItems.Clear ();

	// Première entrée : « (aucune) » si autorisée.
	if (allowNone) {
		list.AppendItem ();
		list.SetTabItemText (1, 1, FR ("—"));
		list.SetTabItemText (1, 2, FR ("(aucune)"));
		if (hasTypes)
			list.SetTabItemText (1, 3, GS::UniString ());
	}

	// Ordre : les paramètres du type demandé EN TÊTE (ordre GDL conservé),
	// puis les autres — pour vérifier d'un coup d'œil que rien ne manque.
	GS::Array<UIndex> order;
	if (!highlightType.IsEmpty ()) {
		for (UIndex i = 0; i < items.GetSize (); ++i) {
			if (items[i].type == highlightType)
				order.Push (i);
		}
	}
	for (UIndex i = 0; i < items.GetSize (); ++i) {
		if (highlightType.IsEmpty () || items[i].type != highlightType)
			order.Push (i);
	}

	const GS::UniString needle = searchEdit.GetText ().ToUpperCase ();
	for (UIndex o = 0; o < order.GetSize (); ++o) {
		const CWGdlParam& item = items[order[o]];
		if (!needle.IsEmpty ()
			&& !item.label.ToUpperCase ().Contains (needle)
			&& !item.name.ToUpperCase ().Contains (needle)
			&& !item.type.ToUpperCase ().Contains (needle))
			continue;

		list.AppendItem ();
		const short row = list.GetItemCount ();
		list.SetTabItemText (row, 1, item.label);
		list.SetTabItemText (row, 2, item.name);
		if (hasTypes) {
			list.SetTabItemText (row, 3, item.type);
			// Type demandé : en gras (double vérification visuelle).
			if (!highlightType.IsEmpty () && item.type == highlightType)
				list.SetTabItemFontStyle (row, 3, DG::Font::Bold);
		}
		visibleItems.Push (static_cast<short> (order[o] + 1));
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
