#ifndef COSTWAVES_MODEL_READER_HPP
#define COSTWAVES_MODEL_READER_HPP

#include "ACAPinc.h"
#include "DataTypes.hpp"

#include <unordered_map>

namespace CostWaves {

// --- Structure composite (phase 3) ---------------------------------------------

// Une couche (skin) d'un attribut composite, telle que définie dans le projet.
struct CWSkinLayer {
	API_AttributeIndex	buildingMaterial;		// matériau de la couche
	double				thickness = 0.0;		// épaisseur en mètres (fillThick)
	bool				core = false;			// couche cœur (APICWallComp_Core)
	bool				finish = false;			// couche finition (APICWallComp_Finish)

	CWSkinLayer () : buildingMaterial (APIInvalidAttributeIndex) {}
};

// Informations d'un attribut composite (voir ModelReader::GetCompositeInfo).
struct CWSkinInfo {
	bool						valid = false;			// attribut lisible
	GS::UniString				name;					// nom du composite
	double						totalThickness = 0.0;	// épaisseur totale en mètres
	std::vector<CWSkinLayer>	layers;					// couches, dans l'ordre du composite

	CWSkinInfo () = default;
};

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
	// groupPropGuid (optionnel) : guid de la propriété CW_Group_ID — si valide,
	// chaque élément groupé est marqué « consommé » et une ligne virtuelle
	// « Ensemble » (ou « Groupe n », valeurs CW-N-…) est ajoutée par groupe.
	static GSErrCode		Scan (const API_Guid&			systemGuid,
							 const API_Guid&			elemIdPropGuid,
							 const API_Guid&			groupPropGuid,
							 const GS::Array<API_Guid>*	elemFilter,
							 GS::Array<CWElementRow>&	outRows,
							 CWScanReport&				outReport);

	// Éléments actuellement sélectionnés dans Archicad (dédupliqués).
	// Err si aucun / base invalide — outGuids reste vide.
	static GSErrCode		GetSelectedElements (GS::Array<API_Guid>& outGuids);

	// Valeurs CW_Group_ID de tous les éléments du projet (paires guid -> valeur,
	// seuls les éléments portant une valeur non vide sont retournés).
	// Sert à numéroter les nouveaux groupes et à éviter de voler les éléments
	// déjà groupés.
	static GSErrCode		CollectGroupValues (const API_Guid& groupPropGuid,
											 GS::Array<GS::Pair<API_Guid, GS::UniString>>& outValues);

	// Propriétés d'un composant (lecture à la demande, pour le panneau de détails).
	static GS::Array<CWPropertyEntry>	GetComponentProperties (const API_ElemComponentID& component);

private:
	// Caches de lecture (phase 3 : un seul appel API par attribut distinct),
	// purgés au début de chaque Scan.
	static std::unordered_map<UInt32, GS::UniString>	typeNameCache;
	static std::unordered_map<UInt32, GS::UniString>	materialNameCache;
	static std::unordered_map<UInt32, CWSkinInfo>		compositeCache;

	static void				ClearCaches ();

	static GS::UniString	GetTypeName (const API_ElemType& type);
	static GS::UniString	GetBuildingMaterialName (API_AttributeIndex index);
	static GS::UniString	GetStoryName (const API_StoryInfo& storyInfo, short floorInd);
	static GS::UniString	GetElementIdValue (const API_Guid& elemGuid, const API_Guid& propGuid);

	// Index de l'attribut composite utilisé par un élément (mur, dallage,
	// toit, coquille). Retourne APIInvalidAttributeIndex si le type n'expose
	// pas de composite ou si la lecture échoue.
	static API_AttributeIndex	GetCompositeIndexOfElement (const API_Guid& elemGuid, API_ElemTypeID typeID);

	// Attribut composite par index : nom, épaisseur totale et couches
	// (matériau, épaisseur, flags cœur/finition). Résultat mis en cache.
	// Retourne false si l'attribut est illisible ou l'index invalide.
	static bool		GetCompositeInfo (API_AttributeIndex compositeIndex, CWSkinInfo& outInfo);

	// Remplit une ligne (quantités + skins enrichis) à partir des quantités
	// lues par ACAPI_Element_GetQuantities ou GetMoreQuantities.
	static void		FillQuantitiesAndSkins (const API_Guid& elemGuid, API_ElemTypeID typeID,
											 const API_ElementQuantity& elementQuantity,
											 const GS::Array<API_CompositeQuantity>& compositeQuantities,
											 CWElementRow& outRow, CWScanReport& outReport);

	static void	ExtractQuantities (API_ElemTypeID typeID, const API_ElementQuantity& quantity,
									GS::Array<CWQuantity>& outQuantities);
	static void	AddQuantity (GS::Array<CWQuantity>& outQuantities, const char* labelUtf8,
							   const char* unitUtf8, double value);
};

} // namespace CostWaves

#endif // COSTWAVES_MODEL_READER_HPP
