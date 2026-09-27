#ifndef COSTWAVES_MODEL_READER_HPP
#define COSTWAVES_MODEL_READER_HPP

#include "ACAPinc.h"
#include "DataTypes.hpp"

namespace CostWaves {

// Lecture des données du modèle Archicad :
//  - systèmes de classification
//  - éléments portant une classe d'un système donné
//  - propriété intégrée "Element ID"
//  - quantités par type d'élément (ACAPI_Element_GetQuantities)
//  - skins composites (matériaux + volumes/surfaces)
//  - composants d'éléments (API 25+) et leurs propriétés (à la demande)
class ModelReader {
public:
	// Retourne tous les systèmes de classification du projet.
	static GS::Array<CWSystemInfo>	GetClassificationSystems ();

	// Résout la propriété intégrée "Element ID" :
	// 1) GUID connu (B1B54D45-C951-42C9-9AF8-898F0BF212AB)
	// 2) repli : recherche par nom parmi toutes les définitions
	static bool						ResolveElementIdPropertyGuid (API_Guid& outGuid, GS::UniString& outNote);

	// Scanne le projet : tous les éléments, filtrés sur ceux portant une classe
	// du système systemGuid. Renvoie le code d'erreur global (APIERR_NOPLAN etc.).
	// elemFilter (optionnel) : si non nul et non vide, seuls ces éléments sont
	// analysés (utilisé pour la lecture de la sélection courante).
	static GSErrCode		Scan (const API_Guid&			systemGuid,
								 const API_Guid&			elemIdPropGuid,
								 const GS::Array<API_Guid>*	elemFilter,
								 GS::Array<CWElementRow>&	outRows,
								 CWScanReport&				outReport);

	// Éléments actuellement sélectionnés dans Archicad (dédupliqués).
	// Err si aucun / base invalide — outGuids reste vide.
	static GSErrCode		GetSelectedElements (GS::Array<API_Guid>& outGuids);

	// Propriétés d'un composant (lecture à la demande, pour le panneau de détails).
	static GS::Array<CWPropertyEntry>	GetComponentProperties (const API_ElemComponentID& component);

private:
	static GS::UniString	GetTypeName (const API_ElemType& type);
	static GS::UniString	GetBuildingMaterialName (API_AttributeIndex index);
	static GS::UniString	GetStoryName (const API_StoryInfo& storyInfo, short floorInd);
	static GS::UniString	GetElementIdValue (const API_Guid& elemGuid, const API_Guid& propGuid);

	static void	ExtractQuantities (API_ElemTypeID typeID, const API_ElementQuantity& quantity,
									GS::Array<CWQuantity>& outQuantities);
	static void	AddQuantity (GS::Array<CWQuantity>& outQuantities, const char* labelUtf8,
							   const char* unitUtf8, double value);
};

} // namespace CostWaves

#endif // COSTWAVES_MODEL_READER_HPP
