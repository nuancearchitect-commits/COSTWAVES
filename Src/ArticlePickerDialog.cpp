#include "CostWavesPrecompiledHeader.hpp"

#include "ArticlePickerDialog.hpp"

#include "CostWavesStyle.hpp"
#include "UniStringWStringConversion.hpp"

namespace CostWaves {

namespace {

GS::UniString FR (const char* utf8Text)
{
	return GS::UniString (utf8Text, CC_UTF8);
}

// Indentation hiérarchique (style classification Archicad).
GS::UniString Indent (short depth)
{
	GS::UniString indent;
	for (short d = 0; d < depth; ++d)
		indent += FR ("    ");
	return indent;
}

} // namespace


ArticlePickerDialog::ArticlePickerDialog (const GS::Array<CWArticle>& inArticles)
	:	DG::ModalDialog (ACAPI_GetOwnResModule (), ID_ADDON_DLG_PICKER, ACAPI_GetOwnResModule ()),
		searchEdit (GetReference (), SearchEditId),
		list (GetReference (), ListId),
		chooseButton (GetReference (), ChooseButtonId),
		cancelButton (GetReference (), CancelButtonId),
		articles (inArticles)
{
	FillList (GS::UniString ());

	chooseButton.Attach (*this);
	cancelButton.Attach (*this);
	list.Attach (*this);
	searchEdit.Attach (*this);
}


void ArticlePickerDialog::FillList (const GS::UniString& filter)
{
	isFilling = true;

	const short columnCount = 2;
	list.SetHeaderItemCount (columnCount);
	list.SetTabFieldCount (columnCount);
	list.SetHeaderItemText (1, FR ("ID"));
	list.SetHeaderItemText (2, FR ("Nom"));

	const short widths[2] = { 160, 320 };
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

	visibleArticles.Clear ();

	// Première entrée : « (aucune) » = pas de correspondance (matériau ignoré).
	list.AppendItem ();
	list.SetTabItemText (1, 1, FR ("—"));
	list.SetTabItemText (1, 2, FR ("(aucune)"));
	CostWavesStyle::CellMuted (list, 1, 1);
	CostWavesStyle::CellMuted (list, 1, 2);

	const GS::UniString needle = filter.ToUpperCase ();
	for (UIndex a = 0; a < articles.GetSize (); ++a) {
		const GS::UniString label = articles[a].id + FR (" — ") + articles[a].name;
		if (!needle.IsEmpty ()
			&& !articles[a].id.ToUpperCase ().Contains (needle)
			&& !articles[a].name.ToUpperCase ().Contains (needle))
			continue;

		list.AppendItem ();
		const short item = list.GetItemCount ();
		list.SetTabItemText (item, 1, Indent (articles[a].depth) + articles[a].id);
		list.SetTabItemText (item, 2, articles[a].name);
		visibleArticles.Push (static_cast<short> (a + 1));
	}

	if (list.GetItemCount () > 0)
		list.SelectItem (1);

	isFilling = false;
}


void ArticlePickerDialog::ChooseCurrent ()
{
	const short item = list.GetSelectedItem ();
	if (item < 1 || item > list.GetItemCount ())
		return;

	if (item == 1) {
		selectedArticleIndex = 0;		// « (aucune) »
	} else {
		const short visibleIndex = static_cast<short> (item - 1);
		if (visibleIndex < 1 || static_cast<UIndex> (visibleIndex) > visibleArticles.GetSize ())
			return;
		selectedArticleIndex = visibleArticles[static_cast<UIndex> (visibleIndex) - 1];
	}

	accepted = true;
	PostCloseRequest (DG::ModalDialog::Accept);
}


void ArticlePickerDialog::ButtonClicked (const DG::ButtonClickEvent& ev)
{
	if (ev.GetSource () == &chooseButton) {
		ChooseCurrent ();
	} else if (ev.GetSource () == &cancelButton) {
		PostCloseRequest (DG::ModalDialog::Cancel);
	}
}


void ArticlePickerDialog::ListBoxDoubleClicked (const DG::ListBoxDoubleClickEvent& ev)
{
	if (ev.GetSource () != &list)
		return;

	ChooseCurrent ();
}


void ArticlePickerDialog::SearchTextChanged (const DG::SearchEditChangeEvent& ev)
{
	if (ev.GetSource () != &searchEdit)
		return;

	FillList (searchEdit.GetText ());
}

} // namespace CostWaves
