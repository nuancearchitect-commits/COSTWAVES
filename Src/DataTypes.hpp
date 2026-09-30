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

// Types de structures natives Archicad servant de clé aux règles de
// correspondance (nouvelle architecture, spec §4–§6).
enum class CWStructureType {
	BuildingMaterial,
	Composite,
	Profile,
	Favorite,
	LibraryPart,
	LibraryPartBool		// article HÉRITÉ : objet GDL + paramètre booléen
						// activé (tablette, seuil, volet…) ; clé « bool:<nom> »
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

	// Nouvelle architecture : article issu de la règle du MATÉRIAU
	// (bibliothèque de correspondances) — prioritaire sur la classification.
	GS::UniString			ruleArticleId;			// article CostWaves de la règle (vide = aucune)
	GS::UniString			ruleQuantity;			// quantité à adopter (vide = automatique)
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

	// Nouvelle architecture — correspondance par règle (spec §4–§7) :
	// la structure native de l'élément (composite, objet de bibliothèque…)
	// est recherchée dans la bibliothèque de règles ; l'article et le niveau
	// de métré de la règle sont PRIORITAIRES sur la classification.
	CWStructureType			structureType = CWStructureType::Composite;
	GS::UniString			structureName;			// ex. "MUR_EXT_30" ou "Fenêtre PVC 120"
	GS::UniString			ruleArticleId;			// article de la règle (vide = aucune règle)
	CWQuantMode				ruleMode = CWQuantMode::Element;
	GS::UniString			ruleQuantity;			// quantité à adopter (vide = automatique)
	bool					hasRule = false;		// une règle s'applique à cette ligne
	bool					ruleIgnored = false;	// structure « Ignorer » (exclue du métré)

	// Phase 4/5 — ensembles et groupes CostWaves :
	bool					isGroupRow = false;		// ligne "ensemble"/"groupe" (virtuelle)
	bool					isNumberedGroup = false;	// groupe numéroté (facturé 1 par groupe)
	int						groupNumber = 0;		// numéro du groupe (0 = ensemble ou aucun)
	GS::UniString			groupId;				// CW_Group_ID (vide = aucun)
	bool					consumed = false;		// membre d'un ensemble/groupe (non facturé seul)
	GS::Array<API_Guid>		groupMembers;			// lignes ensemble/groupe : GUIDs des membres
};

// Article effectif d'une ligne : celui de la RÈGLE de correspondance si
// présente (nouvelle architecture), sinon la classe de classification.
inline const GS::UniString&	RowArticleId (const CWElementRow& row)
{
	return row.ruleArticleId.IsEmpty () ? row.classItemId : row.ruleArticleId;
}

inline const GS::UniString&	ComponentArticleId (const CWComponentRow& component)
{
	return component.ruleArticleId.IsEmpty () ? component.classItemId : component.ruleArticleId;
}

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

// --- Règles de correspondance (nouvelle architecture) ----------------------------
// Une règle relie une structure native Archicad à un article CostWaves :
//  - COMPOSITE ou PROFIL : mode Element = « lui-même » (1 article, quantités
//    de l'élément) ; mode Component = « ses couches » (article vide — le métré
//    se fait sur les quantités des MATÉRIAUX, qui ont chacun leur règle) ;
//  - OBJET de bibliothèque (.gsm posable) : article, mode Element ;
//  - MATÉRIAU : article d'une couche (mode Component).
// « ignored » exclut la structure du métré. La règle ne contient JAMAIS la
// quantité réelle : elle est toujours calculée depuis la maquette.
// Bibliothèque JSON indépendante des projets (<Documents>/CostWaves-regles.json).

struct CWMapRule {
	CWStructureType	structureType = CWStructureType::Composite;
	GS::UniString	structureName;			// ex. "MUR_EXT_30", "BETON_25", "LUM_LED_01"
	GS::UniString	articleId;				// identifiant unique CostWaves (ex. "CW-001")
	CWQuantMode		mode = CWQuantMode::Element;	// élément ou composant (spec §11)
	GS::UniString	quantity;		// quantité à adopter ("Surface nette"…), vide = automatique

	// « Valeur clé » : paramètre dont la valeur différencie les articles
	// d'une même classe (ex. épaisseur — le même matériau donne BETON 15 cm
	// et BETON 25 cm). Vide = l'article ne diffère pas par un paramètre.
	GS::UniString	keyId;			// "element.thickness", "skin.thickness"… ou "property:<guid>"
	GS::UniString	keyName;		// libellé d'affichage de la clé (ex. "Épaisseur de la couche")

	// Valeur clé SECONDAIRE (dimensionnelle) des articles hérités : le
	// paramètre booléen (keyId « bool:<nom> ») fait naître l'article, cette
	// clé GDL de type longueur différencie ses variantes (Ø125/Ø160…).
	GS::UniString	valueKeyId;		// nom GDL stable (vide = aucune)
	GS::UniString	valueKeyName;	// libellé d'affichage
	bool		ignored = false;		// structure exclue du métré

	CWMapRule () = default;
};

// --- « Valeur clé » disponible (entrée de catalogue) ------------------------------

// Une clé du catalogue proposé dans la colonne « Valeur clé » : clés calculées
// par COSTWAVES (géométrie) ou propriété Archicad du projet.
struct CWKeyEntry {
	GS::UniString	id;		// "element.thickness", "skin.thickness", … ou "property:<guid>"
	GS::UniString	group;	// groupe d'affichage (ex. "Couche", "Général")
	GS::UniString	name;	// libellé (ex. "Épaisseur de la couche du matériau")

	CWKeyEntry () = default;
};

// --- Paramètre GDL d'un objet de bibliothèque -------------------------------------

// Un paramètre GDL proposé comme valeur clé ou déclencheur : libellé
// lisible, nom GDL stable (identifiant de la règle) et type GDL
// (« longueur », « bool », « entier »…) affiché dans la colonne Type du
// sélecteur — double vérification visuelle par l'utilisateur.
struct CWGdlParam {
	GS::UniString	label;	// libellé lisible (ex. « Largeur »)
	GS::UniString	name;	// nom GDL stable (ex. « A »)
	GS::UniString	type;	// type GDL (ex. « longueur », « bool »)

	CWGdlParam () = default;
};

struct CWArticle {
	GS::UniString	id;		// identifiant CostWaves = id de l'item de classification
	GS::UniString	name;	// libellé
	GS::UniString	unit;	// "m2", "m3", "m", "U"... (vide si inconnue)
	GS::UniString	calcQuantity;	// règle de calcul : libellé de la quantité à adopter
								// ("Surface nette", "Surface brute", "Volume conditionné"…).
								// Vide = automatique (première quantité de l'unité).
	GS::UniString	calcFormula;	// formule dérivée (prioritaire sur calcQuantity) :
								// expression arithmétique sur les libellés de quantités
								// de la ligne, ex. "Contour ouverture * Épaisseur mur hôte"
								// (enduit latéral des tableaux).
	GS::UniString	chapter;		// chapitre / sous-chapitre de la base CostWaves
								// (ex. "02 Murs / 02.1 Maçonnerie"), vide si inconnu.
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
	USize	unmappedStructures = 0;	// structures natives sans règle (⚠ à configurer)
	USize					groupCount = 0;				// ensembles CostWaves (phase 4)
	USize					numberedGroupCount = 0;		// groupes numérotés CostWaves (phase 5)
	USize					consumedElements = 0;		// éléments membres d'un ensemble/groupe (consommés)
	GS::UniString	elementIdPropertyNote; // note sur la résolution de la propriété Element ID
};

} // namespace CostWaves

#endif // COSTWAVES_DATA_TYPES_HPP
