#include "CostWavesPrecompiledHeader.hpp"

#include "KeyCatalog.hpp"

namespace CostWaves {

namespace {

GS::UniString FR (const char* utf8Text)
{
	return GS::UniString (utf8Text, CC_UTF8);
}

} // namespace


void KeyCatalog::CollectAvailableKeys (GS::Array<CWKeyEntry>& outKeys)
{
	outKeys.Clear ();

	CWKeyEntry key;

	// --- Groupe « Composant » : la structure porteuse ------------------------------
	// Géométrie de l'élément / du composite, mesurée sur la maquette au
	// moment du métré.
	key.group = FR ("Composant");

	key.id = FR ("element.thickness");
	key.name = FR ("Épaisseur de l'élément");
	outKeys.Push (key);

	key.id = FR ("structure.totalThickness");
	key.name = FR ("Épaisseur totale du composite");
	outKeys.Push (key);

	// --- Groupe « Couche » : la couche (skin) du matériau ---------------------------
	key.group = FR ("Couche");

	key.id = FR ("skin.thickness");
	key.name = FR ("Épaisseur de la couche du matériau");
	outKeys.Push (key);

	key.id = FR ("skin.index");
	key.name = FR ("Position de la couche (1, 2, 3…)");
	outKeys.Push (key);

	key.id = FR ("skin.count");
	key.name = FR ("Nombre de couches de l'élément");
	outKeys.Push (key);
}

} // namespace CostWaves
