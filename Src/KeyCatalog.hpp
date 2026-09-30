#ifndef COSTWAVES_KEY_CATALOG_HPP
#define COSTWAVES_KEY_CATALOG_HPP

#include "ACAPinc.h"

#include "DataTypes.hpp"

namespace CostWaves {

// Catalogue des « valeurs clés » : paramètre dont la valeur différencie les
// articles d'une même classe (ex. articles qui varient par épaisseur ou par
// dimension). Seuls DEUX groupes, tous deux calculés par COSTWAVES sur la
// maquette au moment du métré :
//  - « Composant » : la structure porteuse (épaisseur de l'élément,
//    épaisseur totale du composite) ;
//  - « Couche » : la couche (skin) du matériau (épaisseur, position,
//    nombre de couches).
// Les propriétés Archicad ne sont plus proposées ici (liste trop grande) ;
// les objets GDL ont leur propre correspondance, avec leurs paramètres GDL.
class KeyCatalog {
public:
	// Toutes les clés disponibles (clés calculées par COSTWAVES uniquement).
	static void	CollectAvailableKeys (GS::Array<CWKeyEntry>& outKeys);

	// Variables d'une FORMULE DE QUANTITÉ (objets GDL, articles hérités) :
	// paramètres GDL de type longueur de l'objet (nom GDL stable = texte
	// inséré, valeur en mètres) + quantités Archicad de l'élément — une
	// fenêtre/porte porte celles de son MUR HÔTE (Épaisseur mur hôte,
	// Contour ouverture, Surface tableau). outNote explique toute limite
	// (paramètres illisibles…).
	static void	CollectFormulaVariables (const GS::UniString& objectName,
										  GS::Array<CWKeyEntry>& outVariables,
										  GS::UniString& outNote);
};

} // namespace CostWaves

#endif // COSTWAVES_KEY_CATALOG_HPP
