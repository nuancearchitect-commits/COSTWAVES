#include "CostWavesPrecompiledHeader.hpp"

#include "MaterialDialog.hpp"

#include "ArticleManager.hpp"

#include "UniStringWStringConversion.hpp"

#include <cwchar>

namespace CostWaves {

namespace {

// Littéral UTF-8 -> GS::UniString (les sources sont compilees avec /utf-8).
GS::UniString FR (const char* utf8Text)
{
	return GS::UniString (utf8Text, CC_UTF8);
}

GS::UniString US (const char* utf8Text)
{
	return GS::UniString (utf8Text, CC_UTF8);
}

// Remplit un popup avec tous les attributs du type donné (index — nom).
// preferredNamePart : nom (ou fragment) à présélectionner si présent.
void FillAttributePopup (DG::PopUp& popup, API_AttrTypeID typeID,
						 GS::Array<API_AttributeIndex>& outIndices,
						 const GS::UniString& preferredNamePart)
{
	while (popup.GetItemCount () > 0)
		popup.DeleteItem (1);
	outIndices.Clear ();

	GS::Array<API_Attribute> attributes;
	if (ACAPI_Attribute_GetAttributesByType (typeID, attributes) != NoError)
		return;

	short preferredItem = 0;
	const GS::UniString needle = preferredNamePart.ToUpperCase ();
	for (UIndex i = 0; i < attributes.GetSize (); ++i) {
		const API_AttributeIndex index = attributes[i].header.index;
		if (!index.IsPositive ())
			continue;

		const GS::UniString name (attributes[i].header.name, CC_UTF8);

		popup.AppendItem ();
		const short item = popup.GetItemCount ();
		popup.SetItemText (item, index.ToUniString () + FR (" — ") + name);
		outIndices.Push (index);

		if (!needle.IsEmpty () && name.ToUpperCase ().Contains (needle))
			preferredItem = item;
	}

	if (popup.GetItemCount () > 0)
		popup.SelectItem (preferredItem > 0 ? preferredItem : 1);
}

// Remplit un popup avec les stylos de la table active (1..255).
void FillPenPopup (DG::PopUp& popup, GS::Array<short>& outIndices, short defaultPen)
{
	while (popup.GetItemCount () > 0)
		popup.DeleteItem (1);
	outIndices.Clear ();

	UInt32 penCount = 0;
	if (ACAPI_Attribute_GetPenNum (penCount) != NoError)
		return;

	for (UInt32 i = 1; i <= penCount && i <= 255; ++i) {
		API_Pen pen;
		BNZeroMemory (&pen, sizeof (pen));
		pen.index = static_cast<short> (i);
		if (ACAPI_Attribute_GetPen (pen) != NoError)
			continue;

		GS::UniString label = GS::ToUniString (std::to_wstring (static_cast<int> (i))) + FR (" — ");
		const GS::UniString description (pen.description, CC_UTF8);
		label += description.IsEmpty () ? FR ("(sans nom)") : description;

		popup.AppendItem ();
		popup.SetItemText (popup.GetItemCount (), label);
		outIndices.Push (static_cast<short> (i));
	}

	if (popup.GetItemCount () > 0)
		popup.SelectItem (defaultPen >= 1 && defaultPen <= popup.GetItemCount () ? defaultPen : 1);
}

} // namespace


MaterialDialog::MaterialDialog (const GS::Array<CWSystemInfo>& inSystems)
	:	DG::ModalDialog (ACAPI_GetOwnResModule (), ID_ADDON_DLG_MATERIAL, ACAPI_GetOwnResModule ()),
		nameLabel (GetReference (), NameLabelId),
		nameEdit (GetReference (), NameEditId),
		newClassCheck (GetReference (), NewClassCheckId),
		systemLabel (GetReference (), SystemLabelId),
		systemPopup (GetReference (), SystemPopupId),
		parentLabel (GetReference (), ParentLabelId),
		parentPopup (GetReference (), ParentPopupId),
		classIdLabel (GetReference (), ClassIdLabelId),
		classIdEdit (GetReference (), ClassIdEditId),
		classNameLabel (GetReference (), ClassNameLabelId),
		classNameEdit (GetReference (), ClassNameEditId),
		existingLabel (GetReference (), ExistingLabelId),
		existingPopup (GetReference (), ExistingPopupId),
		fillLabel (GetReference (), FillLabelId),
		fillPopup (GetReference (), FillPopupId),
		surfaceLabel (GetReference (), SurfaceLabelId),
		surfacePopup (GetReference (), SurfacePopupId),
		penFgLabel (GetReference (), PenFgLabelId),
		penFgPopup (GetReference (), PenFgPopupId),
		penBgLabel (GetReference (), PenBgLabelId),
		penBgPopup (GetReference (), PenBgPopupId),
		priorityLabel (GetReference (), PriorityLabelId),
		priorityEdit (GetReference (), PriorityEditId),
		noteText (GetReference (), NoteTextId),
		createButton (GetReference (), CreateButtonId),
		cancelButton (GetReference (), CancelButtonId),
		systems (inSystems)
{
	noteText.SetText (FR ("La classe sert d'article dans le métré ; le matériau est lié à cette classe.\n")
					  + FR ("S'il existe déjà un matériau de ce nom, ses attributs (hachure, surface, stylos, puissance) sont mis à jour.\n")
					  + FR ("L'ID proposé est le premier disponible parmi les enfants de la classe parente."));

	FillSystemsPopup ();
	ReloadClassPopups ();

	FillAttributePopup (fillPopup, API_FilltypeID, fillIndices, FR ("plein"));
	FillAttributePopup (surfacePopup, API_MaterialID, surfaceIndices, GS::UniString ());
	FillPenPopup (penFgPopup, penIndices, 1);
	FillPenPopup (penBgPopup, penIndices, 1);

	priorityEdit.SetMin (1);
	priorityEdit.SetMax (1000);
	priorityEdit.SetValue (500);

	UpdateMode ();
	UpdateAutoClassId ();

	systemPopup.Attach (*this);
	parentPopup.Attach (*this);
	newClassCheck.Attach (*this);
	createButton.Attach (*this);
	cancelButton.Attach (*this);
}


void MaterialDialog::FillSystemsPopup ()
{
	isFilling = true;

	while (systemPopup.GetItemCount () > 0)
		systemPopup.DeleteItem (1);

	short defaultItem = 1;
	const GS::UniString costWavesName (ArticleManager::CostWavesSystemName (), CC_UTF8);
	for (UIndex i = 0; i < systems.GetSize (); ++i) {
		systemPopup.AppendItem ();
		systemPopup.SetItemText (systemPopup.GetItemCount (), systems[i].name);
		if (systems[i].name == costWavesName)
			defaultItem = static_cast<short> (i + 1);
	}

	if (systemPopup.GetItemCount () > 0)
		systemPopup.SelectItem (defaultItem);

	isFilling = false;
}


void MaterialDialog::ReloadClassPopups ()
{
	isFilling = true;

	while (parentPopup.GetItemCount () > 0)
		parentPopup.DeleteItem (1);
	while (existingPopup.GetItemCount () > 0)
		existingPopup.DeleteItem (1);
	items.Clear ();
	itemDepths.Clear ();

	API_Guid systemGuid = APINULLGuid;
	const short systemIndex = systemPopup.GetSelectedItem ();
	if (systemIndex >= 1 && static_cast<USize> (systemIndex) <= systems.GetSize ())
		systemGuid = systems[static_cast<USize> (systemIndex) - 1].guid;

	if (systemGuid != APINULLGuid)
		ArticleManager::CollectItems (systemGuid, items, itemDepths);

	parentPopup.AppendItem ();
	parentPopup.SetItemText (1, FR ("(racine — classe de premier niveau)"));

	for (UIndex i = 0; i < items.GetSize (); ++i) {
		GS::UniString indent;
		for (short d = 0; d < itemDepths[i]; ++d)
			indent += US ("    ");
		const GS::UniString label = indent + items[i].id + FR (" — ") + items[i].name;

		parentPopup.AppendItem ();
		parentPopup.SetItemText (parentPopup.GetItemCount (), label);

		existingPopup.AppendItem ();
		existingPopup.SetItemText (existingPopup.GetItemCount (), label);
	}

	if (parentPopup.GetItemCount () > 0)
		parentPopup.SelectItem (1);
	if (existingPopup.GetItemCount () > 0)
		existingPopup.SelectItem (1);

	// Mode par défaut : nouvelle classe si le système est vide, sinon
	// classe existante.
	newClassCheck.Uncheck ();
	if (existingPopup.GetItemCount () == 0)
		newClassCheck.Check ();

	isFilling = false;
}


void MaterialDialog::UpdateAutoClassId ()
{
	API_Guid systemGuid = APINULLGuid;
	const short systemIndex = systemPopup.GetSelectedItem ();
	if (systemIndex >= 1 && static_cast<USize> (systemIndex) <= systems.GetSize ())
		systemGuid = systems[static_cast<USize> (systemIndex) - 1].guid;
	if (systemGuid == APINULLGuid)
		return;

	API_Guid parentGuid = APINULLGuid;
	const short parentIndex = parentPopup.GetSelectedItem ();
	if (parentIndex >= 2 && static_cast<USize> (parentIndex - 2) < items.GetSize ())
		parentGuid = items[static_cast<USize> (parentIndex) - 2].guid;

	classIdEdit.SetText (ArticleManager::FirstAvailableChildId (systemGuid, parentGuid));
}


void MaterialDialog::UpdateMode ()
{
	if (newClassCheck.IsChecked ()) {
		parentPopup.Enable ();
		classIdEdit.Enable ();
		classNameEdit.Enable ();
		existingPopup.Disable ();
	} else {
		parentPopup.Disable ();
		classIdEdit.Disable ();
		classNameEdit.Disable ();
		existingPopup.Enable ();
	}
}


bool MaterialDialog::Apply (GS::UniString& outSummary, GS::UniString& outError)
{
	outSummary.Clear ();
	outError.Clear ();

	const GS::UniString materialName = nameEdit.GetText ();
	if (materialName.IsEmpty ()) {
		outError = FR ("Le nom du matériau est vide.");
		return false;
	}

	API_Guid systemGuid = APINULLGuid;
	const short systemIndex = systemPopup.GetSelectedItem ();
	if (systemIndex >= 1 && static_cast<USize> (systemIndex) <= systems.GetSize ())
		systemGuid = systems[static_cast<USize> (systemIndex) - 1].guid;
	if (systemGuid == APINULLGuid) {
		outError = FR ("Aucun système de classification sélectionné.");
		return false;
	}

	const bool createNewClass = newClassCheck.IsChecked ();
	API_Guid parentItemGuid = APINULLGuid;
	API_Guid existingItemGuid = APINULLGuid;
	GS::UniString classId;
	GS::UniString className;

	if (createNewClass) {
		const short parentIndex = parentPopup.GetSelectedItem ();
		if (parentIndex >= 2 && static_cast<USize> (parentIndex - 2) < items.GetSize ())
			parentItemGuid = items[static_cast<USize> (parentIndex) - 2].guid;

		classId = classIdEdit.GetText ();
		className = classNameEdit.GetText ();
		if (className.IsEmpty ())
			className = materialName;
		if (classId.IsEmpty ()) {
			outError = FR ("L'identifiant de la classe est vide.");
			return false;
		}
	} else {
		const short existingIndex = existingPopup.GetSelectedItem ();
		if (existingIndex < 1 || static_cast<USize> (existingIndex) > items.GetSize ()) {
			outError = FR ("Aucune classe existante sélectionnée.");
			return false;
		}
		existingItemGuid = items[static_cast<USize> (existingIndex) - 1].guid;
	}

	CWMaterialAttributes attributes;
	attributes.connPriority = priorityEdit.GetValue ();

	const short fillSelection = fillPopup.GetSelectedItem ();
	if (fillSelection >= 1 && static_cast<USize> (fillSelection) <= fillIndices.GetSize ())
		attributes.cutFill = fillIndices[static_cast<USize> (fillSelection) - 1];

	const short surfaceSelection = surfacePopup.GetSelectedItem ();
	if (surfaceSelection >= 1 && static_cast<USize> (surfaceSelection) <= surfaceIndices.GetSize ())
		attributes.cutMaterial = surfaceIndices[static_cast<USize> (surfaceSelection) - 1];

	const short penFgSelection = penFgPopup.GetSelectedItem ();
	if (penFgSelection >= 1 && static_cast<USize> (penFgSelection) <= penIndices.GetSize ())
		attributes.cutFillPen = penIndices[static_cast<USize> (penFgSelection) - 1];

	const short penBgSelection = penBgPopup.GetSelectedItem ();
	if (penBgSelection >= 1 && static_cast<USize> (penBgSelection) <= penIndices.GetSize ())
		attributes.cutFillBackgroundPen = penIndices[static_cast<USize> (penBgSelection) - 1];

	API_Guid itemGuid = APINULLGuid;
	bool materialCreated = false;
	bool classCreated = false;
	const GSErrCode err = ArticleManager::CreateMaterialWithClass (materialName, attributes,
																   createNewClass, systemGuid,
																   parentItemGuid, classId, className,
																   existingItemGuid, itemGuid,
																   materialCreated, classCreated, outError);
	if (err != NoError) {
		if (outError.IsEmpty ())
			outError = FR ("Création du matériau impossible.");
		return false;
	}

	outSummary = FR ("Matériau « ") + materialName + FR (" » ")
		+ (materialCreated ? FR ("créé") : FR ("mis à jour"))
		+ FR (" · classe ") + (classCreated ? (classId + FR (" créée")) : FR ("affectée"))
		+ FR (" · puissance ")
		+ GS::ToUniString (std::to_wstring (attributes.connPriority))
		+ FR (".\nNB : la création d'attributs n'est pas annulable (limite API).");
	return true;
}


void MaterialDialog::ButtonClicked (const DG::ButtonClickEvent& ev)
{
	if (ev.GetSource () == &createButton) {
		if (nameEdit.GetText ().IsEmpty ()) {
			DG::WarningAlert (FR ("Le nom du matériau est vide."),
							  FR ("Saisissez un nom pour le matériau."), FR ("OK"));
			return;
		}
		if (newClassCheck.IsChecked () && classIdEdit.GetText ().IsEmpty ()) {
			DG::WarningAlert (FR ("L'identifiant de la classe est vide."),
							  FR ("Laissez l'ID proposé (premier ID disponible) ou saisissez-en un."), FR ("OK"));
			return;
		}
		accepted = true;
		PostCloseRequest (DG::ModalDialog::Accept);
	} else if (ev.GetSource () == &cancelButton) {
		PostCloseRequest (DG::ModalDialog::Cancel);
	}
}


void MaterialDialog::PopUpChanged (const DG::PopUpChangeEvent& ev)
{
	if (isFilling)
		return;

	if (ev.GetSource () == &systemPopup) {
		ReloadClassPopups ();
		UpdateMode ();
		UpdateAutoClassId ();
	} else if (ev.GetSource () == &parentPopup) {
		UpdateAutoClassId ();
	}
}


void MaterialDialog::CheckItemChanged (const DG::CheckItemChangeEvent& ev)
{
	if (isFilling)
		return;

	if (ev.GetSource () == &newClassCheck)
		UpdateMode ();
}

} // namespace CostWaves
