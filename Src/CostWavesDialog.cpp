#include "CostWavesPrecompiledHeader.hpp"

#include "CostWavesDialog.hpp"

#include "Exporter.hpp"
#include "ModelReader.hpp"

#include "UniStringWStringConversion.hpp"

#include <cwchar>

namespace CostWaves {

namespace {

GS::UniString FR (const char* utf8Text)
{
	return GS::UniString (utf8Text, CC_UTF8);
}

GS::UniString US (const char* utf8Text)
{
	return GS::UniString (utf8Text, CC_UTF8);
}

// "1234,56" — affichage français.
GS::UniString FormatValue (double value)
{
	wchar_t buffer[64];
	swprintf (buffer, 64, L"%.2f", value);

	std::wstring text (buffer);
	const std::size_t dot = text.find (L'.');
	if (dot != std::wstring::npos)
		text[dot] = L',';

	return GS::ToUniString (text);
}

// Résumé compact des quantités (colonne "Quantités").
GS::UniString QuantitiesSummary (const GS::Array<CWQuantity>& quantities, USize maxItems)
{
	if (quantities.IsEmpty ())
		return FR ("—");

	GS::UniString summary;
	USize count = 0;
	for (UIndex i = 0; i < quantities.GetSize () && count < maxItems; ++i) {
		if (count > 0)
			summary += US (" · ");
		summary += quantities[i].label + " " + FormatValue (quantities[i].value) + " " + quantities[i].unit;
		++count;
	}
	if (quantities.GetSize () > maxItems)
		summary += FR (" · …");
	return summary;
}

// La ligne peut-elle encore recevoir ce fragment (~110 caractères) ?
bool LineFits (const GS::UniString& line, const GS::UniString& chunk)
{
	return line.GetLength () + chunk.GetLength () < 110u;
}

} // namespace


CostWavesDialog::CostWavesDialog ()
	:	DG::ModalDialog (ACAPI_GetOwnResModule (), DialogResourceId, ACAPI_GetOwnResModule ()),
		systemPopup (GetReference (), SystemPopupId),
		refreshButton (GetReference (), RefreshButtonId),
		statusText (GetReference (), StatusTextId),
		table (GetReference (), TableId),
		detailsGroup (GetReference (), DetailsGroupId),
		detail1 (GetReference (), DetailText1Id),
		detail2 (GetReference (), DetailText2Id),
		detail3 (GetReference (), DetailText3Id),
		detail4 (GetReference (), DetailText4Id),
		detail5 (GetReference (), DetailText5Id),
		exportJsonButton (GetReference (), ExportJsonButtonId),
		exportCsvButton (GetReference (), ExportCsvButtonId),
		closeButton (GetReference (), CloseButtonId)
{
	SetTitle (GS::UniString (ADDON_NAME) + " " + ADDON_VERSION);

	Attach (*this);					// PanelObserver
	systemPopup.Attach (*this);		// PopUpObserver
	refreshButton.Attach (*this);	// ButtonItemObserver
	table.Attach (*this);			// ListBoxObserver
	exportJsonButton.Attach (*this);
	exportCsvButton.Attach (*this);
	closeButton.Attach (*this);

	InitTable ();

	ModelReader::ResolveElementIdPropertyGuid (elemIdPropGuid, elemIdPropNote);

	LoadSystems ();

	if (selectedSystem != APINULLGuid)
		RefreshData ();
	else
		ClearDetails ();
}


void CostWavesDialog::InitTable ()
{
	const short columnCount = 6;

	// En-têtes de colonnes (indices DG : 1-based).
	table.SetHeaderItemCount (columnCount);
	table.SetHeaderItemText (1, FR ("Type"));
	table.SetHeaderItemText (2, FR ("GUID"));
	table.SetHeaderItemText (3, FR ("ID élément"));
	table.SetHeaderItemText (4, FR ("Étage"));
	table.SetHeaderItemText (5, FR ("Classe"));
	table.SetHeaderItemText (6, FR ("Quantités disponibles"));

	// Colonnes : positions calculées sur la largeur du contrôle.
	const short tableWidth = table.GetWidth ();
	const short proportions[columnCount] = { 9, 24, 12, 14, 17, 24 };

	const short totalProportion = 100;
	short position = 0;
	for (short i = 1; i <= columnCount; ++i) {
		const short width = static_cast<short> ((tableWidth * proportions[i - 1]) / totalProportion);
		table.SetHeaderItemSize (i, width);
		table.SetHeaderItemSizeableFlag (i, true);
		table.SetTabFieldProperties (i, position, position + width,
									 DG::ListBox::Left, DG::ListBox::MiddleTruncate, i > 1);
		position = static_cast<short> (position + width);
	}
}


void CostWavesDialog::LoadSystems ()
{
	isFilling = true;

	systems = ModelReader::GetClassificationSystems ();

	for (UIndex i = 0; i < systems.GetSize (); ++i) {
		systemPopup.AppendItem ();
		systemPopup.SetItemText (static_cast<short> (i + 1), systems[i].name);
	}

	if (!systems.IsEmpty ()) {
		systemPopup.SelectItem (1);
		selectedSystem = systems[0].guid;
	}

	isFilling = false;
}


void CostWavesDialog::RefreshData ()
{
	if (selectedSystem == APINULLGuid) {
		SetDetailLine (1, FR ("Aucun système de classification trouvé dans le projet."));
		SetDetailLine (2, FR ("Créez / importez un système dans Archicad (Option > Classifications…) puis Actualiser."));
		SetDetailLine (3, GS::UniString ());
		SetDetailLine (4, GS::UniString ());
		SetDetailLine (5, GS::UniString ());
		return;
	}

	isFilling = true;

	rows.Clear ();
	ModelReader::Scan (selectedSystem, elemIdPropGuid, rows, report);
	FillTable ();

	isFilling = false;

	UpdateStatus ();
	UpdateDetails (1);
}


void CostWavesDialog::FillTable ()
{
	table.SetHeaderSynchronState (true);

	while (table.GetItemCount () > 0)
		table.DeleteItem (1);

	displayRows.Clear ();

	for (UIndex e = 0; e < rows.GetSize (); ++e) {
		const CWElementRow& element = rows[e];

		const GS::UniString floorText = element.storyName.IsEmpty ()
			? GS::ToUniString (std::to_wstring (static_cast<int> (element.floorInd)))
			: GS::ToUniString (std::to_wstring (static_cast<int> (element.floorInd))) + " - " + element.storyName;

		const GS::UniString classText = element.classItemId.IsEmpty ()
			? element.classItemName
			: element.classItemId + " - " + element.classItemName;

		table.AppendItem ();
		const short itemIndex = table.GetItemCount ();
		table.SetTabItemText (itemIndex, 1, FR ("Élément"));
		table.SetTabItemText (itemIndex, 2, APIGuidToString (element.guid));
		table.SetTabItemText (itemIndex, 3, element.elementId);
		table.SetTabItemText (itemIndex, 4, floorText);
		table.SetTabItemText (itemIndex, 5, classText);
		table.SetTabItemText (itemIndex, 6, QuantitiesSummary (element.quantities, 3));

		DisplayRow elementRow;
		elementRow.kind = RowKind::Element;
		elementRow.elementIndex = e;
		displayRows.Push (elementRow);

		for (UIndex c = 0; c < element.components.GetSize (); ++c) {
			const CWComponentRow& component = element.components[c];

			table.AppendItem ();
			const short compItemIndex = table.GetItemCount ();
			table.SetTabItemText (compItemIndex, 1,
				component.kind == RowKind::Skin ? FR ("Composant (skin)") : FR ("Composant"));
			table.SetTabItemText (compItemIndex, 2,
				component.guid == APINULLGuid ? GS::UniString () : APIGuidToString (component.guid));
			table.SetTabItemText (compItemIndex, 3, element.elementId);
			table.SetTabItemText (compItemIndex, 4, floorText);
			table.SetTabItemText (compItemIndex, 5, classText);
			table.SetTabItemText (compItemIndex, 6,
				QuantitiesSummary (component.quantities, 3));

			DisplayRow componentRow;
			componentRow.kind = component.kind;
			componentRow.elementIndex = e;
			componentRow.componentIndex = c;
			displayRows.Push (componentRow);
		}
	}

	if (table.GetItemCount () > 0)
		table.SelectItem (1);
}


void CostWavesDialog::UpdateStatus ()
{
	GS::UniString status = GS::ToUniString (std::to_wstring (static_cast<int> (report.classifiedElements)))
		+ FR (" éléments classés · ")
		+ GS::ToUniString (std::to_wstring (static_cast<int> (report.componentCount)))
		+ FR (" composants · ")
		+ GS::ToUniString (std::to_wstring (static_cast<int> (report.skinCount)))
		+ FR (" skins · ")
		+ GS::ToUniString (std::to_wstring (static_cast<int> (report.scannedElements)))
		+ FR (" éléments analysés");

	if (report.quantityErrors > 0) {
		status += FR (" · ⚠ ")
			+ GS::ToUniString (std::to_wstring (static_cast<int> (report.quantityErrors)))
			+ FR (" erreurs de quantités");
	}

	statusText.SetText (status);
}


void CostWavesDialog::SetDetailLine (short lineIndex, const GS::UniString& text)
{
	switch (lineIndex) {
		case 1: detail1.SetText (text); break;
		case 2: detail2.SetText (text); break;
		case 3: detail3.SetText (text); break;
		case 4: detail4.SetText (text); break;
		case 5: detail5.SetText (text); break;
		default: break;
	}
}


void CostWavesDialog::ClearDetails ()
{
	for (short i = 1; i <= 5; ++i)
		SetDetailLine (i, GS::UniString ());
}


void CostWavesDialog::UpdateDetails (short listItem)
{
	ClearDetails ();

	if (listItem < 1 || static_cast<UIndex> (listItem) > displayRows.GetSize ())
		return;

	const DisplayRow& displayRow = displayRows[static_cast<UIndex> (listItem) - 1];
	if (displayRow.elementIndex >= rows.GetSize ())
		return;

	CWElementRow& element = rows[displayRow.elementIndex];

	const GS::UniString floorText = element.storyName.IsEmpty ()
		? GS::ToUniString (std::to_wstring (static_cast<int> (element.floorInd)))
		: GS::ToUniString (std::to_wstring (static_cast<int> (element.floorInd))) + " - " + element.storyName;

	if (displayRow.kind == RowKind::Element) {
		SetDetailLine (1, FR ("Élément — ") + element.typeName + " — " + APIGuidToString (element.guid));
		SetDetailLine (2, FR ("ID : ") + (element.elementId.IsEmpty () ? FR ("(vide)") : element.elementId)
			+ FR (" · Étage : ") + floorText
			+ FR (" · Type : ") + element.typeName);
		SetDetailLine (3, FR ("Classe : ") + element.classItemId + " (" + element.classItemName + ")");

		// Quantités complètes, réparties sur les lignes restantes.
		GS::UniString line;
		short lineIndex = 4;
		for (UIndex q = 0; q < element.quantities.GetSize (); ++q) {
			const GS::UniString chunk = element.quantities[q].label + " = "
				+ FormatValue (element.quantities[q].value) + " " + element.quantities[q].unit;
			if (line.IsEmpty ())
				line = chunk;
			else if (LineFits (line, chunk))
				line += US (" · ") + chunk;
			else {
				SetDetailLine (lineIndex, line);
				++lineIndex;
				if (lineIndex > 5)
					return;
				line = chunk;
			}
		}
		if (!line.IsEmpty () && lineIndex <= 5)
			SetDetailLine (lineIndex, line);
		return;
	}

	// --- Ligne composant / skin ---
	if (displayRow.componentIndex >= element.components.GetSize ())
		return;

	CWComponentRow& component = element.components[displayRow.componentIndex];

	if (component.kind == RowKind::Skin) {
		SetDetailLine (1, FR ("Skin (composite) — matériau : ") + component.label);
		SetDetailLine (2, FR ("Élément parent — ") + element.typeName + " — " + APIGuidToString (element.guid));
	} else {
		SetDetailLine (1, FR ("Composant — ") + APIGuidToString (component.guid));
		SetDetailLine (2, FR ("Élément parent — ") + element.typeName + " — "
			+ APIGuidToString (element.guid) + " · ID : " + element.elementId);
	}

	short lineIndex = 3;
	if (!component.quantities.IsEmpty ()) {
		GS::UniString line;
		for (UIndex q = 0; q < component.quantities.GetSize (); ++q) {
			const GS::UniString chunk = component.quantities[q].label + " = "
				+ FormatValue (component.quantities[q].value) + " " + component.quantities[q].unit;
			if (line.IsEmpty ())
				line = chunk;
			else {
				SetDetailLine (lineIndex, line);
				++lineIndex;
				if (lineIndex > 5)
					return;
				line = chunk;
			}
		}
		if (!line.IsEmpty () && lineIndex <= 5)
			SetDetailLine (lineIndex, line);
		++lineIndex;
	}

	// Propriétés du composant, lues à la demande.
	if (component.kind == RowKind::Component && !component.propertiesFetched) {
		API_ElemComponentID componentId;
		BNZeroMemory (&componentId, sizeof (componentId));
		componentId.elemGuid = element.guid;
		componentId.componentID.componentGuid = component.guid;

		component.properties = ModelReader::GetComponentProperties (componentId);
		component.propertiesFetched = true;
	}

	if (component.kind == RowKind::Component) {
		if (component.properties.IsEmpty ()) {
			SetDetailLine (lineIndex <= 5 ? lineIndex : 5, FR ("Aucune propriété de composant."));
		} else {
			GS::UniString line;
			for (UIndex p = 0; p < component.properties.GetSize (); ++p) {
				const GS::UniString chunk = component.properties[p].name + " : " + component.properties[p].value;
				if (line.IsEmpty ())
					line = chunk;
				else if (LineFits (line, chunk))
					line += US (" · ") + chunk;
				else {
					SetDetailLine (lineIndex, line);
					++lineIndex;
					if (lineIndex > 5)
						return;
					line = chunk;
				}
			}
			if (!line.IsEmpty () && lineIndex <= 5)
				SetDetailLine (lineIndex, line);
		}
	}
}


void CostWavesDialog::Export (bool jsonFormat)
{
	if (rows.IsEmpty ()) {
		DG::WarningAlert (FR ("Aucune donnée à exporter."),
						  FR ("Lancez d'abord une lecture avec un système de classification."),
						  FR ("OK"));
		return;
	}

	GS::UniString systemName;
	for (UIndex i = 0; i < systems.GetSize (); ++i) {
		if (systems[i].guid == selectedSystem) {
			systemName = systems[i].name;
			break;
		}
	}

	GS::UniString path;
	GS::UniString error;

	const GSErrCode err = jsonFormat
		? Exporter::ExportJSON (systemName, rows, report, path, error)
		: Exporter::ExportCSV (systemName, rows, report, path, error);

	if (err == NoError) {
		DG::InformationAlert (FR ("Export réussi."), path, FR ("OK"));
	} else {
		DG::ErrorAlert (FR ("Échec de l'export."), error, FR ("OK"));
	}
}


void CostWavesDialog::PanelResized (const DG::PanelResizeEvent& ev)
{
	BeginMoveResizeItems ();

	const short dx = ev.GetHorizontalChange ();
	const short dy = ev.GetVerticalChange ();

	systemPopup.MoveAndResize (0, 0, dx, 0);
	refreshButton.Move (0, dy);
	statusText.MoveAndResize (0, 0, dx, 0);
	table.MoveAndResize (0, 0, dx, dy);
	detailsGroup.MoveAndResize (0, dy, dx, 0);
	detail1.MoveAndResize (0, dy, dx, 0);
	detail2.MoveAndResize (0, dy, dx, 0);
	detail3.MoveAndResize (0, dy, dx, 0);
	detail4.MoveAndResize (0, dy, dx, 0);
	detail5.MoveAndResize (0, dy, dx, 0);
	exportJsonButton.Move (0, dy);
	exportCsvButton.Move (0, dy);
	closeButton.MoveAndResize (dx, dy, 0, 0);

	EndMoveResizeItems ();
}


void CostWavesDialog::ButtonClicked (const DG::ButtonClickEvent& ev)
{
	if (ev.GetSource () == &refreshButton) {
		RefreshData ();
	} else if (ev.GetSource () == &exportJsonButton) {
		Export (true);
	} else if (ev.GetSource () == &exportCsvButton) {
		Export (false);
	} else if (ev.GetSource () == &closeButton) {
		PostCloseRequest (DG::ModalDialog::Cancel);
	}
}


void CostWavesDialog::PopUpChanged (const DG::PopUpChangeEvent& ev)
{
	if (isFilling)
		return;

	if (ev.GetSource () == &systemPopup) {
		const short selection = systemPopup.GetSelectedItem ();
		if (selection >= 1 && static_cast<UIndex> (selection) <= systems.GetSize ()) {
			selectedSystem = systems[static_cast<UIndex> (selection) - 1].guid;
			RefreshData ();
		}
	}
}


void CostWavesDialog::ListBoxSelectionChanged (const DG::ListBoxSelectionEvent& ev)
{
	if (isFilling)
		return;

	if (ev.GetSource () == &table)
		UpdateDetails (table.GetSelectedItem ());
}

} // namespace CostWaves
