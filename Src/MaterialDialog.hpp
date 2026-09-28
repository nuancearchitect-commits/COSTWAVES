#ifndef COSTWAVES_MATERIAL_DIALOG_HPP
#define COSTWAVES_MATERIAL_DIALOG_HPP

#include "ACAPinc.h"
#include "DGModule.hpp"

#include "ResourceIds.hpp"
#include "DataTypes.hpp"

namespace CostWaves {

// Fenêtre « Créer le matériau… » :
//  - nom du matériau de construction ;
//  - classe de classification : soit une nouvelle classe (système + parent +
//    ID pré-rempli au premier disponible + nom), soit une classe existante ;
//  - attributs du matériau : hachure (remplissage en coupe), surface de
//    coupe, stylos avant/arrière-plan, puissance (priorité de connexion).
// À la validation : le matériau est créé (ou mis à jour s'il existe déjà) et
// lié à la classe, qui sert d'article dans le métré.
// Fenêtre modale définie en ressource GRC (ID_ADDON_DLG_MATERIAL).
class MaterialDialog final :	public DG::ModalDialog,
								public DG::ButtonItemObserver,
								public DG::PopUpObserver,
								public DG::CheckItemObserver
{
public:
	enum ItemIds {
		NameLabelId			= 1,
		NameEditId			= 2,
		NewClassCheckId		= 3,
		SystemLabelId		= 4,
		SystemPopupId		= 5,
		ParentLabelId		= 6,
		ParentPopupId		= 7,
		ClassIdLabelId		= 8,
		ClassIdEditId		= 9,
		ClassNameLabelId	= 10,
		ClassNameEditId		= 11,
		ExistingLabelId		= 12,
		ExistingPopupId		= 13,
		FillLabelId			= 14,
		FillPopupId			= 15,
		SurfaceLabelId		= 16,
		SurfacePopupId		= 17,
		PenFgLabelId		= 18,
		PenFgPopupId		= 19,
		PenBgLabelId		= 20,
		PenBgPopupId		= 21,
		PriorityLabelId		= 22,
		PriorityEditId		= 23,
		NoteTextId			= 24,
		CreateButtonId		= 25,
		CancelButtonId		= 26
	};

	MaterialDialog (const GS::Array<CWSystemInfo>& inSystems);

	bool	IsAccepted () const { return accepted; }

	// Applique la création (après Invoke, si accepté). Retourne false +
	// outError en cas d'échec ; outSummary contient le bilan sinon.
	bool	Apply (GS::UniString& outSummary, GS::UniString& outError);

private:
	// DG::ButtonItemObserver
	virtual void	ButtonClicked (const DG::ButtonClickEvent& ev) override;

	// DG::PopUpObserver
	virtual void	PopUpChanged (const DG::PopUpChangeEvent& ev) override;

	// DG::CheckItemObserver
	virtual void	CheckItemChanged (const DG::CheckItemChangeEvent& ev) override;

	void	FillSystemsPopup ();		// systèmes du projet (défaut : « CostWaves »)
	void	ReloadClassPopups ();		// items du système courant (parent + existant)
	void	UpdateAutoClassId ();		// premier ID libre sous le parent courant
	void	UpdateMode ();				// nouvelle classe <-> classe existante

	GS::Array<CWSystemInfo>				systems;
	GS::Array<API_ClassificationItem>	items;			// items du système courant
	GS::Array<short>					itemDepths;		// profondeur de chaque item (0 = racine)
	GS::Array<API_AttributeIndex>		fillIndices;	// index popup -> index d'attribut hachure
	GS::Array<API_AttributeIndex>		surfaceIndices;	// index popup -> index d'attribut surface
	GS::Array<short>					penIndices;		// index popup -> index de stylo

	DG::LeftText	nameLabel;
	DG::TextEdit	nameEdit;
	DG::CheckBox	newClassCheck;
	DG::LeftText	systemLabel;
	DG::PopUp		systemPopup;
	DG::LeftText	parentLabel;
	DG::PopUp		parentPopup;			// 1er item « (racine) »
	DG::LeftText	classIdLabel;
	DG::TextEdit	classIdEdit;
	DG::LeftText	classNameLabel;
	DG::TextEdit	classNameEdit;
	DG::LeftText	existingLabel;
	DG::PopUp		existingPopup;
	DG::LeftText	fillLabel;
	DG::PopUp		fillPopup;
	DG::LeftText	surfaceLabel;
	DG::PopUp		surfacePopup;
	DG::LeftText	penFgLabel;
	DG::PopUp		penFgPopup;
	DG::LeftText	penBgLabel;
	DG::PopUp		penBgPopup;
	DG::LeftText	priorityLabel;
	DG::IntEdit		priorityEdit;
	DG::LeftText	noteText;
	DG::Button		createButton;
	DG::Button		cancelButton;

	bool	accepted = false;
	bool	isFilling = false;
};

} // namespace CostWaves

#endif // COSTWAVES_MATERIAL_DIALOG_HPP
