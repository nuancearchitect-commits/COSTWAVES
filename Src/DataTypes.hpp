#ifndef COSTWAVES_DATA_TYPES_HPP
#define COSTWAVES_DATA_TYPES_HPP

#include "ACAPinc.h"

namespace CostWaves {

// --- Valeurs par défaut ------------------------------------------------------

enum class RowKind {
	Element,
	Component,	// composant "properties" (API 25+)
	Skin		// skin d'une structure composite (API_CompositeQuantity)
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
};

// --- Ligne "élément" ----------------------------------------------------------

struct CWElementRow {
	API_Guid					guid = APINULLGuid;
	API_ElemType				type;
	GS::UniString				typeName;		// nom localisé (ex. "Mur")
	GS::UniString				elementId;		// propriété intégrée "Element ID"
	short						floorInd = 0;
	GS::UniString				storyName;		// nom de l'étage
	GS::UniString				classItemId;	// ex. "CW-MUR"
	GS::UniString				classItemName;	// ex. "Mur exterieur"
	GS::Array<CWQuantity>		quantities;
	GS::Array<CWComponentRow>	components;		// composants + skins
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

// --- Rapport de scan -----------------------------------------------------------

struct CWScanReport {
	USize	scannedElements = 0;		// éléments analysés au total
	USize	classifiedElements = 0;		// éléments portant une classe du système
	USize	componentCount = 0;			// composants (API 25+) sur les éléments classés
	USize	skinCount = 0;				// skins composites sur les éléments classés
	USize	quantityErrors = 0;			// échecs ACAPI_Element_GetQuantities
	GS::UniString	elementIdPropertyNote; // note sur la résolution de la propriété Element ID
};

} // namespace CostWaves

#endif // COSTWAVES_DATA_TYPES_HPP
