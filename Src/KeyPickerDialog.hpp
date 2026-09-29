#ifndef COSTWAVES_KEY_PICKER_DIALOG_HPP
#define COSTWAVES_KEY_PICKER_DIALOG_HPP

#include "ACAPinc.h"
#include "DGModule.hpp"

#include "ResourceIds.hpp"
#include "DataTypes.hpp"

namespace CostWaves {

// Sélecteur de « valeur clé » (paramètre différenciant les articles d'une
// même classe, ex. épaisseur). Pour garder la liste courte :
//  - un FILTRE PAR GROUPE (popup) : clés calculées COSTWAVES (Élément,
//    Couche, Composite/profil) puis groupes de propriétés Archicad ;
//  - une barre de recherche ;
//  - le catalogue ne propose que des paramètres de DIMENSION/POSITION
//    (épaisseur, hauteur, profondeur, largeur, longueur, position).
// La première entrée « — (aucune) » retire la clé. Double-clic ou
// « Choisir » valide.
class KeyPickerDialog final :	public DG::ModalDialog,
								public DG::ButtonItemObserver,
								public DG::ListBoxObserver,
								public DG::SearchEditObserver,
								public DG::PopUpObserver
{
public:
	enum ItemIds {
		SearchEditId	= 1,
		GroupLabelId	= 2,
		GroupPopupId	= 3,
		ListId			= 4,
		ChooseButtonId	= 5,
		CancelButtonId	= 6
	};

	KeyPickerDialog (const GS::Array<CWKeyEntry>& inKeys, const GS::UniString& inCurrentKeyId);

	bool		IsAccepted () const { return accepted; }
	// 0 = « (aucune) » ; sinon index 1-based dans keys.
	short		GetSelectedKeyIndex () const { return selectedKeyIndex; }

private:
	// DG::ButtonItemObserver
	virtual void	ButtonClicked (const DG::ButtonClickEvent& ev) override;

	// DG::ListBoxObserver
	virtual void	ListBoxDoubleClicked (const DG::ListBoxDoubleClickEvent& ev) override;

	// DG::SearchEditObserver
	virtual void	SearchTextChanged (const DG::SearchEditChangeEvent& ev) override;

	// DG::PopUpObserver
	virtual void	PopUpChanged (const DG::PopUpChangeEvent& ev) override;

	void	FillList ();
	void	ChooseCurrent ();

	DG::SearchEdit		searchEdit;
	DG::LeftText		groupLabel;
	DG::PopUp			groupPopup;
	DG::MultiSelListBox	list;
	DG::Button			chooseButton;
	DG::Button			cancelButton;

	GS::Array<CWKeyEntry>		keys;
	GS::Array<GS::UniString>	groups;		// groupes distincts du catalogue
	GS::Array<short>			visibleKeys;	// index 1-based affichés (hors « (aucune) »)
	GS::UniString				currentKeyId;	// clé déjà choisie (présélection)

	short	selectedKeyIndex = 0;
	bool	accepted = false;
	bool	isFilling = false;
};

} // namespace CostWaves

#endif // COSTWAVES_KEY_PICKER_DIALOG_HPP
