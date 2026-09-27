#include "CostWavesPrecompiledHeader.hpp"

#include "ModelReader.hpp"

#include "UniStringWStringConversion.hpp"

#include <cwchar>

namespace CostWaves {

namespace {

// Littéral UTF-8 -> GS::UniString (les sources sont compilees avec /utf-8).
GS::UniString FR (const char* utf8Text)
{
	return GS::UniString (utf8Text, CC_UTF8);
}

// GUID integre de la propriete "Element ID" (stable entre projets).
const char* kElementIdPropertyGuidString = "B1B54D45-C951-42C9-9AF8-898F0BF212AB";

} // namespace


GS::Array<CWSystemInfo> ModelReader::GetClassificationSystems ()
{
	GS::Array<CWSystemInfo> result;

	GS::Array<API_ClassificationSystem> systems;
	const GSErrCode err = ACAPI_Classification_GetClassificationSystems (systems);
	if (err != NoError)
		return result;

	for (UIndex i = 0; i < systems.GetSize (); ++i) {
		result.Push (CWSystemInfo (systems[i].guid, systems[i].name, systems[i].editionVersion));
	}
	return result;
}


bool ModelReader::ResolveElementIdPropertyGuid (API_Guid& outGuid, GS::UniString& outNote)
{
	// 1) GUID integre connu : "Element ID" (groupe integre "ID and Categories").
	const API_Guid candidate = APIGuidFromString (kElementIdPropertyGuidString);

	API_PropertyDefinition definition;
	definition.guid = candidate;
	if (ACAPI_Property_GetPropertyDefinition (definition) == NoError) {
		outGuid = candidate;
		outNote = FR ("Propriété « Element ID » résolue par son GUID intégré.");
		return true;
	}

	// 2) Repli : recherche par nom parmi toutes les definitions de proprietes.
	GS::Array<API_PropertyGroup> groups;
	if (ACAPI_Property_GetPropertyGroups (groups) == NoError) {
		for (UIndex g = 0; g < groups.GetSize (); ++g) {
			GS::Array<API_PropertyDefinition> definitions;
			if (ACAPI_Property_GetPropertyDefinitions (groups[g].guid, definitions) == NoError) {
				for (UIndex d = 0; d < definitions.GetSize (); ++d) {
					const GS::UniString& name = definitions[d].name;
					if (name == GS::UniString ("Element ID") || name == FR ("ID d'élément")) {
						outGuid = definitions[d].guid;
						outNote = FR ("Propriété « Element ID » résolue par recherche de nom (groupe : ")
								  + groups[g].name + FR (").");
						return true;
					}
				}
			}
		}
	}

	outNote = FR ("Propriété « Element ID » introuvable — la colonne ID restera vide.");
	return false;
}


GS::UniString ModelReader::GetTypeName (const API_ElemType& type)
{
	static std::unordered_map<UInt32, GS::UniString> cache;

	const UInt32 key = (static_cast<UInt32> (type.typeID) << 12)
					 ^ static_cast<UInt32> (type.variationID);

	const auto it = cache.find (key);
	if (it != cache.end ())
		return it->second;

	GS::UniString name;
	if (ACAPI_Element_GetElemTypeName (type, name) != NoError || name.IsEmpty ())
		name = FR ("Type inconnu");

	cache.emplace (key, name);
	return name;
}


GS::UniString ModelReader::GetBuildingMaterialName (API_AttributeIndex index)
{
	API_Attribute attribute;
	BNZeroMemory (&attribute, sizeof (attribute));
	attribute.header.typeID = API_BuildingMaterialID;
	attribute.header.index = index;

	if (ACAPI_Attribute_Get (&attribute) != NoError)
		return FR ("Matériau ?");

	// header.name est une chaîne C UTF-8 (voir API_Attr_Head).
	return GS::UniString (attribute.header.name, CC_UTF8);
}


GS::UniString ModelReader::GetStoryName (const API_StoryInfo& storyInfo, short floorInd)
{
	if (storyInfo.data == nullptr)
		return GS::UniString ();

	const short index = static_cast<short> (floorInd - storyInfo.firstStory);
	if (index < 0)
		return GS::UniString ();

	const short count = static_cast<short> (storyInfo.lastStory - storyInfo.firstStory + 1);
	if (index >= count)
		return GS::UniString ();

	return GS::UniString ((*storyInfo.data)[index].uName);
}


GS::UniString ModelReader::GetElementIdValue (const API_Guid& elemGuid, const API_Guid& propGuid)
{
	API_Property property;
	if (ACAPI_Element_GetPropertyValue (elemGuid, propGuid, property) != NoError)
		return GS::UniString ();

	if (property.status != API_Property_HasValue)
		return GS::UniString ();

	return property.value.singleVariant.variant.uniStringValue;
}


void ModelReader::AddQuantity (GS::Array<CWQuantity>& outQuantities, const char* labelUtf8,
							   const char* unitUtf8, double value)
{
	outQuantities.Push (CWQuantity (FR (labelUtf8), value, FR (unitUtf8)));
}


void ModelReader::ExtractQuantities (API_ElemTypeID typeID, const API_ElementQuantity& quantity,
									 GS::Array<CWQuantity>& outQuantities)
{
	switch (typeID) {
		case API_WallID:
			AddQuantity (outQuantities, "Volume", "m³", quantity.wall.volume);
			AddQuantity (outQuantities, "Volume conditionné", "m³", quantity.wall.volume_cond);
			AddQuantity (outQuantities, "Surface côté ligne de réf.", "m²", quantity.wall.surface1);
			AddQuantity (outQuantities, "Surface côté opposé", "m²", quantity.wall.surface2);
			AddQuantity (outQuantities, "Surface arêtes", "m²", quantity.wall.surface3);
			AddQuantity (outQuantities, "Longueur", "m", quantity.wall.length);
			AddQuantity (outQuantities, "Surface fenêtres", "m²", quantity.wall.windowsSurf);
			AddQuantity (outQuantities, "Surface portes", "m²", quantity.wall.doorsSurf);
			AddQuantity (outQuantities, "Surface trous vides", "m²", quantity.wall.emptyholesSurf);
			break;

		case API_SlabID:
			AddQuantity (outQuantities, "Surface supérieure", "m²", quantity.slab.topSurface);
			AddQuantity (outQuantities, "Surface inférieure", "m²", quantity.slab.bottomSurface);
			AddQuantity (outQuantities, "Surface arêtes", "m²", quantity.slab.edgeSurface);
			AddQuantity (outQuantities, "Volume", "m³", quantity.slab.volume);
			AddQuantity (outQuantities, "Volume conditionné", "m³", quantity.slab.volume_cond);
			AddQuantity (outQuantities, "Périmètre", "m", quantity.slab.perimeter);
			AddQuantity (outQuantities, "Surface trous", "m²", quantity.slab.holesSurf);
			break;

		case API_ColumnID:
			AddQuantity (outQuantities, "Volume noyau", "m³", quantity.column.coreVolume);
			AddQuantity (outQuantities, "Volume revêtement", "m³", quantity.column.veneVolume);
			AddQuantity (outQuantities, "Surface noyau", "m²", quantity.column.coreSurface);
			AddQuantity (outQuantities, "Surface revêtement", "m²", quantity.column.veneSurface);
			AddQuantity (outQuantities, "Périmètre", "m", quantity.column.perimeter);
			break;

		case API_BeamID:
			AddQuantity (outQuantities, "Volume", "m³", quantity.beam.volume);
			AddQuantity (outQuantities, "Volume conditionné", "m³", quantity.beam.volume_cond);
			AddQuantity (outQuantities, "Surface", "m²", quantity.beam.area);
			AddQuantity (outQuantities, "Longueur gauche", "m", quantity.beam.leftLength);
			AddQuantity (outQuantities, "Longueur droite", "m", quantity.beam.rightLength);
			AddQuantity (outQuantities, "Surface supérieure", "m²", quantity.beam.topSurface);
			AddQuantity (outQuantities, "Surface inférieure", "m²", quantity.beam.bottomSurface);
			AddQuantity (outQuantities, "Surface arêtes", "m²", quantity.beam.edgeSurface);
			break;

		case API_WindowID:
		case API_DoorID:
			AddQuantity (outQuantities, "Surface", "m²", quantity.window.surface);
			AddQuantity (outQuantities, "Volume", "m³", quantity.window.volume);
			AddQuantity (outQuantities, "Largeur", "m", quantity.window.width1);
			AddQuantity (outQuantities, "Hauteur", "m", quantity.window.height1);
			AddQuantity (outQuantities, "Surface brute", "m²", quantity.window.grossSurf);
			AddQuantity (outQuantities, "Hauteur appui", "m", quantity.window.sillHeight);
			break;

		case API_ObjectID:
		case API_LampID:
			AddQuantity (outQuantities, "Surface", "m²", quantity.symb.surface);
			AddQuantity (outQuantities, "Volume", "m³", quantity.symb.volume);
			break;

		case API_SkylightID:
			AddQuantity (outQuantities, "Surface ouverture", "m²", quantity.skylight.openingSurface);
			AddQuantity (outQuantities, "Volume ouverture", "m³", quantity.skylight.openingVolume);
			AddQuantity (outQuantities, "Largeur ouverture", "m", quantity.skylight.openingWidth);
			AddQuantity (outQuantities, "Hauteur ouverture", "m", quantity.skylight.openingHeight);
			break;

		case API_MeshID:
			AddQuantity (outQuantities, "Surface supérieure", "m²", quantity.mesh.topSurface);
			AddQuantity (outQuantities, "Surface inférieure", "m²", quantity.mesh.bottomSurface);
			AddQuantity (outQuantities, "Surface arêtes", "m²", quantity.mesh.edgeSurface);
			AddQuantity (outQuantities, "Volume", "m³", quantity.mesh.volume);
			AddQuantity (outQuantities, "Périmètre", "m", quantity.mesh.perimeter);
			AddQuantity (outQuantities, "Surface projetée", "m²", quantity.mesh.projectedArea);
			break;

		case API_RoofID:
			AddQuantity (outQuantities, "Surface supérieure", "m²", quantity.roof.topSurface);
			AddQuantity (outQuantities, "Surface inférieure", "m²", quantity.roof.bottomSurface);
			AddQuantity (outQuantities, "Surface arêtes", "m²", quantity.roof.edgeSurface);
			AddQuantity (outQuantities, "Volume", "m³", quantity.roof.volume);
			AddQuantity (outQuantities, "Périmètre", "m", quantity.roof.perimeter);
			AddQuantity (outQuantities, "Longueur faîtages", "m", quantity.roof.ridgesLength);
			AddQuantity (outQuantities, "Longueur noues", "m", quantity.roof.valleysLength);
			AddQuantity (outQuantities, "Longueur rives", "m", quantity.roof.eavesLength);
			break;

		case API_ShellID:
			AddQuantity (outQuantities, "Surface de référence", "m²", quantity.shell.referenceSurface);
			AddQuantity (outQuantities, "Surface opposée", "m²", quantity.shell.oppositeSurface);
			AddQuantity (outQuantities, "Surface arêtes", "m²", quantity.shell.edgeSurface);
			AddQuantity (outQuantities, "Volume", "m³", quantity.shell.volume);
			AddQuantity (outQuantities, "Périmètre", "m", quantity.shell.perimeter);
			break;

		case API_MorphID:
			AddQuantity (outQuantities, "Surface", "m²", quantity.morph.surface);
			AddQuantity (outQuantities, "Volume", "m³", quantity.morph.volume);
			AddQuantity (outQuantities, "Surface au plan", "m²", quantity.morph.floorPlanArea);
			AddQuantity (outQuantities, "Périmètre au plan", "m", quantity.morph.floorPlanPerimeter);
			break;

		case API_ZoneID:
			AddQuantity (outQuantities, "Surface", "m²", quantity.zone.area);
			AddQuantity (outQuantities, "Surface nette", "m²", quantity.zone.netarea);
			AddQuantity (outQuantities, "Volume", "m³", quantity.zone.volume);
			AddQuantity (outQuantities, "Périmètre", "m", quantity.zone.perimeter);
			AddQuantity (outQuantities, "Hauteur", "m", quantity.zone.height);
			AddQuantity (outQuantities, "Surface murs", "m²", quantity.zone.wallsSurf);
			break;

		case API_StairID:
			AddQuantity (outQuantities, "Surface", "m²", quantity.stair.area);
			AddQuantity (outQuantities, "Volume", "m³", quantity.stair.volume);
			AddQuantity (outQuantities, "Hauteur", "m", quantity.stair.height);
			AddQuantity (outQuantities, "Longueur ligne de foulée", "m", quantity.stair.walklineLength);
			AddQuantity (outQuantities, "Nb contremarches", "U", static_cast<double> (quantity.stair.numOfRisers));
			AddQuantity (outQuantities, "Nb marches", "U", static_cast<double> (quantity.stair.numOfTreads));
			break;

		case API_RailingID:
			AddQuantity (outQuantities, "Surface", "m²", quantity.railing.area);
			AddQuantity (outQuantities, "Volume", "m³", quantity.railing.volume);
			AddQuantity (outQuantities, "Longueur 3D", "m", quantity.railing.length3D);
			break;

		case API_CurtainWallID:
			AddQuantity (outQuantities, "Longueur", "m", quantity.cw.length);
			AddQuantity (outQuantities, "Hauteur", "m", quantity.cw.height);
			AddQuantity (outQuantities, "Surface panneaux", "m²", quantity.cw.panelsSurface);
			AddQuantity (outQuantities, "Surface contour", "m²", quantity.cw.contourSurface);
			AddQuantity (outQuantities, "Surface limite", "m²", quantity.cw.boundarySurface);
			break;

		case API_HatchID:
			AddQuantity (outQuantities, "Surface", "m²", quantity.hatch.surface);
			AddQuantity (outQuantities, "Périmètre", "m", quantity.hatch.perimeter);
			break;

		default:
			// Type non couvert en phase 1 : aucune quantité extraite.
			break;
	}
}


GSErrCode ModelReader::GetSelectedElements (GS::Array<API_Guid>& outGuids)
{
	outGuids.Clear ();

	API_SelectionInfo selectionInfo;
	BNZeroMemory (&selectionInfo, sizeof (selectionInfo));

	GS::Array<API_Neig> selNeigs;
	const GSErrCode err = ACAPI_Selection_Get (&selectionInfo, &selNeigs, false);

	if (selectionInfo.typeID != API_SelEmpty && selectionInfo.marquee.coords != nullptr) {
		BMKillHandle (reinterpret_cast<GSHandle*> (&selectionInfo.marquee.coords));
	}

	if (err != NoError)
		return err;

	if (selectionInfo.typeID == API_SelEmpty)
		return NoError;		// rien de sélectionné : liste vide, pas une erreur

	for (UIndex i = 0; i < selNeigs.GetSize (); ++i) {
		const API_Guid& guid = selNeigs[i].guid;
		if (guid == APINULLGuid)
			continue;

		// Déduplication (plusieurs neigs peuvent viser le même élément).
		bool alreadyPresent = false;
		for (UIndex k = 0; k < outGuids.GetSize (); ++k) {
			if (outGuids[k] == guid) {
				alreadyPresent = true;
				break;
			}
		}
		if (!alreadyPresent)
			outGuids.Push (guid);
	}

	return NoError;
}


GSErrCode ModelReader::Scan (const API_Guid& systemGuid, const API_Guid& elemIdPropGuid,
							 const GS::Array<API_Guid>* elemFilter,
							 GS::Array<CWElementRow>& outRows, CWScanReport& outReport)
{
	outRows.Clear ();

	// 1) Noms des étages.
	API_StoryInfo storyInfo;
	BNZeroMemory (&storyInfo, sizeof (storyInfo));
	const bool haveStories = (ACAPI_ProjectSetting_GetStorySettings (&storyInfo) == NoError);

	// 2) Éléments à analyser : la sélection courante si un filtre est fourni,
	//    sinon tous les éléments du projet, quel que soit leur type.
	GS::Array<API_Guid> elemList;
	if (elemFilter != nullptr && !elemFilter->IsEmpty ()) {
		elemList = *elemFilter;
	} else {
		const GSErrCode listErr = ACAPI_Element_GetElemList (API_ZombieElemID, &elemList);
		if (listErr != NoError) {
			if (haveStories)
				BMKillHandle (reinterpret_cast<GSHandle*> (&storyInfo.data));
			return listErr;
		}
	}

	outReport = CWScanReport ();
	outReport.scannedElements = elemList.GetSize ();

	const bool haveElemIdProp = (elemIdPropGuid != APINULLGuid);

	for (UIndex i = 0; i < elemList.GetSize (); ++i) {
		const API_Guid& elemGuid = elemList[i];

		// 3) En-tête : type + étage.
		API_Elem_Head header;
		BNZeroMemory (&header, sizeof (header));
		header.guid = elemGuid;
		if (ACAPI_Element_GetHeader (&header) != NoError)
			continue;

		// 4) L'élément porte-t-il une classe dans le système choisi ?
		//    (item [out] : guid nul si l'élément n'est pas classé dans ce système)
		API_ClassificationItem item;
		if (ACAPI_Element_GetClassificationInSystem (elemGuid, systemGuid, item) != NoError)
			continue;
		if (item.guid == APINULLGuid)
			continue;	// pas de classe dans ce système

		CWElementRow row;
		row.guid = elemGuid;
		row.type = header.type;
		row.typeName = GetTypeName (header.type);
		row.floorInd = header.floorInd;
		if (haveStories)
			row.storyName = GetStoryName (storyInfo, header.floorInd);
		row.classItemId = item.id;
		row.classItemName = item.name;
		if (haveElemIdProp)
			row.elementId = GetElementIdValue (elemGuid, elemIdPropGuid);

		// 5) Quantités de l'élément + skins composites.
		API_ElementQuantity		elementQuantity;
		GS::Array<API_CompositeQuantity>	compositeQuantities;
		API_Quantities			quantities;
		API_QuantitiesMask		mask;
		API_QuantityPar			params;

		BNZeroMemory (&elementQuantity, sizeof (elementQuantity));
		BNZeroMemory (&quantities, sizeof (quantities));
		BNZeroMemory (&mask, sizeof (mask));
		BNZeroMemory (&params, sizeof (params));

		ACAPI_ELEMENT_QUANTITIES_MASK_SETFULL (mask);

		quantities.elements = &elementQuantity;
		quantities.composites = &compositeQuantities;

		if (ACAPI_Element_GetQuantities (elemGuid, &params, &quantities, &mask) == NoError) {
			ExtractQuantities (header.type.typeID, elementQuantity, row.quantities);

			for (UIndex s = 0; s < compositeQuantities.GetSize (); ++s) {
				const API_CompositeQuantity& skin = compositeQuantities[s];
				CWComponentRow skinRow;
				skinRow.kind = RowKind::Skin;
				skinRow.label = GetBuildingMaterialName (skin.buildMatIndices);
				AddQuantity (skinRow.quantities, "Volume", "m³", skin.volumes);
				AddQuantity (skinRow.quantities, "Surface projetée", "m²", skin.projectedArea);
				row.components.Push (skinRow);
				++outReport.skinCount;
			}
		} else {
			++outReport.quantityErrors;
		}

		// 6) Composants (API 25+).
		GS::Array<API_ElemComponentID> components;
		if (ACAPI_Element_GetComponents (elemGuid, components) == NoError) {
			for (UIndex c = 0; c < components.GetSize (); ++c) {
				CWComponentRow compRow;
				compRow.kind = RowKind::Component;
				compRow.guid = components[c].componentID.componentGuid;
				compRow.label = FR ("Composant ") + GS::ToUniString (std::to_wstring (static_cast<int> (c + 1)));
				row.components.Push (compRow);
				++outReport.componentCount;
			}
		}

		outRows.Push (row);
		++outReport.classifiedElements;
	}

	if (haveStories)
		BMKillHandle (reinterpret_cast<GSHandle*> (&storyInfo.data));

	return NoError;
}


GS::Array<CWPropertyEntry> ModelReader::GetComponentProperties (const API_ElemComponentID& component)
{
	GS::Array<CWPropertyEntry> result;

	GS::Array<API_PropertyDefinition> definitions;
	if (ACAPI_Element_GetPropertyDefinitions (component, API_PropertyDefinitionFilter_All, definitions) != NoError)
		return result;

	if (definitions.IsEmpty ())
		return result;

	// Pour un composant, seule la variante "ByGuid" existe (pas de GetPropertyValues composant).
	GS::Array<API_Guid> definitionGuids;
	for (UIndex d = 0; d < definitions.GetSize (); ++d)
		definitionGuids.Push (definitions[d].guid);

	GS::Array<API_Property> values;
	if (ACAPI_Element_GetPropertyValuesByGuid (component, definitionGuids, values) != NoError)
		return result;

	for (UIndex i = 0; i < definitions.GetSize () && i < values.GetSize (); ++i) {
		if (values[i].status != API_Property_HasValue)
			continue;

		CWPropertyEntry entry;
		entry.name = definitions[i].name;

		const API_Variant& variant = values[i].value.singleVariant.variant;
		switch (variant.type) {
			case API_PropertyStringValueType:
				entry.value = variant.uniStringValue;
				break;
			case API_PropertyRealValueType:
				entry.value = GS::ToUniString (std::to_wstring (variant.doubleValue));
				break;
			case API_PropertyIntegerValueType:
				entry.value = GS::ToUniString (std::to_wstring (variant.intValue));
				break;
			case API_PropertyBooleanValueType:
				entry.value = variant.boolValue ? FR ("vrai") : FR ("faux");
				break;
			case API_PropertyGuidValueType:
				entry.value = APIGuidToString (variant.guidValue);
				break;
			default:
				entry.value = FR ("(valeur non affichable)");
				break;
		}

		result.Push (entry);
	}

	return result;
}

} // namespace CostWaves
