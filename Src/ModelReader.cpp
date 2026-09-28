#include "CostWavesPrecompiledHeader.hpp"

#include <cstring>

#include "ModelReader.hpp"

#include "ArticleManager.hpp"
#include "RuleLibrary.hpp"
#include "UniStringWStringConversion.hpp"

#include <cwchar>

namespace CostWaves {

namespace {

// Littéral UTF-8 -> GS::UniString (les sources sont compilees avec /utf-8).
GS::UniString FR (const char* utf8Text)
{
	return GS::UniString (utf8Text, CC_UTF8);
}

// Littéral -> GS::UniString (départ de chaîne pour l'opérateur +).
GS::UniString US (const char* utf8Text)
{
	return GS::UniString (utf8Text, CC_UTF8);
}

// GUID integre de la propriete "Element ID" (stable entre projets).
const char* kElementIdPropertyGuidString = "B1B54D45-C951-42C9-9AF8-898F0BF212AB";

} // namespace


// --- Caches de lecture (phase 3) ----------------------------------------------

std::unordered_map<UInt32, GS::UniString>	ModelReader::typeNameCache;
std::unordered_map<UInt32, GS::UniString>	ModelReader::materialNameCache;
std::unordered_map<UInt32, GS::UniString>	ModelReader::layerNameCache;
std::unordered_map<UInt32, GS::UniString>	ModelReader::libPartNameCache;
std::unordered_map<UInt32, CWSkinInfo>		ModelReader::compositeCache;
// Classification du matériau dans le système scanné (index -> id, nom).
// id vide = connu SANS classe (mis en cache pour éviter les rappels API).
std::unordered_map<UInt32, GS::Pair<GS::UniString, GS::UniString>>	ModelReader::materialClassCache;


void ModelReader::ClearCaches ()
{
	materialClassCache.clear ();
	layerNameCache.clear ();
	typeNameCache.clear ();
	materialNameCache.clear ();
	compositeCache.clear ();
}


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
	const UInt32 key = (static_cast<UInt32> (type.typeID) << 12)
					 ^ static_cast<UInt32> (type.variationID);

	const auto it = typeNameCache.find (key);
	if (it != typeNameCache.end ())
		return it->second;

	GS::UniString name;
	if (ACAPI_Element_GetElemTypeName (type, name) != NoError || name.IsEmpty ())
		name = FR ("Type inconnu");

	typeNameCache.emplace (key, name);
	return name;
}


GS::UniString ModelReader::GetBuildingMaterialName (API_AttributeIndex index)
{
	// Phase 3 : un seul ACAPI_Attribute_Get par matériau distinct.
	const UInt32 key = static_cast<UInt32> (index.GenerateHashValue ());

	const auto it = materialNameCache.find (key);
	if (it != materialNameCache.end ())
		return it->second;

	API_Attribute attribute;
	BNZeroMemory (&attribute, sizeof (attribute));
	attribute.header.typeID = API_BuildingMaterialID;
	attribute.header.index = index;

	GS::UniString name = FR ("Matériau ?");
	if (ACAPI_Attribute_Get (&attribute) == NoError && attribute.header.name[0] != '\0')
		name = GS::UniString (attribute.header.name, CC_UTF8);

	materialNameCache.emplace (key, name);
	return name;
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


bool ModelReader::GetMaterialClassification (API_AttributeIndex materialIndex, const API_Guid& systemGuid,
											 GS::UniString& outItemId, GS::UniString& outItemName)
{
	outItemId.Clear ();
	outItemName.Clear ();

	if (!materialIndex.IsPositive ())
		return false;

	const UInt32 key = static_cast<UInt32> (materialIndex.ToInt32_Deprecated ());
	const auto cached = materialClassCache.find (key);
	if (cached != materialClassCache.end ()) {
		outItemId = cached->second.first;
		outItemName = cached->second.second;
		return !outItemId.IsEmpty ();
	}

	API_Attr_Head attrHead;
	BNZeroMemory (&attrHead, sizeof (attrHead));
	attrHead.typeID = API_BuildingMaterialID;
	attrHead.index = materialIndex;

	API_ClassificationItem item;
	if (ACAPI_Attribute_GetClassificationInSystem (attrHead, systemGuid, item) == NoError
		&& item.guid != APINULLGuid) {
		outItemId = item.id;
		outItemName = item.name;
	}

	GS::Pair<GS::UniString, GS::UniString> entry (outItemId, outItemName);
	materialClassCache[key] = entry;
	return !outItemId.IsEmpty ();
}


bool ModelReader::Is2DType (API_ElemTypeID typeID)
{
	switch (typeID) {
		case API_LineID:
		case API_PolyLineID:
		case API_SplineID:
		case API_ArcID:
		case API_CircleID:
		case API_HatchID:
			return true;
		default:
			return false;
	}
}


GS::UniString ModelReader::GetLayerName (API_AttributeIndex layerIndex)
{
	if (!layerIndex.IsPositive ())
		return GS::UniString ();

	// Phase 3 : un seul ACAPI_Attribute_Get par calque distinct.
	const UInt32 key = static_cast<UInt32> (layerIndex.GenerateHashValue ());
	const auto it = layerNameCache.find (key);
	if (it != layerNameCache.end ())
		return it->second;

	GS::UniString name;
	API_Attribute attribute;
	BNZeroMemory (&attribute, sizeof (attribute));
	attribute.header.typeID = API_LayerID;
	attribute.header.index = layerIndex;

	if (ACAPI_Attribute_Get (&attribute) == NoError)
		name = GS::UniString (attribute.header.name, CC_UTF8);

	layerNameCache.emplace (key, name);
	return name;
}


// Index de l'objet de bibliothèque d'un élément (portes/fenêtres via
// openingBase, objets/lampes via le struct commun — pattern DevKit).
namespace {

Int32 LibIndOfElement (const API_Guid& elemGuid, API_ElemTypeID typeID)
{
	API_Element elem;
	BNZeroMemory (&elem, sizeof (elem));
	elem.header.guid = elemGuid;
	if (ACAPI_Element_Get (&elem) != NoError)
		return -1;

	switch (typeID) {
		case API_ObjectID:
		case API_LampID:
			return elem.object.libInd;
		case API_DoorID:
		case API_WindowID:
			return elem.window.openingBase.libInd;
		default:
			return -1;
	}
}

} // namespace


GS::UniString ModelReader::GetLibraryPartName (Int32 libInd)
{
	if (libInd <= 0)
		return GS::UniString ();

	// Un seul ACAPI_LibraryPart_Get par objet de bibliothèque distinct.
	const UInt32 key = static_cast<UInt32> (libInd);
	const auto it = libPartNameCache.find (key);
	if (it != libPartNameCache.end ())
		return it->second;

	GS::UniString name;
	API_LibPart libPart;
	BNZeroMemory (&libPart, sizeof (libPart));
	libPart.index = libInd;
	if (ACAPI_LibraryPart_Get (&libPart) == NoError)
		name = GS::UniString (libPart.docu_UName);
	// ACAPI_LibraryPart_Get alloue libPart.location : le libérer.
	delete libPart.location;

	libPartNameCache.emplace (key, name);
	return name;
}


namespace {

const double kPi = 3.14159265358979323846;

// Longueur d'un arc de cercle defined par sa corde et son angle :
// r = corde / (2 sin(|angle|/2)), longueur = r * |angle|.
double ArcLengthFromChordAngle (double chord, double angle)
{
	if (fabs (angle) < 1e-9 || fabs (chord) < 1e-12)
		return chord;
	const double half = fabs (angle) / 2.0;
	if (half >= kPi - 1e-9)
		return kPi * chord / 2.0;		// demi-cercle et plus
	const double radius = chord / (2.0 * sin (half));
	return radius * fabs (angle);
}

// Marque les arêtes couvertes par un enregistrement d'arc : les arêtes
// partant de begIndex jusqu'à endIndex EXCLU (l'arête partant d'endIndex
// est droite), en bouclant sur le contour si besoin.
void MarkArcEdges (const API_PolyArc& arc, Int32 nCoords, std::vector<bool>& isArcEdge)
{
	if (arc.begIndex == arc.endIndex)
		return;		// arc complet : corde nulle, géré par ArcEdgesTotal

	Int32 i = arc.begIndex;
	const Int32 safety = 2 * nCoords + 2;
	for (Int32 step = 0; step <= safety; ++step) {
		if (i == arc.endIndex)
			return;			// l'arc s'arrête AU sommet endIndex
		isArcEdge[static_cast<size_t> (i)] = true;
		i = (i % nCoords) + 1;
	}
}

// Somme des longueurs d'arc (un enregistrement API_PolyArc = un arc).
double ArcEdgesTotal (const API_Coord* coords, Int32 nCoords, const API_PolyArc* arcs, Int32 nArcs)
{
	if (arcs == nullptr || nArcs <= 0)
		return 0.0;

	double total = 0.0;
	for (Int32 a = 0; a < nArcs; ++a) {
		const API_PolyArc& arc = arcs[a];
		if (arc.begIndex < 1 || arc.endIndex < 1 || arc.begIndex > nCoords || arc.endIndex > nCoords)
			continue;
		const API_Coord& c1 = coords[arc.begIndex - 1];
		const API_Coord& c2 = coords[arc.endIndex - 1];
		const double chord = sqrt ((c2.x - c1.x) * (c2.x - c1.x)
									 + (c2.y - c1.y) * (c2.y - c1.y));
		total += ArcLengthFromChordAngle (chord, arc.arcAngle);
	}
	return total;
}

} // namespace


void ModelReader::Extract2DQuantities (const API_Guid& elemGuid, API_ElemTypeID typeID,
										   GS::Array<CWQuantity>& outQuantities)
{
	switch (typeID) {
		case API_LineID:
		case API_PolyLineID:
		case API_SplineID:
		case API_ArcID:
		case API_CircleID:
			break;

		case API_HatchID:
		default:
			// Les hachures passent par le pipeline des quantités
			// (API_HatchQuantity : surface + périmètre) — rien à calculer ici.
			return;
	}

	API_Element element;
	BNZeroMemory (&element, sizeof (element));
	element.header.guid = elemGuid;
	if (ACAPI_Element_Get (&element) != NoError)
		return;

	if (typeID == API_LineID) {
		// §5 : longueur géométrique du segment.
		const API_Coord& beg = element.line.begC;
		const API_Coord& end = element.line.endC;
		const double length = sqrt ((end.x - beg.x) * (end.x - beg.x)
										 + (end.y - beg.y) * (end.y - beg.y));
		AddQuantity (outQuantities, "Longueur", "m", length);
		return;
	}

	if (typeID == API_ArcID || typeID == API_CircleID) {
		// §5/§6 : arc → longueur d'arc ; cercle → circonférence, surface,
		// rayon, diamètre (l'utilisateur choisit la mesure à facturer).
		const API_ArcType& arc = (typeID == API_CircleID) ? element.circle : element.arc;
		const double r = arc.r;

		double span = fabs (arc.endAng - arc.begAng);
		if (span > 2.0 * kPi)
			span = 2.0 * kPi;

		if (typeID == API_CircleID || arc.whole) {
			AddQuantity (outQuantities, "Circonférence", "m", 2.0 * kPi * r);
			AddQuantity (outQuantities, "Surface", "m²", kPi * r * r);
			AddQuantity (outQuantities, "Rayon", "m", r);
			AddQuantity (outQuantities, "Diamètre", "m", 2.0 * r);
		} else {
			AddQuantity (outQuantities, "Longueur", "m", r * span);
			AddQuantity (outQuantities, "Rayon", "m", r);
		}
		return;
	}

	// Polylignes et splines : coordonnées via le memo (masque polygone).
	API_ElementMemo memo;
	BNZeroMemory (&memo, sizeof (memo));
	if (ACAPI_Element_GetMemo (elemGuid, &memo, APIMemoMask_Polygon) != NoError)
		return;

	if (typeID == API_PolyLineID) {
		const Int32 nCoords = element.polyLine.poly.nCoords;
		const Int32 nSubPolys = element.polyLine.poly.nSubPolys > 0
			? element.polyLine.poly.nSubPolys : 1;
		if (memo.coords != nullptr && *memo.coords != nullptr && nCoords >= 2) {
			const API_Coord* coords = *memo.coords;
			const API_PolyArc* arcs = (memo.parcs != nullptr && *memo.parcs != nullptr)
				? *memo.parcs : nullptr;
			const Int32 nArcs = arcs != nullptr ? element.polyLine.poly.nArcs : 0;

			// Fins de sous-contours (1-based) : (*pends)[k] = dernier sommet
			// du (k+1)-ième sous-contour. Sans pends : contour unique.
			std::vector<Int32> subEnds;
			subEnds.push_back (nCoords);
			if (nSubPolys > 1 && memo.pends != nullptr && *memo.pends != nullptr) {
				subEnds.clear ();
				const Int32 nEnds = static_cast<Int32> (BMGetHandleSize (
					reinterpret_cast<GSHandle> (memo.pends)) / sizeof (Int32));
				for (Int32 e = 0; e < nEnds && e < nSubPolys; ++e)
					subEnds.push_back ((*memo.pends)[e]);
				if (subEnds.empty ())
					subEnds.push_back (nCoords);
			}

			// Arête sortante du sommet i : vers i+1, ou retour au début du
			// sous-contour si i en est le dernier sommet.
			auto nextOf = [&subEnds, nCoords] (Int32 i) -> Int32 {
				for (size_t k = 0; k < subEnds.size (); ++k) {
					if (subEnds[k] == i)
						return (k == 0) ? 1 : subEnds[k - 1] + 1;
				}
				return (i % nCoords) + 1;
			};

			// Arêtes droites hors arcs…
			std::vector<bool> isArcEdge (static_cast<size_t> (nCoords) + 1, false);
			for (Int32 a = 0; a < nArcs; ++a) {
				if (arcs[a].begIndex >= 1 && arcs[a].begIndex <= nCoords
					&& arcs[a].endIndex >= 1 && arcs[a].endIndex <= nCoords)
					MarkArcEdges (arcs[a], nCoords, isArcEdge);
			}
			double straight = 0.0;
			for (Int32 i = 1; i <= nCoords; ++i) {
				if (isArcEdge[static_cast<size_t> (i)])
					continue;
				const API_Coord& c1 = coords[i - 1];
				const API_Coord& c2 = coords[nextOf (i) - 1];
				straight += sqrt ((c2.x - c1.x) * (c2.x - c1.x)
									+ (c2.y - c1.y) * (c2.y - c1.y));
			}

			// … + longueurs d'arcs (corde/angle -> longueur d'arc).
			AddQuantity (outQuantities, "Longueur", "m", straight + ArcEdgesTotal (coords, nCoords, arcs, nArcs));
		}
		ACAPI_DisposeElemMemoHdls (&memo);
		return;
	}

	if (typeID == API_SplineID) {
		// §5 : longueur géométrique (approximation : polyligne joignant les
		// points de la spline ; l'index 0 est un point utilisé).
		if (memo.coords != nullptr && *memo.coords != nullptr) {
			const Int32 nPoints = static_cast<Int32> (BMGetHandleSize (
				reinterpret_cast<GSHandle> (memo.coords)) / sizeof (API_Coord));
			if (nPoints >= 2) {
				const API_Coord* coords = *memo.coords;
				double length = 0.0;
				for (Int32 i = 1; i < nPoints; ++i) {
					const API_Coord& c1 = coords[i - 1];
					const API_Coord& c2 = coords[i];
					length += sqrt ((c2.x - c1.x) * (c2.x - c1.x)
									   + (c2.y - c1.y) * (c2.y - c1.y));
				}
				if (element.spline.closed && nPoints > 2) {
					const API_Coord& c1 = coords[nPoints - 1];
					const API_Coord& c2 = coords[0];
					length += sqrt ((c2.x - c1.x) * (c2.x - c1.x)
									   + (c2.y - c1.y) * (c2.y - c1.y));
				}
				AddQuantity (outQuantities, "Longueur", "m", length);
			}
		}
		ACAPI_DisposeElemMemoHdls (&memo);
		return;
	}
}


API_AttributeIndex ModelReader::GetCompositeIndexOfElement (const API_Guid& elemGuid, API_ElemTypeID typeID)
{
	// Types exposant une structure composite : mur, dallage (champ .composite),
	// toit et coquille (via le champ commun shellBase).
	switch (typeID) {
		case API_WallID:
		case API_SlabID:
		case API_RoofID:
		case API_ShellID:
			break;
		default:
			return APIInvalidAttributeIndex;
	}

	API_Element element;
	BNZeroMemory (&element, sizeof (element));
	element.header.guid = elemGuid;

	if (ACAPI_Element_Get (&element) != NoError)
		return APIInvalidAttributeIndex;

	switch (typeID) {
		case API_WallID:	return element.wall.composite;
		case API_SlabID:	return element.slab.composite;
		case API_RoofID:	return element.roof.shellBase.composite;
		case API_ShellID:	return element.shell.shellBase.composite;
		default:			return APIInvalidAttributeIndex;
	}
}


bool ModelReader::GetCompositeInfo (API_AttributeIndex compositeIndex, CWSkinInfo& outInfo)
{
	outInfo = CWSkinInfo ();
	if (!compositeIndex.IsPositive ())
		return false;

	// Phase 3 : un seul couple ACAPI_Attribute_Get/GetDef par composite distinct.
	const UInt32 key = static_cast<UInt32> (compositeIndex.GenerateHashValue ());

	const auto it = compositeCache.find (key);
	if (it != compositeCache.end ()) {
		outInfo = it->second;
		return outInfo.valid;
	}

	CWSkinInfo info;

	// 1) Attribut composite : nom + épaisseur totale.
	API_Attribute attribute;
	BNZeroMemory (&attribute, sizeof (attribute));
	attribute.header.typeID = API_CompWallID;
	attribute.header.index = compositeIndex;

	if (ACAPI_Attribute_Get (&attribute) == NoError) {
		info.valid = true;
		info.name = GS::UniString (attribute.header.name, CC_UTF8);
		info.totalThickness = attribute.compWall.totalThick;		// mètres

		// 2) Couches du composite (définition étendue de l'attribut).
		API_AttributeDef defs;
		BNZeroMemory (&defs, sizeof (defs));
		if (ACAPI_Attribute_GetDef (API_CompWallID, compositeIndex, &defs) == NoError
			&& defs.cwall_compItems != nullptr) {
			const GSSize byteCount = BMGetHandleSize (reinterpret_cast<GSConstHandle> (defs.cwall_compItems));
			const long layerCount = static_cast<long> (byteCount / static_cast<GSSize> (sizeof (API_CWallComponent)));
			for (long l = 0; l < layerCount; ++l) {
				const API_CWallComponent& layer = (*defs.cwall_compItems)[l];
				CWSkinLayer skinLayer;
				skinLayer.buildingMaterial = layer.buildingMaterial;
				skinLayer.thickness = layer.fillThick;			// mètres
				skinLayer.core = (layer.flagBits & APICWallComp_Core) != 0;
				skinLayer.finish = (layer.flagBits & APICWallComp_Finish) != 0;
				info.layers.push_back (skinLayer);
			}
		}
		ACAPI_DisposeAttrDefsHdls (&defs);
	}

	compositeCache.emplace (key, info);
	outInfo = info;
	return info.valid;
}


void ModelReader::AddQuantity (GS::Array<CWQuantity>& outQuantities, const char* labelUtf8,
							   const char* unitUtf8, double value)
{
	outQuantities.Push (CWQuantity (FR (labelUtf8), value, FR (unitUtf8)));
}


// --- Quantités dérivées : ouvertures (fenêtres/portes) et objets GDL ------------

namespace {

// Ajout direct d'une quantité (le modèle CWQuantity est public).
void PushQuantity (GS::Array<CWQuantity>& outQuantities, const char* labelUtf8, const char* unitUtf8, double value)
{
	outQuantities.Push (CWQuantity (GS::UniString (labelUtf8, CC_UTF8), value, GS::UniString (unitUtf8, CC_UTF8)));
}

// Épaisseur du mur hôte d'une ouverture (fenêtre/porte) via son champ owner.
double HostWallThickness (const API_Guid& ownerGuid)
{
	if (ownerGuid == APINULLGuid)
		return 0.0;

	API_Element wall;
	BNZeroMemory (&wall, sizeof (wall));
	wall.header.guid = ownerGuid;
	if (ACAPI_Element_Get (&wall) != NoError)
		return 0.0;
	if (wall.header.type.typeID != API_WallID)
		return 0.0;
	return wall.wall.thickness;
}

// Fenêtres et portes (spec : quantité dérivée « enduit latéral ») : contour de
// l'ouverture, épaisseur du mur hôte et surface du tableau = contour × épaisseur.
void AddOpeningDerivedQuantities (const API_Guid& elemGuid, bool isDoor, double width, double height,
								  GS::Array<CWQuantity>& outQuantities)
{
	if (width <= 0.0 || height <= 0.0)
		return;

	API_Element elem;
	BNZeroMemory (&elem, sizeof (elem));
	elem.header.guid = elemGuid;
	if (ACAPI_Element_Get (&elem) != NoError)
		return;

	const API_Guid owner = isDoor ? elem.door.owner : elem.window.owner;
	const double thickness = HostWallThickness (owner);
	if (thickness <= 0.0)
		return;

	const double contour = 2.0 * (width + height);
	PushQuantity (outQuantities, "Contour ouverture", "m", contour);
	PushQuantity (outQuantities, "Épaisseur mur hôte", "m", thickness);
	PushQuantity (outQuantities, "Surface tableau", "m²", contour * thickness);
}

// Objets GDL : dimensions A/B (struct de l'élément) et ZZYZX (paramètres du
// memo, pattern DevKit : APIMemoMask_AddPars + handle de API_AddParType).
void AddGdlDimensionQuantities (const API_Guid& elemGuid, bool withStructSizes,
								GS::Array<CWQuantity>& outQuantities)
{
	if (withStructSizes) {
		API_Element objElem;
		BNZeroMemory (&objElem, sizeof (objElem));
		objElem.header.guid = elemGuid;
		if (ACAPI_Element_Get (&objElem) == NoError && objElem.header.type.typeID == API_ObjectID) {
			if (objElem.object.xRatio > 0.0)
				PushQuantity (outQuantities, "Largeur A", "m", objElem.object.xRatio);
			if (objElem.object.yRatio > 0.0)
				PushQuantity (outQuantities, "Profondeur B", "m", objElem.object.yRatio);
		}
	}

	API_ElementMemo memo;
	BNZeroMemory (&memo, sizeof (memo));
	if (ACAPI_Element_GetMemo (elemGuid, &memo, APIMemoMask_AddPars) != NoError)
		return;
	if (memo.params == nullptr || *memo.params == nullptr) {
		ACAPI_DisposeElemMemoHdls (&memo);
		return;
	}

	const GSSize nParams = BMGetHandleSize (reinterpret_cast<GSHandle> (memo.params))
		/ static_cast<GSSize> (sizeof (API_AddParType));
	double paramA = 0.0;
	double paramB = 0.0;
	double paramZZYZX = 0.0;
	bool hasParamA = false;
	bool hasParamB = false;
	for (GSIndex p = 0; p < nParams; ++p) {
		const API_AddParType& par = (*memo.params)[p];
		if (par.typeID != APIParT_Length)
			continue;
		if (strcmp (par.name, "A") == 0) {
			paramA = par.value.real;
			hasParamA = true;
		} else if (strcmp (par.name, "B") == 0) {
			paramB = par.value.real;
			hasParamB = true;
		} else if (strcmp (par.name, "ZZYZX") == 0) {
			paramZZYZX = par.value.real;
		}
	}
	ACAPI_DisposeElemMemoHdls (&memo);

	if (!withStructSizes && hasParamA && paramA > 0.0)
		PushQuantity (outQuantities, "Largeur A", "m", paramA);
	if (!withStructSizes && hasParamB && paramB > 0.0)
		PushQuantity (outQuantities, "Profondeur B", "m", paramB);
	if (paramZZYZX > 0.0)
		PushQuantity (outQuantities, "Hauteur ZZYZX", "m", paramZZYZX);
}

} // namespace


void ModelReader::ExtractQuantities (const API_Guid& elemGuid, API_ElemTypeID typeID,
									 const API_ElementQuantity& quantity,
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
			AddQuantity (outQuantities, "Surface", "m²", quantity.window.surface);
			AddQuantity (outQuantities, "Volume", "m³", quantity.window.volume);
			AddQuantity (outQuantities, "Largeur", "m", quantity.window.width1);
			AddQuantity (outQuantities, "Hauteur", "m", quantity.window.height1);
			AddQuantity (outQuantities, "Surface brute", "m²", quantity.window.grossSurf);
			AddQuantity (outQuantities, "Hauteur appui", "m", quantity.window.sillHeight);
			// Quantité dérivée : enduit latéral = contour de l'ouverture ×
			// épaisseur du mur hôte (utilisable dans les formules par article).
			AddOpeningDerivedQuantities (elemGuid, false, quantity.window.width1, quantity.window.height1,
										 outQuantities);
			break;

		case API_DoorID:
			// API_DoorQuantity = API_WindowQuantity, membre distinct de l'union.
			AddQuantity (outQuantities, "Surface", "m²", quantity.door.surface);
			AddQuantity (outQuantities, "Volume", "m³", quantity.door.volume);
			AddQuantity (outQuantities, "Largeur", "m", quantity.door.width1);
			AddQuantity (outQuantities, "Hauteur", "m", quantity.door.height1);
			AddQuantity (outQuantities, "Surface brute", "m²", quantity.door.grossSurf);
			AddQuantity (outQuantities, "Hauteur appui", "m", quantity.door.sillHeight);
			// Quantité dérivée : enduit latéral = contour de l'ouverture ×
			// épaisseur du mur hôte (portes, portes-fenêtres…).
			AddOpeningDerivedQuantities (elemGuid, true, quantity.door.width1, quantity.door.height1,
										 outQuantities);
			break;

		case API_ObjectID:
			AddQuantity (outQuantities, "Surface", "m²", quantity.symb.surface);
			AddQuantity (outQuantities, "Volume", "m³", quantity.symb.volume);
			// Dimensions GDL : A et B depuis l'élément, ZZYZX (hauteur) depuis
			// les paramètres de la bibliothèque.
			AddGdlDimensionQuantities (elemGuid, true, outQuantities);
			break;

		case API_LampID:
			// API_LightQuantity = API_ObjectQuantity, membre distinct de l'union.
			AddQuantity (outQuantities, "Surface", "m²", quantity.light.surface);
			AddQuantity (outQuantities, "Volume", "m³", quantity.light.volume);
			// Dimensions GDL via les paramètres de la bibliothèque (A/B/ZZYZX).
			AddGdlDimensionQuantities (elemGuid, false, outQuantities);
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

		// --- Phase 3 : sous-éléments de mur-rideau, escalier, garde-corps,
		//     segments de colonne et de poutre (union API_ElementQuantity). ---

		case API_CurtainWallFrameID:
			AddQuantity (outQuantities, "Largeur", "m", quantity.cwFrame.width);
			AddQuantity (outQuantities, "Profondeur", "m", quantity.cwFrame.depth);
			AddQuantity (outQuantities, "Longueur", "m", quantity.cwFrame.length);
			break;

		case API_CurtainWallPanelID:
			AddQuantity (outQuantities, "Surface", "m²", quantity.cwPanel.surface);
			AddQuantity (outQuantities, "Surface brute", "m²", quantity.cwPanel.grossSurface);
			AddQuantity (outQuantities, "Périmètre", "m", quantity.cwPanel.perimeter);
			AddQuantity (outQuantities, "Périmètre brut", "m", quantity.cwPanel.grossPerimeter);
			AddQuantity (outQuantities, "Largeur", "m", quantity.cwPanel.width);
			AddQuantity (outQuantities, "Hauteur", "m", quantity.cwPanel.height);
			AddQuantity (outQuantities, "Épaisseur", "mm", quantity.cwPanel.thickness * 1000.0);
			break;

		case API_CurtainWallAccessoryID:
			AddQuantity (outQuantities, "Largeur", "m", quantity.cwAccessory.width);
			AddQuantity (outQuantities, "Hauteur", "m", quantity.cwAccessory.height);
			AddQuantity (outQuantities, "Longueur", "m", quantity.cwAccessory.length);
			break;

		case API_RiserID:
			AddQuantity (outQuantities, "Volume", "m³", quantity.stairRiser.volume);
			AddQuantity (outQuantities, "Largeur", "m", quantity.stairRiser.width);
			AddQuantity (outQuantities, "Surface face", "m²", quantity.stairRiser.frontArea);
			break;

		case API_TreadID:
			AddQuantity (outQuantities, "Surface", "m²", quantity.stairTread.area);
			AddQuantity (outQuantities, "Volume", "m³", quantity.stairTread.volume);
			AddQuantity (outQuantities, "Épaisseur", "mm", quantity.stairTread.thickness * 1000.0);
			break;

		case API_StairStructureID:
			AddQuantity (outQuantities, "Volume", "m³", quantity.stairStructure.volume);
			AddQuantity (outQuantities, "Épaisseur", "mm", quantity.stairStructure.thickness * 1000.0);
			AddQuantity (outQuantities, "Longueur 3D", "m", quantity.stairStructure.length3D);
			break;

		case API_RailingToprailID:
			AddQuantity (outQuantities, "Volume", "m³", quantity.railingToprail.volume);
			AddQuantity (outQuantities, "Longueur 3D", "m", quantity.railingToprail.length3D);
			break;

		case API_RailingHandrailID:
			AddQuantity (outQuantities, "Volume", "m³", quantity.railingHandrail.volume);
			AddQuantity (outQuantities, "Longueur 3D", "m", quantity.railingHandrail.length3D);
			break;

		case API_RailingRailID:
			AddQuantity (outQuantities, "Volume", "m³", quantity.railingRail.volume);
			AddQuantity (outQuantities, "Longueur 3D", "m", quantity.railingRail.length3D);
			break;

		case API_RailingToprailEndID:
			AddQuantity (outQuantities, "Volume", "m³", quantity.railingToprailEnd.volume);
			AddQuantity (outQuantities, "Longueur 3D", "m", quantity.railingToprailEnd.length3D);
			break;

		case API_RailingHandrailEndID:
			AddQuantity (outQuantities, "Volume", "m³", quantity.railingHandrailEnd.volume);
			AddQuantity (outQuantities, "Longueur 3D", "m", quantity.railingHandrailEnd.length3D);
			break;

		case API_RailingRailEndID:
			AddQuantity (outQuantities, "Volume", "m³", quantity.railingRailEnd.volume);
			AddQuantity (outQuantities, "Longueur 3D", "m", quantity.railingRailEnd.length3D);
			break;

		case API_RailingToprailConnectionID:
			AddQuantity (outQuantities, "Volume", "m³", quantity.railingToprailConnection.volume);
			AddQuantity (outQuantities, "Longueur 3D", "m", quantity.railingToprailConnection.length3D);
			break;

		case API_RailingHandrailConnectionID:
			AddQuantity (outQuantities, "Volume", "m³", quantity.railingHandrailConnection.volume);
			AddQuantity (outQuantities, "Longueur 3D", "m", quantity.railingHandrailConnection.length3D);
			break;

		case API_RailingRailConnectionID:
			AddQuantity (outQuantities, "Volume", "m³", quantity.railingRailConnection.volume);
			AddQuantity (outQuantities, "Longueur 3D", "m", quantity.railingRailConnection.length3D);
			break;

		case API_RailingPostID:
			AddQuantity (outQuantities, "Volume", "m³", quantity.railingPost.volume);
			AddQuantity (outQuantities, "Longueur 3D", "m", quantity.railingPost.length3D);
			break;

		case API_RailingInnerPostID:
			AddQuantity (outQuantities, "Volume", "m³", quantity.railingInnerPost.volume);
			AddQuantity (outQuantities, "Longueur 3D", "m", quantity.railingInnerPost.length3D);
			break;

		case API_RailingBalusterID:
			AddQuantity (outQuantities, "Volume", "m³", quantity.railingBaluster.volume);
			AddQuantity (outQuantities, "Longueur 3D", "m", quantity.railingBaluster.length3D);
			break;

		case API_RailingPanelID:
			AddQuantity (outQuantities, "Volume", "m³", quantity.railingPanel.volume);
			break;

		case API_RailingSegmentID:
			AddQuantity (outQuantities, "Volume", "m³", quantity.railingSegment.volume);
			AddQuantity (outQuantities, "Longueur 3D", "m", quantity.railingSegment.length3D);
			break;

		case API_ColumnSegmentID:
			AddQuantity (outQuantities, "Volume brut", "m³", quantity.columnSegment.grossVolume);
			AddQuantity (outQuantities, "Volume noyau brut", "m³", quantity.columnSegment.coreGrossVolume);
			AddQuantity (outQuantities, "Volume revêtement brut", "m³", quantity.columnSegment.veneerGrossVolume);
			AddQuantity (outQuantities, "Surface noyau brute", "m²", quantity.columnSegment.coreGrossSurface);
			AddQuantity (outQuantities, "Surface revêtement brute", "m²", quantity.columnSegment.veneerGrossSurface);
			break;

		case API_BeamSegmentID:
			AddQuantity (outQuantities, "Volume", "m³", quantity.beamSegment.volume);
			AddQuantity (outQuantities, "Longueur", "m", quantity.beamSegment.length);
			AddQuantity (outQuantities, "Surface supérieure", "m²", quantity.beamSegment.topSurface);
			AddQuantity (outQuantities, "Surface inférieure", "m²", quantity.beamSegment.bottomSurface);
			break;

		default:
			// Type sans quantités exploitables (ex. jonction de mur-rideau,
			// nœud / motif de garde-corps) : aucune quantité extraite.
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


GSErrCode ModelReader::CollectGroupValues (const API_Guid& groupPropGuid,
										   GS::Array<GS::Pair<API_Guid, GS::UniString>>& outValues)
{
	outValues.Clear ();

	if (groupPropGuid == APINULLGuid)
		return NoError;

	GS::Array<API_Guid> elemList;
	if (ACAPI_Element_GetElemList (API_ZombieElemID, &elemList) != NoError)
		return APIERR_GENERAL;

	for (UIndex i = 0; i < elemList.GetSize (); ++i) {
		API_Property property;
		if (ACAPI_Element_GetPropertyValue (elemList[i], groupPropGuid, property) != NoError)
			continue;

		if (property.status != API_Property_HasValue)
			continue;
		if (property.value.singleVariant.variant.type != API_PropertyStringValueType)
			continue;

		GS::Pair<API_Guid, GS::UniString> entry (elemList[i],
												 property.value.singleVariant.variant.uniStringValue);
		outValues.Push (entry);
	}

	return NoError;
}


void ModelReader::FillQuantitiesAndSkins (const API_Guid& elemGuid, API_ElemTypeID typeID,
										  const API_Guid& systemGuid,
										  const GS::Array<CWMapRule>& rules,
										  const API_ElementQuantity& elementQuantity,
										  const GS::Array<API_CompositeQuantity>& compositeQuantities,
										  CWElementRow& outRow, CWScanReport& outReport)
{
	ExtractQuantities (elemGuid, typeID, elementQuantity, outRow.quantities);

	// Phase 3 : structure composite de l'élément (mur, dallage, toit, coquille)
	// pour enrichir chaque skin : nom du composite, épaisseur de couche,
	// position et flags cœur / finition.
	CWSkinInfo compositeInfo;
	const bool haveComposite = !compositeQuantities.IsEmpty ()
		&& GetCompositeInfo (GetCompositeIndexOfElement (elemGuid, typeID), compositeInfo);

	for (UIndex s = 0; s < compositeQuantities.GetSize (); ++s) {
		const API_CompositeQuantity& skin = compositeQuantities[s];
		CWComponentRow skinRow;
		skinRow.kind = RowKind::Skin;
		skinRow.label = GetBuildingMaterialName (skin.buildMatIndices);

		// Phase 5 : le skin porte la classe de son MATÉRIAU (un mur sans
		// classe dont les couches ont des matériaux classés est « appelé »).
		if (GetMaterialClassification (skin.buildMatIndices, systemGuid,
										  skinRow.classItemId, skinRow.classItemName))
			++outReport.classifiedSkins;

		// Nouvelle architecture (spec §4/§11) : règle du MATÉRIAU — prioritaire
		// sur la classification ; « Ignorer » exclut la couche du métré.
		const CWMapRule* materialRule = RuleLibrary::FindRule (rules, CWStructureType::BuildingMaterial,
															  skinRow.label);
		if (materialRule != nullptr && materialRule->ignored)
			continue;
		if (materialRule != nullptr) {
			skinRow.ruleArticleId = materialRule->articleId;
			skinRow.ruleQuantity = materialRule->quantity;
		}

		if (haveComposite) {
			// Correspondance skin -> couche du composite : par position
			// d'abord, repli sur le matériau si l'ordre ne correspond pas.
			long layerIndex = -1;
			if (s < compositeInfo.layers.size ()
				&& compositeInfo.layers[s].buildingMaterial == skin.buildMatIndices) {
				layerIndex = static_cast<long> (s);
			} else {
				for (UIndex l = 0; l < compositeInfo.layers.size (); ++l) {
					if (compositeInfo.layers[l].buildingMaterial == skin.buildMatIndices) {
						layerIndex = static_cast<long> (l);
						break;
					}
				}
			}

			if (layerIndex >= 0) {
				const CWSkinLayer& layer = compositeInfo.layers[layerIndex];
				skinRow.compositeName = compositeInfo.name;
				skinRow.skinIndex = static_cast<short> (layerIndex);
				skinRow.skinCount = static_cast<short> (compositeInfo.layers.size ());
				skinRow.coreSkin = layer.core;
				skinRow.finishSkin = layer.finish;
				AddQuantity (skinRow.quantities, "Épaisseur", "mm", layer.thickness * 1000.0);
			}
		}

		AddQuantity (skinRow.quantities, "Volume", "m³", skin.volumes);
		AddQuantity (skinRow.quantities, "Surface projetée", "m²", skin.projectedArea);
		outRow.components.Push (skinRow);
		++outReport.skinCount;
	}
}


GSErrCode ModelReader::Scan (const API_Guid& systemGuid, const API_Guid& elemIdPropGuid,
							 const API_Guid& groupPropGuid, bool include2D,
							 const GS::Array<CWMapRule>& rules,
							 const GS::Array<API_Guid>* elemFilter,
							 GS::Array<CWElementRow>& outRows, CWScanReport& outReport)
{
	outRows.Clear ();

	// Phase 3 : purge des caches d'attributs (noms matériaux, composites,
	// types) pour refléter d'éventuelles modifications depuis la dernière lecture.
	ClearCaches ();

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
	const bool haveGroupProp = (groupPropGuid != APINULLGuid);

	// --- Passe 1 : en-têtes + classification -----------------------------------
	// Les lignes sont créées ici (ordre = ordre du projet) ; les quantités et
	// composants sont remplis ensuite (passes 2 et 3).
	struct ItemInfo {
		API_Guid	guid = APINULLGuid;
		API_ElemType	type;
		UIndex		rowIndex = 0;
	};

	GS::Array<ItemInfo> items;
	std::unordered_map<UInt32, GS::Array<UIndex>> groupsByType;	// clé : typeID + variationID
	GS::Array<GS::UniString>	unmappedSeen;						// structures sans règle déjà comptées

	for (UIndex i = 0; i < elemList.GetSize (); ++i) {
		const API_Guid& elemGuid = elemList[i];

		// En-tête : type + étage.
		API_Elem_Head header;
		BNZeroMemory (&header, sizeof (header));
		header.guid = elemGuid;
		if (ACAPI_Element_GetHeader (&header) != NoError)
			continue;

		// Dessins 2D (spec §2/§4) : ligne, polyligne, spline, arc, cercle,
		// hachure — source de quantification indépendante, incluse sur
		// demande de l'utilisateur.
		const bool is2D = Is2DType (header.type.typeID);
		if (is2D && !include2D)
			continue;

		// --- Nouvelle architecture (spec §4/§7/§11) : structure native de
		// l'élément + règle de correspondance. La règle est PRIORITAIRE sur
		// la classification et détermine l'article, le niveau de métré
		// (élément/composant) et la quantité à adopter.
		CWStructureType structureType = CWStructureType::Composite;
		GS::UniString structureName;
		switch (header.type.typeID) {
			case API_ObjectID:
			case API_LampID:
			case API_DoorID:
			case API_WindowID: {
				structureType = CWStructureType::LibraryPart;
				structureName = GetLibraryPartName (LibIndOfElement (elemGuid, header.type.typeID));
				break;
			}
			case API_WallID:
			case API_SlabID:
			case API_RoofID:
			case API_ShellID: {
				structureType = CWStructureType::Composite;
				CWSkinInfo compositeInfo;
				if (GetCompositeInfo (GetCompositeIndexOfElement (elemGuid, header.type.typeID), compositeInfo))
					structureName = compositeInfo.name;
				break;
			}
			default:
				break;
		}

		const CWMapRule* structureRule = (!is2D && !structureName.IsEmpty ())
			? RuleLibrary::FindRule (rules, structureType, structureName)
			: nullptr;

		// Structure « Ignorer » (spec §4) : exclue du métré.
		if (structureRule != nullptr && structureRule->ignored)
			continue;

		API_ClassificationItem item;
		bool elementClassified = (ACAPI_Element_GetClassificationInSystem (elemGuid, systemGuid, item) == NoError
											   && item.guid != APINULLGuid);

		// Repli 2D : classe portée par la propriété CW_Article_ID
		// (affectée par l'utilisateur) si le dessin n'est pas classé.
		if (is2D && !elementClassified) {
			const API_Guid articlePropGuid = ArticleManager::FindArticleIdPropertyGuid ();
			if (articlePropGuid != APINULLGuid) {
				const GS::UniString articleId = GetElementIdValue (elemGuid, articlePropGuid);
				if (!articleId.IsEmpty ()) {
					item.id = articleId;
					item.name = articleId;
					elementClassified = true;
				}
			}
		}

		bool hasClassifiedSkin = false;
		if (!is2D && !elementClassified) {
			CWSkinInfo compositeInfo;
			if (GetCompositeInfo (GetCompositeIndexOfElement (elemGuid, header.type.typeID), compositeInfo)) {
				for (UIndex l = 0; l < compositeInfo.layers.size () && !hasClassifiedSkin; ++l) {
					GS::UniString skinClassId;
					GS::UniString skinClassName;
					hasClassifiedSkin = GetMaterialClassification (compositeInfo.layers[l].buildingMaterial,
																  systemGuid, skinClassId, skinClassName);
				}
			}
			if (!hasClassifiedSkin && structureRule == nullptr)
				continue;	// aucune classe, ni l'élément ni ses skins, ni règle
		}

		CWElementRow row;
		row.guid = elemGuid;
		row.type = header.type;
		row.typeName = GetTypeName (header.type);
		row.floorInd = header.floorInd;
		if (haveStories)
			row.storyName = GetStoryName (storyInfo, header.floorInd);
		row.is2D = is2D;
		row.layerName = GetLayerName (header.layer);
		row.structureType = structureType;
		row.structureName = structureName;
		if (structureRule != nullptr) {
			row.hasRule = true;
			row.ruleArticleId = structureRule->articleId;
			row.ruleMode = structureRule->mode;
			row.ruleQuantity = structureRule->quantity;
		}
		if (elementClassified) {
			row.classItemId = item.id;
			row.classItemName = item.name;
		}
		if (haveElemIdProp)
			row.elementId = GetElementIdValue (elemGuid, elemIdPropGuid);

		// Phase 4 : appartenance à un ensemble CostWaves (CW_Group_ID).
		if (haveGroupProp) {
			row.groupId = GetElementIdValue (elemGuid, groupPropGuid);
			row.consumed = !row.groupId.IsEmpty ();
		}

		ItemInfo info;
		info.guid = elemGuid;
		info.type = header.type;
		info.rowIndex = outRows.GetSize ();

		const UInt32 typeKey = (static_cast<UInt32> (header.type.typeID) << 12)
							 ^ static_cast<UInt32> (header.type.variationID);
		// Les hachures passent par le batch des quantités (surface/périmètre) ;
		// les autres dessins 2D sont calculés géométriquement en passe 1.
		if (!is2D || header.type.typeID == API_HatchID)
			groupsByType[typeKey].Push (items.GetSize ());
		else
			Extract2DQuantities (elemGuid, header.type.typeID, row.quantities);
		items.Push (info);

		outRows.Push (row);
		if (elementClassified) {
			if (is2D)
				++outReport.classified2D;
			else
				++outReport.classifiedElements;
		}

		// Structure présente dans la maquette mais sans règle (spec §7) :
		// ⚠ à configurer dans le gestionnaire de correspondances.
		if (!is2D && !structureName.IsEmpty () && structureRule == nullptr) {
			const GS::UniString unmappedKey = GS::ToUniString (std::to_wstring (
												  static_cast<int> (structureType))) + US ("|") + structureName;
			bool alreadySeen = false;
			for (UIndex u = 0; u < unmappedSeen.GetSize (); ++u) {
				if (unmappedSeen[u] == unmappedKey) {
					alreadySeen = true;
					break;
				}
			}
			if (!alreadySeen) {
				unmappedSeen.Push (unmappedKey);
				++outReport.unmappedStructures;
			}
		}
	}

	// --- Passe 2 : quantités, lues par lot (phase 3) ----------------------------
	// ACAPI_Element_GetMoreQuantities traite d'un seul appel tous les éléments
	// d'un même type ; en cas d'échec du lot, repli unitaire sur
	// ACAPI_Element_GetQuantities (ancien comportement).
	{
		API_QuantityPar params;
		BNZeroMemory (&params, sizeof (params));

		API_QuantitiesMask mask;
		BNZeroMemory (&mask, sizeof (mask));
		ACAPI_ELEMENT_QUANTITIES_MASK_SETFULL (mask);

		for (const auto& group : groupsByType) {
			const GS::Array<UIndex>& indices = group.second;
			if (indices.IsEmpty ())
				continue;

			// Buffers de sortie : un par élément du lot, adressés par les
			// API_Quantities (pattern du DevKit). Les std::vector sont
			// construits à la taille finale : les adresses restent stables.
			GS::Array<API_Guid> guids;
			std::vector<API_ElementQuantity> quantityBuffers (indices.GetSize ());
			std::vector<GS::Array<API_CompositeQuantity>> compositeBuffers (indices.GetSize ());
			std::vector<GS::Array<API_ElemPartQuantity>> elemPartBuffers (indices.GetSize ());
			std::vector<GS::Array<API_ElemPartCompositeQuantity>> elemPartCompositeBuffers (indices.GetSize ());

			GS::Array<API_Quantities> quantities;
			for (UIndex k = 0; k < indices.GetSize (); ++k) {
				const ItemInfo& info = items[indices[k]];
				guids.Push (info.guid);

				API_Quantities q;
				q.elements = &quantityBuffers[k];
				q.composites = &compositeBuffers[k];
				q.elemPartQuantities = &elemPartBuffers[k];
				q.elemPartComposites = &elemPartCompositeBuffers[k];
				quantities.Push (q);
			}

			const GSErrCode batchErr = ACAPI_Element_GetMoreQuantities (&guids, &params, &quantities, &mask);

			for (UIndex k = 0; k < indices.GetSize (); ++k) {
				const ItemInfo& info = items[indices[k]];
				CWElementRow& row = outRows[info.rowIndex];

				if (batchErr == NoError) {
					FillQuantitiesAndSkins (info.guid, info.type.typeID, systemGuid, rules, quantityBuffers[k],
											compositeBuffers[k], row, outReport);
				} else {
					// Repli unitaire pour ce lot.
					API_ElementQuantity elementQuantity;
					GS::Array<API_CompositeQuantity> compositeQuantities;
					GS::Array<API_ElemPartQuantity> elemPartQuantities;
					GS::Array<API_ElemPartCompositeQuantity> elemPartComposites;
					BNZeroMemory (&elementQuantity, sizeof (elementQuantity));

					API_Quantities single;
					single.elements = &elementQuantity;
					single.composites = &compositeQuantities;
					single.elemPartQuantities = &elemPartQuantities;
					single.elemPartComposites = &elemPartComposites;

					if (ACAPI_Element_GetQuantities (info.guid, &params, &single, &mask) == NoError) {
						FillQuantitiesAndSkins (info.guid, info.type.typeID, systemGuid, rules, elementQuantity,
												compositeQuantities, row, outReport);
					} else {
						++outReport.quantityErrors;
					}
				}
			}
		}
	}

	// --- Passe 3 : composants (API 25+) ------------------------------------------
	// Les dessins 2D sont des objets de métré à part entière : jamais de
	// composants (spec §2/§8).
	for (UIndex i = 0; i < items.GetSize (); ++i) {
		const ItemInfo& info = items[i];
		CWElementRow& row = outRows[info.rowIndex];
		if (row.is2D)
			continue;

		GS::Array<API_ElemComponentID> components;
		if (ACAPI_Element_GetComponents (info.guid, components) == NoError) {
			for (UIndex c = 0; c < components.GetSize (); ++c) {
				CWComponentRow compRow;
				compRow.kind = RowKind::Component;
				compRow.guid = components[c].componentID.componentGuid;
				compRow.label = FR ("Composant ") + GS::ToUniString (std::to_wstring (static_cast<int> (c + 1)));
				row.components.Push (compRow);
				++outReport.componentCount;
			}
		}
	}

	// --- Passe 4/5 : lignes « Ensemble » et « Groupe n » -------------------------
	// Une ligne virtuelle par CW_Group_ID distinct : l'ensemble (ou le groupe
	// numéroté, valeurs « CW-N-<n> ») est facturé comme une seule ligne, ses
	// membres sont « consommés » (affichés en sous-lignes, exclus de la
	// facturation individuelle). Pour un groupe numéroté, la quantité réelle
	// du métré est le NOMBRE de groupes de l'article.
	if (haveGroupProp) {
		GS::Array<GS::UniString>	groupIds;		// groupes déjà vus (ordre d'apparition)
		GS::Array<UIndex>			groupRowIndices;	// index de la ligne ensemble correspondante

		for (UIndex i = 0; i < outRows.GetSize (); ++i) {
			const CWElementRow& row = outRows[i];
			if (!row.consumed)
				continue;

			UIndex groupIndex = 0;
			bool found = false;
			for (UIndex g = 0; g < groupIds.GetSize (); ++g) {
				if (groupIds[g] == row.groupId) {
					groupIndex = g;
					found = true;
					break;
				}
			}

			if (!found) {
				int groupNumber = 0;
				const bool numbered = ArticleManager::ParseNumberedGroupValue (row.groupId, groupNumber);

				CWElementRow groupRow;
				groupRow.guid = APINULLGuid;
				groupRow.isGroupRow = true;
				groupRow.isNumberedGroup = numbered;
				groupRow.groupNumber = groupNumber;
				groupRow.groupId = row.groupId;
				groupRow.floorInd = row.floorInd;
				groupRow.storyName = row.storyName;
				groupRow.classItemId = row.classItemId;
				groupRow.classItemName = row.classItemName;

				if (numbered) {
					groupRow.typeName = FR ("Groupe");
					groupRow.elementId = FR ("Groupe ")
						+ GS::ToUniString (std::to_wstring (groupNumber));
					++outReport.numberedGroupCount;
				} else {
					groupRow.typeName = FR ("Ensemble");
					groupRow.elementId = row.groupId;
					++outReport.groupCount;
				}

				groupIds.Push (row.groupId);
				groupRowIndices.Push (outRows.GetSize ());
				outRows.Push (groupRow);
				groupIndex = groupIds.GetSize () - 1;
			}

			outRows[groupRowIndices[groupIndex]].groupMembers.Push (row.guid);
			++outReport.consumedElements;
		}
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
