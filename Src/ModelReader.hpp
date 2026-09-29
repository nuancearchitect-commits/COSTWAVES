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
	// Dessin 2D ? (ligne, polyligne, spline, arc, cercle, hachure — spec §2)
	static bool		Is2DType (API_ElemTypeID typeID);

	// Nom du calque d'un élément (cache) — filtre métré 2D (CW-METRE-…).
	static GS::UniString	GetLayerName (API_AttributeIndex layerIndex);

	// Nom de l'objet de bibliothèque chargé d'index donné (vide si introuvable).
	static GS::UniString	GetLibraryPartName (Int32 libInd);

	// Quantités géométriques des dessins 2D (§5/§6) : longueur des lignes,
	// polylignes (arcs compris), arcs ; rayon/diamètre/circonférence/surface
	// des cercles. Les hachures passent par le pipeline des quantités.
	static void		Extract2DQuantities (const API_Guid& elemGuid, API_ElemTypeID typeID,
									 GS::Array<CWQuantity>& outQuantities);

	static GSErrCode		Scan (const API_Guid&			systemGuid,
							 const API_Guid&			elemIdPropGuid,
							 const API_Guid&			groupPropGuid,
							 bool					include2D,
							 const GS::Array<CWMapRule>&	rules,
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

	// --- Nouvelle architecture : détection des structures ------------------------

	// Structure native d'un élément placé : composite / profil / objet de
	// bibliothèque / matériau de base (mur ou dalle sans composite, poteau ou
	// poutre sans profil). Retourne false si la structure est illisible.
	static bool	GetElementStructure (const API_Guid& elemGuid, API_ElemTypeID typeID,
									  CWStructureType& outType, GS::UniString& outName);

	// Matériaux des couches d'un composite, par nom d'attribut (mode
	// « ses couches » du gestionnaire de correspondances). Retourne false si
	// le composite est introuvable. Les matériaux d'un PROFIL ne sont pas
	// lisibles dans l'attribut : ils se découvrent depuis les éléments posés
	// (CollectSkinMaterialNames).
	static bool	GetStructureLayerMaterialNames (const GS::UniString& compositeName,
												 GS::Array<GS::UniString>& outNames);

	// Noms des matériaux des couches d'éléments donnés (lecture des quantités
	// composants) : matériaux réellement posés — sert notamment aux profils.
	static void	CollectSkinMaterialNames (const GS::Array<API_Guid>& elemGuids,
										   GS::Array<GS::UniString>& outNames);

	// Nom d'un matériau de construction par index (avec cache).
	static GS::UniString	GetBuildingMaterialName (API_AttributeIndex index);

	// Paramètres GDL d'un objet de bibliothèque, par nom d'attribut
	// (valeurs clés de la correspondance objets GDL). Seules les
	// variables de TYPE LONGUEUR sont retournées (épaisseur, hauteur,
	// dimensions…), hors tableaux et paramètres cachés. Paires
	// (libellé lisible, nom GDL stable). outNote explique un échec ou
	// une liste vide (diagnostic affiché à l'utilisateur).
	static bool	GetLibraryPartParameters (const GS::UniString& libPartName,
										 GS::Array<GS::Pair<GS::UniString, GS::UniString>>& outParams,
										 GS::UniString& outNote);

private:
	// Caches de lecture (phase 3 : un seul appel API par attribut distinct),
	// purgés au début de chaque Scan.
	static std::unordered_map<UInt32, GS::UniString>	typeNameCache;
	static std::unordered_map<UInt32, GS::UniString>	materialNameCache;
	static std::unordered_map<UInt32, GS::UniString>	layerNameCache;
	static std::unordered_map<UInt32, GS::UniString>	libPartNameCache;
	static std::unordered_map<UInt32, CWSkinInfo>		compositeCache;
	// Classification des matériaux dans le système scanné (phase 5) :
	// index du matériau -> (id de classe, nom) ; id vide = sans classe.
	static std::unordered_map<UInt32, GS::Pair<GS::UniString, GS::UniString>>	materialClassCache;

	static void				ClearCaches ();

	static GS::UniString	GetTypeName (const API_ElemType& type);
	static GS::UniString	GetStoryName (const API_StoryInfo& storyInfo, short floorInd);
	static GS::UniString	GetElementIdValue (const API_Guid& elemGuid, const API_Guid& propGuid);

	// Classe du matériau de construction dans le système donné (résultat mis
	// en cache ; retourne false si le matériau n'est pas classé).
	static bool	GetMaterialClassification (API_AttributeIndex materialIndex, const API_Guid& systemGuid,
										 GS::UniString& outItemId, GS::UniString& outItemName);

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
										 const API_Guid& systemGuid,
										   const GS::Array<CWMapRule>&	rules,
											 const API_ElementQuantity& elementQuantity,
											 const GS::Array<API_CompositeQuantity>& compositeQuantities,
											 CWElementRow& outRow, CWScanReport& outReport);

	static void	ExtractQuantities (const API_Guid& elemGuid, API_ElemTypeID typeID,
						 const API_ElementQuantity& quantity,
									GS::Array<CWQuantity>& outQuantities);
	static void	AddQuantity (GS::Array<CWQuantity>& outQuantities, const char* labelUtf8,
							   const char* unitUtf8, double value);
};

} // namespace CostWaves

#endif // COSTWAVES_MODEL_READER_HPP
