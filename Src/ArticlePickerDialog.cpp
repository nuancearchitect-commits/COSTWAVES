#include "CostWavesPrecompiledHeader.hpp"

#include "ArticlePickerDialog.hpp"

#include "UniStringWStringConversion.hpp"

#include <cwchar>

namespace CostWaves {

namespace {

// Littéral UTF-8 -> GS::UniString (les sources sont compilees avec /utf-8).
GS::UniString FR (const char* utf8Text)
{
	return GS::UniString (utf8Text, CC_UTF8);
}

// GUID de disposition de la fenêtre (fixe, pour la mémorisation écran).
const char* kArticlePickerGuidString = "4B9D1E62-C7A4-4F58-8D21-9E6C3B0A7F14";

} // namespace


ArticlePickerDialog::ArticlePickerDialog (const GS::Array<CWArticle>& inArticles,
										  USize selectedElementCount, bool numbered)
	:	DG::ModalDialog (DG::NativePoint (DG::NativeUnit (120), DG::NativeUnit (120)), 440, 180,
							APIGuid2GSGuid (APIGuidFromString (kArticlePickerGuidString)),
							DG::ModalDialog::NoGrow, DG::ModalDialog::TopCaption,
							DG::ModalDialog::NormalFrame),
		infoText (GetReference (), DG::Rect (10, 12, 430, 30)),
		articleLabel (GetReference (), DG::Rect (10, 46, 430, 64)),
		articlePopup (GetReference (), DG::Rect (10, 68, 430, 90), 80, 3),
		createButton (GetReference (), DG::Rect (200, 140, 320, 163)),
		cancelButton (GetReference (), DG::Rect (330, 140, 430, 163)),
		articles (inArticles)
{
	SetTitle (numbered ? FR ("CostWaves — Créer un groupe")
					   : FR ("CostWaves — Créer un ensemble"));

	infoText.SetText (GS::ToUniString (std::to_wstring (static_cast<int> (selectedElementCount)))
					  + FR (" élément(s) sélectionné(s) dans le plan.")
					  + FR (" Ils seront facturés ensemble via l'article choisi."));
	articleLabel.SetText (numbered ? FR ("Article (classe) du groupe — la quantité facturée sera le nombre de groupes :")
								   : FR ("Article (classe) de l'ensemble :"));

	for (UIndex i = 0; i < articles.GetSize (); ++i) {
		GS::UniString label = articles[i].id + FR (" — ") + articles[i].name;
		if (!articles[i].unit.IsEmpty ())
			label += FR (" (") + articles[i].unit + FR (")");
		articlePopup.AppendItem ();
		articlePopup.SetItemText (articlePopup.GetItemCount (), label);
	}
	if (articlePopup.GetItemCount () > 0)
		articlePopup.SelectItem (1);

	createButton.SetText (numbered ? FR ("Créer le groupe") : FR ("Créer l'ensemble"));
	cancelButton.SetText (FR ("Annuler"));

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
