#ifndef COSTWAVES_KEY_PICKER_DIALOG_HPP
#define COSTWAVES_KEY_PICKER_DIALOG_HPP

#include "ACAPinc.h"
#include "DGModule.hpp"

#include "ResourceIds.hpp"
#include "DataTypes.hpp"

namespace CostWaves {

// Sélecteur de « valeur clé » (paramètre différenciant les articles d'une
// même classe, ex. épaisseur) : liste « Groupe | Valeur clé » avec barre de
// recherche — clés calculées par COSTWAVES (géométrie) puis propriétés
// Archicad du projet. La première entrée « — (aucune) » retire la clé.
// Double-clic ou « Choisir » valide.
class KeyPickerDialog final :	public DG::ModalDialog,
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

	KeyPickerDialog (const GS::Array<CWKeyEntry>& inKeys, const GS::UniString& inCurrentKeyId);

	bool	IsAccepted () const { return accepted; }
	// 0 = « (aucune) » ; sinon index 1-based dans keys.
	short	GetSelectedKeyIndex () const { return selectedKeyIndex; }

private:
	// DG::ButtonItemObserver
	virtual void	ButtonClicked (const DG::ButtonClickEvent& ev) override;

	// DG::ListBoxObserver
	virtual void	ListBoxDoubleClicked (const DG::ListBoxDoubleClickEvent& ev) override;

	// DG::SearchEditObserver
	virtual void	SearchTextChanged (const DG::SearchEditChangeEvent& ev) override;

	void	FillList (const GS::UniString& filter);
	void	ChooseCurrent ();

	DG::SearchEdit		searchEdit;
	DG::MultiSelListBox	list;
	DG::Button			chooseButton;
	DG::Button			cancelButton;

	GS::Array<CWKeyEntry>	keys;
	GS::Array<short>		visibleKeys;	// index 1-based affichés (hors « (aucune) »)
	GS::UniString			currentKeyId;	// clé déjà choisie (présélection)

	short	selectedKeyIndex = 0;
	bool	accepted = false;
	bool	isFilling = false;
};

} // namespace CostWaves

#endif // COSTWAVES_KEY_PICKER_DIALOG_HPP
