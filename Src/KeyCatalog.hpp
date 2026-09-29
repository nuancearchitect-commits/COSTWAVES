#ifndef COSTWAVES_KEY_CATALOG_HPP
#define COSTWAVES_KEY_CATALOG_HPP

#include "ACAPinc.h"

#include "DataTypes.hpp"

namespace CostWaves {

// Catalogue des « valeurs clés » : paramètre dont la valeur différencie les
// articles d'une même classe (ex. articles qui varient par épaisseur ou par
// dimension). Deux sources :
//  - clés CALCULÉES par COSTWAVES : géométrie de l'élément et de ses
//    couches (épaisseurs, position de la couche…), mesurées sur la
//    maquette au moment du métré ;
//  - PROPRIÉTÉS ARCHICAD du projet (gestionnaire de propriétés) :
//    identifiées par GUID stable — mais seulement celles dont le nom
//    évoque une DIMENSION ou une POSITION (épaisseur, hauteur,
//    profondeur, largeur, longueur, position), pour garder le catalogue
//    court (tous types et groupes confondus).
// Le sélecteur regroupe ensuite ces clés par groupe (filtre popup).
class KeyCatalog {
public:
	// Toutes les clés disponibles (clés calculées d'abord, puis propriétés
	// Archicad). Sans projet ouvert, seules les clés calculées sont listées.
	static void	CollectAvailableKeys (GS::Array<CWKeyEntry>& outKeys);
};

} // namespace CostWaves

#endif // COSTWAVES_KEY_CATALOG_HPP
