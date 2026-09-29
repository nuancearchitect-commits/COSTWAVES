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

	// --- Clés calculées par COSTWAVES (géométrie) --------------------------------
	// Mesurées sur la maquette au moment du métré : elles ne dépendent
	// d'aucune propriété projetée. « skin.* » s'applique au matériau de la
	// couche (mode Matériau) ; les autres à la structure porteuse.
	{
		CWKeyEntry key;

		key.id = FR ("element.thickness");
		key.group = FR ("Élément");
		key.name = FR ("Épaisseur de l'élément");
		outKeys.Push (key);

		key.id = FR ("skin.thickness");
		key.group = FR ("Couche");
		key.name = FR ("Épaisseur de la couche du matériau");
		outKeys.Push (key);

		key.id = FR ("skin.index");
		key.group = FR ("Couche");
		key.name = FR ("Position de la couche (1, 2, 3…)");
		outKeys.Push (key);

		key.id = FR ("skin.count");
		key.group = FR ("Couche");
		key.name = FR ("Nombre de couches de l'élément");
		outKeys.Push (key);

		key.id = FR ("structure.totalThickness");
		key.group = FR ("Composite / profil");
		key.name = FR ("Épaisseur totale du composite");
		outKeys.Push (key);
	}

	// --- Propriétés Archicad du projet (intégrées + personnalisées) ---------------
	// Les définitions sont identifiées par leur GUID : un changement de
	// libellé dans le gestionnaire de propriétés ne casse pas la règle.
	GS::Array<API_PropertyGroup> groups;
	if (ACAPI_Property_GetPropertyGroups (groups) != NoError)
		return;

	for (UIndex g = 0; g < groups.GetSize (); ++g) {
		// Les groupes intégrés peuvent avoir un nom vide (localisé par
		// Archicad) : libellé de repli.
		const GS::UniString groupName = groups[g].name.IsEmpty () ? FR ("Archicad") : groups[g].name;

		GS::Array<API_PropertyDefinition> definitions;
		if (ACAPI_Property_GetPropertyDefinitions (groups[g].guid, definitions) != NoError)
			continue;

		for (UIndex d = 0; d < definitions.GetSize (); ++d) {
			if (definitions[d].name.IsEmpty ())
				continue;

			CWKeyEntry key;
			key.id = FR ("property:") + APIGuidToString (definitions[d].guid);
			key.group = groupName;
			key.name = definitions[d].name;
			outKeys.Push (key);
		}
	}
}

} // namespace CostWaves
