#ifndef COSTWAVES_GDL_ITEM_PICKER_DIALOG_HPP
#define COSTWAVES_GDL_ITEM_PICKER_DIALOG_HPP

#include "ACAPinc.h"
#include "DGModule.hpp"

#include "ResourceIds.hpp"
#include "DataTypes.hpp"

namespace CostWaves {

// Sélecteur générique « recherche + liste » utilisé par la correspondance
// des objets GDL et les articles hérités : choix d'un objet de
// bibliothèque (2 colonnes) ou d'un paramètre GDL (3 colonnes :
// Paramètre | Nom GDL | Type — le type permet la double vérification).
// Les paramètres du type demandé (inHighlightType, ex. « longueur » ou
// « bool ») sont listés EN TÊTE avec leur type en gras.
// Quand les items ont un type, une RANGÉE DE BOUTONS RADIO sous la
// recherche propose « Tous » + un bouton par type réellement présent,
// pour filtrer la liste par type. RIEN n'est filtré automatiquement :
// « Tous » est sélectionné à l'ouverture, le filtre ne s'applique que
// sur un clic explicite (et se combine avec la recherche).
// La ressource du dialogue est passée au constructeur (titre défini
// dans le GRC) ; si les items ont un type, elle doit déclarer les
// boutons radio 5..15 (cf. ID_ADDON_DLG_PARAMPICKER). Si allowNone est
// vrai, la première entrée « — (aucune) » est proposée (toujours
// visible, même filtrée). Double-clic ou « Choisir » valide.
class GdlItemPickerDialog final :	public DG::ModalDialog,
						public DG::ButtonItemObserver,
						public DG::ListBoxObserver,
						public DG::SearchEditObserver,
						public DG::RadioItemObserver
{
public:
	enum ItemIds {
		SearchEditId	= 1,
		ListId			= 2,
		ChooseButtonId	= 3,
		CancelButtonId	= 4,

		// Boutons radio de filtre par type (uniquement dans la ressource
		// du sélecteur de paramètres) : « Tous » + un emplacement par
		// type présent, les emplacements inutilisés sont masqués.
		TypeRadioFirstId	= 5,
		TypeRadioSlotCount	= 11
	};

	GdlItemPickerDialog (short inDialogResourceId,
						const GS::UniString& inHeader1, const GS::UniString& inHeader2,
						const GS::Array<CWGdlParam>& inItems,
						bool inAllowNone,
						const GS::UniString& inHighlightType = GS::UniString ());
	~GdlItemPickerDialog ();

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

	// DG::RadioItemObserver
	virtual void	RadioItemChanged (const DG::RadioItemChangeEvent& ev) override;

	void	BuildTypeFilterRow ();	// boutons radio de filtre (si types présents)
	void	FillList ();
	void	ChooseCurrent ();

	DG::SearchEdit		searchEdit;
	DG::MultiSelListBox	list;
	DG::Button			chooseButton;
	DG::Button			cancelButton;
	DG::RadioButton*	typeRadios[TypeRadioSlotCount] = {};	// créés si types présents

	GS::Array<CWGdlParam>	items;
	GS::Array<short>		visibleItems;	// index 1-based affichés
	GS::Array<GS::UniString>	radioTypes;	// type ciblé par bouton radio ("" = tous)
	GS::UniString			header1;
	GS::UniString			header2;
	GS::UniString			highlightType;	// type demandé (en tête, en gras)
	GS::UniString			typeFilter;		// filtre actif ("" = aucun)
	bool					allowNone = false;

	short	selectedItemIndex = 0;
	bool	accepted = false;
	bool	isFilling = false;
};

} // namespace CostWaves

#endif // COSTWAVES_GDL_ITEM_PICKER_DIALOG_HPP
