#ifndef COSTWAVES_GDL_ITEM_PICKER_DIALOG_HPP
#define COSTWAVES_GDL_ITEM_PICKER_DIALOG_HPP

#include "ACAPinc.h"
#include "DGModule.hpp"

#include "ResourceIds.hpp"

namespace CostWaves {

// Sélecteur générique « recherche + liste » utilisé par la correspondance
// des objets GDL : choix d'un objet de bibliothèque (une colonne) ou d'un
// paramètre GDL (libellé + nom GDL). La ressource du dialogue est passée
// au constructeur (le titre est défini dans le GRC). Si allowNone est
// vrai, la première entrée « — (aucune) » est proposée.
// Double-clic ou « Choisir » valide.
class GdlItemPickerDialog final :	public DG::ModalDialog,
									public DG::ButtonItemObserver,
									public DG::ListBoxObserver,
									public DG::SearchEditObserver
{
public:
	enum ItemIds {
		SearchEditId	= 1,
		ListId			= 2,
		ChooseButtonId	= 3,
		CancelButtonId	= 4
	};

	// inItems : paires (colonne 1 = libellé, colonne 2 = complément).
	GdlItemPickerDialog (short inDialogResourceId,
						 const GS::UniString& inHeader1, const GS::UniString& inHeader2,
						 const GS::Array<GS::Pair<GS::UniString, GS::UniString>>& inItems,
						 bool inAllowNone);

	bool	IsAccepted () const { return accepted; }
	// 0 = « (aucune) » (si autorisée) ; sinon index 1-based dans items.
	short	GetSelectedItemIndex () const { return selectedItemIndex; }

private:
	// DG::ButtonItemObserver
	virtual void	ButtonClicked (const DG::ButtonClickEvent& ev) override;

	// DG::ListBoxObserver
	virtual void	ListBoxDoubleClicked (const DG::ListBoxDoubleClickEvent& ev) override;

	// DG::SearchEditObserver
	virtual void	SearchTextChanged (const DG::SearchEditChangeEvent& ev) override;

	void	FillList ();
	void	ChooseCurrent ();

	DG::SearchEdit		searchEdit;
	DG::MultiSelListBox	list;
	DG::Button			chooseButton;
	DG::Button			cancelButton;

	GS::Array<GS::Pair<GS::UniString, GS::UniString>>	items;
	GS::Array<short>									visibleItems;	// index 1-based affichés
	GS::UniString	header1;
	GS::UniString	header2;
	bool			allowNone = false;

	short	selectedItemIndex = 0;
	bool	accepted = false;
	bool	isFilling = false;
};

} // namespace CostWaves

#endif // COSTWAVES_GDL_ITEM_PICKER_DIALOG_HPP
