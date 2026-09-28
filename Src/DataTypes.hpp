#ifndef COSTWAVES_DATA_TYPES_HPP
#define COSTWAVES_DATA_TYPES_HPP

#include "ACAPinc.h"

namespace CostWaves {

// --- Valeurs par défaut ------------------------------------------------------

// Mode de quantification BIM (spec §3) : l'utilisateur calcule soit
// l'ÉLÉMENT, soit ses COMPOSANTS (skins) — jamais les deux à la fois.
// Les dessins 2D forment une catégorie indépendante, toujours incluse.
enum class CWQuantMode {
	Element = 0,		// quantités de l'élément (mur, dalle…)
	Component = 1		// quantités des skins de chaque élément
};

enum class RowKind {
	Element,
	Component,	// composant "properties" (API 25+)
	Skin,		// skin d'une structure composite (API_CompositeQuantity)
	Group,		// ligne "ensemble CostWaves" (facturée comme une seule ligne)
	GroupMember	// élément membre d'un ensemble (consommé, non facturé seul)
};

// --- Quantité unitaire -------------------------------------------------------

struct CWQuantity {
	GS::UniString	label;	// ex. "Volume"
	double			value = 0.0;
	GS::UniString	unit;	// ex. "m3", "m2", "m", "U"

	CWQuantity () = default;
	CWQuantity (const GS::UniString& inLabel, double inValue, const GS::UniString& inUnit)
		: label (inLabel), value (inValue), unit (inUnit) {}
};

// --- Propriété d'un composant (lecture à la demande) --------------------------

struct CWPropertyEntry {
	GS::UniString	name;
	GS::UniString	value;
};

// --- Ligne "composant" --------------------------------------------------------

struct CWComponentRow {
	RowKind					kind = RowKind::Component;
	API_Guid				guid = APINULLGuid;		// GUID du composant (ou APINULLGuid pour un skin)
	GS::UniString			label;					// ex. "Composant 1" ou nom du matériau
	GS::Array<CWQuantity>	quantities;				// volumes / surfaces du skin
	GS::Array<CWPropertyEntry> properties;			// rempli à la demande (détails / export)
	bool					propertiesFetched = false;

	// Phase 5 : classification du MATÉRIAU du skin (l'élément parent peut ne
	// pas avoir de classe — un mur non classé dont les couches ont des
	// matériaux classés est « appelé » via ses skins).
	GS::UniString			classItemId;			// classe du matériau (vide = aucune)
	GS::UniString			classItemName;

	// Phase 3 — enrichissement des skins composites :
	GS::UniString			compositeName;			// nom du composite (vide si inconnu)
	short					skinIndex = -1;			// position dans le composite (0..n-1, -1 si inconnue)
	short					skinCount = 0;			// nombre de couches du composite
	bool					coreSkin = false;		// couche cœur (APICWallComp_Core)
	bool					finishSkin = false;		// couche finition (APICWallComp_Finish)
};

// --- Ligne "élément" ----------------------------------------------------------

struct CWElementRow {
	API_Guid				guid = APINULLGuid;
	API_ElemType			type;
	GS::UniString			typeName;		// nom localisé (ex. "Mur")
	GS::UniString			elementId;		// propriété intégrée "Element ID"
	GS::UniString			layerName;		// calque (filtre métré 2D : CW-METRE-…)
	bool				is2D = false;		// dessin 2D (ligne, polyligne, spline, arc, cercle, hachure)
	short					floorInd = 0;
	GS::UniString			storyName;		// nom de l'étage
	GS::UniString			classItemId;	// ex. "CW-MUR"
	GS::UniString			classItemName;	// ex. "Mur exterieur"
	GS::Array<CWQuantity>		quantities;
	GS::Array<CWComponentRow>	components;	// composants + skins

	// Phase 4/5 — ensembles et groupes CostWaves :
	bool					isGroupRow = false;		// ligne "ensemble"/"groupe" (virtuelle)
	bool					isNumberedGroup = false;	// groupe numéroté (facturé 1 par groupe)
	int						groupNumber = 0;		// numéro du groupe (0 = ensemble ou aucun)
	GS::UniString			groupId;				// CW_Group_ID (vide = aucun)
	bool					consumed = false;		// membre d'un ensemble/groupe (non facturé seul)
	GS::Array<API_Guid>		groupMembers;			// lignes ensemble/groupe : GUIDs des membres
};

// --- Système de classification -------------------------------------------------

struct CWSystemInfo {
	API_Guid		guid = APINULLGuid;
	GS::UniString	name;
	GS::UniString	edition;

	CWSystemInfo () = default;
	CWSystemInfo (const API_Guid& inGuid, const GS::UniString& inName, const GS::UniString& inEdition)
		: guid (inGuid), name (inName), edition (inEdition) {}
};

// --- Article CostWaves (phase 2) -------------------------------------------------

struct CWArticle {
	GS::UniString	id;		// identifiant CostWaves = id de l'item de classification
	GS::UniString	name;	// libellé
	GS::UniString	unit;	// "m2", "m3", "m", "U"... (vide si inconnue)
	GS::UniString	calcQuantity;	// règle de calcul : libellé de la quantité à adopter
								// ("Surface nette", "Surface brute", "Volume conditionné"…).
								// Vide = automatique (première quantité de l'unité).
	short			depth = 0;	// profondeur dans la classification (0 = racine, pour l'indentation)

	CWArticle () = default;
	CWArticle (const GS::UniString& inId, const GS::UniString& inName, const GS::UniString& inUnit)
		: id (inId), name (inName), unit (inUnit) {}
};

// --- Attributs d'un matériau de construction (phase 5) ---------------------------

struct CWMaterialAttributes {
	Int32					connPriority = 500;	// "puissance" : priorité de connexion (1..1000)
	API_AttributeIndex		cutFill = ACAPI_CreateAttributeIndex (1);		// hachure (remplissage en coupe)
	short					cutFillPen = 1;						// stylo avant-plan du remplissage
	short					cutFillBackgroundPen = 1;			// stylo arrière-plan du remplissage
	API_AttributeIndex		cutMaterial = ACAPI_CreateAttributeIndex (1);	// surface de coupe

	CWMaterialAttributes () = default;
};

// --- Récapitulatif par article (phase 4) -----------------------------------------

struct CWArticleSummary {
	GS::UniString	articleId;			// identifiant d'article (= classe)
	GS::UniString	articleName;		// libellé
	GS::UniString	unit;				// unité de facturation ("?" si article inconnu)
	USize					elementCount = 0;	// éléments facturés individuellement
	USize					groupCount = 0;		// ensembles facturés
	USize					numberedGroupCount = 0;	// groupes numérotés facturés (1 par groupe)
	USize					skinCount = 0;		// skins classés facturés (matériau classé)
	double					totalQuantity = 0.0;	// somme des quantités facturées

	CWArticleSummary () = default;
};

// --- Rapport de scan -----------------------------------------------------------

struct CWScanReport {
	USize	scannedElements = 0;		// éléments analysés au total
	USize	classifiedElements = 0;		// éléments portant une classe du système
	USize	componentCount = 0;			// composants (API 25+) sur les éléments classés
	USize	skinCount = 0;				// skins composites sur les éléments classés
	USize	quantityErrors = 0;			// échecs ACAPI_Element_GetQuantities
	USize	classifiedSkins = 0;		// skins dont le matériau porte une classe
	USize	classified2D = 0;		// dessins 2D classés (lignes, hachures…)
	USize					groupCount = 0;				// ensembles CostWaves (phase 4)
	USize					numberedGroupCount = 0;		// groupes numérotés CostWaves (phase 5)
	USize					consumedElements = 0;		// éléments membres d'un ensemble/groupe (consommés)
	GS::UniString	elementIdPropertyNote; // note sur la résolution de la propriété Element ID
};

} // namespace CostWaves

#endif // COSTWAVES_DATA_TYPES_HPP
