#include "CostWavesPrecompiledHeader.hpp"

#include "ArticlePickerDialog.hpp"

#include "ResourceIds.hpp"

#include "UniStringWStringConversion.hpp"

#include <cwchar>

namespace CostWaves {

namespace {

// Littéral UTF-8 -> GS::UniString (les sources sont compilees avec /utf-8).
GS::UniString FR (const char* utf8Text)
{
	return GS::UniString (utf8Text, CC_UTF8);
}

// Indentation hiérarchique (une unité par niveau de classification).
GS::UniString Indent (short depth)
{
	GS::UniString indent;
	for (short d = 0; d < depth; ++d)
		indent += FR ("    ");
	return indent;
}

} // namespace


ArticlePickerDialog::ArticlePickerDialog (const GS::Array<CWArticle>& inArticles,
										  USize selectedElementCount, bool numbered)
	:	DG::ModalDialog (ACAPI_GetOwnResModule (), ID_ADDON_DLG_PICKER, ACAPI_GetOwnResModule ()),
		infoText (GetReference (), InfoTextId),
		articleLabel (GetReference (), ArticleLabelId),
		articlePopup (GetReference (), ArticlePopupId),
		createButton (GetReference (), CreateButtonId),
		cancelButton (GetReference (), CancelButtonId),
		articles (inArticles)
{
	SetTitle (numbered ? FR ("CostWaves — Créer un groupe")
					   : FR ("CostWaves — Créer un ensemble"));

	infoText.SetText (GS::ToUniString (std::to_wstring (static_cast<int> (selectedElementCount)))
					  + FR (" élément(s) sélectionné(s) dans le plan.")
					  + FR (" Ils seront facturés ensemble via l'article choisi."));
	articleLabel.SetText (numbered ? FR ("Article (classe) du groupe — la quantité facturée sera le nombre de groupes :")
								   : FR ("Article (classe) de l'ensemble :"));

	// Liste hiérarchique (indentée comme la classification).
	for (UIndex i = 0; i < articles.GetSize (); ++i) {
		GS::UniString label = Indent (articles[i].depth) + articles[i].id + FR (" — ") + articles[i].name;
		if (!articles[i].unit.IsEmpty ())
			label += FR (" (") + articles[i].unit + FR (")");
		articlePopup.AppendItem ();
		articlePopup.SetItemText (articlePopup.GetItemCount (), label);
	}
	if (articlePopup.GetItemCount () > 0)
		articlePopup.SelectItem (1);

	createButton.SetText (numbered ? FR ("Créer le groupe") : FR ("Créer l'ensemble"));

	createButton.Attach (*this);
	cancelButton.Attach (*this);
}


void ArticlePickerDialog::ButtonClicked (const DG::ButtonClickEvent& ev)
{
	if (ev.GetSource () == &createButton) {
		const short selection = articlePopup.GetSelectedItem ();
		if (selection >= 1 && static_cast<USize> (selection) <= articles.GetSize ()) {
			selectedArticle = articles[static_cast<USize> (selection) - 1];
			accepted = true;
			PostCloseRequest (DG::ModalDialog::Accept);
		}
	} else if (ev.GetSource () == &cancelButton) {
		PostCloseRequest (DG::ModalDialog::Cancel);
	}
}

} // namespace CostWaves
