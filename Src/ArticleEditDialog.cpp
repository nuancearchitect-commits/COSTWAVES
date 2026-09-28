#include "CostWavesPrecompiledHeader.hpp"

#include "ArticleEditDialog.hpp"

namespace CostWaves {

namespace {

GS::UniString FR (const char* utf8Text)
{
	return GS::UniString (utf8Text, CC_UTF8);
}

} // namespace


ArticleEditDialog::ArticleEditDialog ()
	:	DG::ModalDialog (ACAPI_GetOwnResModule (), ID_ADDON_DLG_ARTICLE, ACAPI_GetOwnResModule ()),
		chapterLabel (GetReference (), ChapterLabelId),
		chapterEdit (GetReference (), ChapterEditId),
		idLabel (GetReference (), IdLabelId),
		idEdit (GetReference (), IdEditId),
		nameLabel (GetReference (), NameLabelId),
		nameEdit (GetReference (), NameEditId),
		unitLabel (GetReference (), UnitLabelId),
		unitPopup (GetReference (), UnitPopupId),
		modeLabel (GetReference (), ModeLabelId),
		modePopup (GetReference (), ModePopupId),
		createButton (GetReference (), CreateButtonId),
		cancelButton (GetReference (), CancelButtonId)
{
	// Unités usuelles du métré.
	const char* units[7] = { "m", "m²", "m³", "ml", "U", "ENS", "kg" };
	for (short u = 0; u < 7; ++u) {
		unitPopup.AppendItem ();
		unitPopup.SetItemText (static_cast<short> (u + 1), GS::UniString (units[u], CC_UTF8));
	}
	unitPopup.SelectItem (2);	// m²

	// Mode de métré (spec §11) : élément ou composants, jamais les deux.
	modePopup.AppendItem ();
	modePopup.SetItemText (1, FR ("Élément (le parent → 1 article)"));
	modePopup.AppendItem ();
	modePopup.SetItemText (2, FR ("Composants (chaque couche → son article)"));
	modePopup.SelectItem (1);

	// Id unique par défaut (evite les collisions avec la base CostWaves) :
	// LOCAL-<8 premiers hex du GUID genere>.
	{
		GS::Guid guid;
		guid.Generate ();
		char buffer[64] = { 0 };
		guid.ConvertToString (buffer);
		char hex[9] = { 0 };
		size_t j = 0;
		for (size_t i = 0; buffer[i] != '\0' && j < 8; ++i) {
			const char c = buffer[i];
			if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F'))
				hex[j++] = c;
		}
		idEdit.SetText (FR ("LOCAL-") + GS::UniString (hex, CC_UTF8));
	}

	createButton.Attach (*this);
	cancelButton.Attach (*this);
}


void ArticleEditDialog::ButtonClicked (const DG::ButtonClickEvent& ev)
{
	if (ev.GetSource () == &createButton) {
		const GS::UniString id = idEdit.GetText ();
		const GS::UniString name = nameEdit.GetText ();
		if (id.IsEmpty () || name.IsEmpty ()) {
			DG::WarningAlert (FR ("Créer un article"),
							  FR ("Renseignez au minimum l'ID et la désignation de l'article."), FR ("OK"));
			return;
		}

		article.id = id;
		article.name = name;
		article.chapter = chapterEdit.GetText ();
		const short unitSelection = unitPopup.GetSelectedItem ();
		article.unit = (unitSelection >= 1 && unitSelection <= unitPopup.GetItemCount ())
			? unitPopup.GetItemText (unitSelection) : GS::UniString ();
		article.calcQuantity = GS::UniString ();
		article.calcFormula = GS::UniString ();

		accepted = true;
		PostCloseRequest (DG::ModalDialog::Accept);
	} else if (ev.GetSource () == &cancelButton) {
		PostCloseRequest (DG::ModalDialog::Cancel);
	}
}

} // namespace CostWaves
