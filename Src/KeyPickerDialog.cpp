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
		groupLabel (GetReference (), GroupLabelId),
		groupPopup (GetReference (), GroupPopupId),
		list (GetReference (), ListId),
		chooseButton (GetReference (), ChooseButtonId),
		cancelButton (GetReference (), CancelButtonId),
		keys (inKeys),
		currentKeyId (inCurrentKeyId)
{
	// Groupes distincts du catalogue, sans doublon, ordre conservé
	// (clés calculées COSTWAVES d'abord, puis propriétés Archicad).
	for (UIndex k = 0; k < keys.GetSize (); ++k) {
		if (!keys[k].group.IsEmpty () && !groups.Contains (keys[k].group))
			groups.Push (keys[k].group);
	}

	// Filtre par groupe : « (tous les groupes) » + un item par groupe.
	groupPopup.AppendItem ();
	groupPopup.SetItemText (groupPopup.GetItemCount (), FR ("(tous les groupes)"));
	for (UIndex g = 0; g < groups.GetSize (); ++g) {
		groupPopup.AppendItem ();
		groupPopup.SetItemText (groupPopup.GetItemCount (), groups[g]);
	}

	// Par défaut : le groupe de la clé déjà choisie (édition), sinon tous.
	short groupSelection = 1;
	if (!currentKeyId.IsEmpty ()) {
		for (UIndex k = 0; k < keys.GetSize (); ++k) {
			if (keys[k].id != currentKeyId)
				continue;
			for (UIndex g = 0; g < groups.GetSize (); ++g) {
				if (groups[g] == keys[k].group) {
					// +1 : item 1 = « (tous les groupes) », items 2.. = groupes.
					groupSelection = static_cast<short> (g + 2);
					break;
				}
			}
			break;
		}
	}
	groupPopup.SelectItem (groupSelection);

	FillList ();

	chooseButton.Attach (*this);
	cancelButton.Attach (*this);
	list.Attach (*this);
	searchEdit.Attach (*this);
	groupPopup.Attach (*this);
}


void KeyPickerDialog::FillList ()
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

	// Filtres combinés : GROUPE sélectionné dans le popup + TEXTE recherché.
	const short groupSelection = groupPopup.GetSelectedItem ();
	bool hasGroupFilter = false;
	GS::UniString groupFilter;
	if (groupSelection >= 2 && static_cast<UIndex> (groupSelection - 1) <= groups.GetSize ()) {
		groupFilter = groups[static_cast<UIndex> (groupSelection) - 2];
		hasGroupFilter = true;
	}

	const GS::UniString needle = searchEdit.GetText ().ToUpperCase ();
	for (UIndex k = 0; k < keys.GetSize (); ++k) {
		if (hasGroupFilter && keys[k].group != groupFilter)
			continue;
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

	FillList ();
}


void KeyPickerDialog::PopUpChanged (const DG::PopUpChangeEvent& ev)
{
	if (ev.GetSource () != &groupPopup || isFilling)
		return;

	FillList ();
}

} // namespace CostWaves
