#include "CostWavesPrecompiledHeader.hpp"

#include "KeyCatalog.hpp"

#include "ModelReader.hpp"

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

void KeyCatalog::CollectFormulaVariables (const GS::UniString& objectName,
										  GS::Array<CWKeyEntry>& outVariables,
										  GS::UniString& outNote)
{
	outVariables.Clear ();
	outNote.Clear ();

	// 1) Paramètres GDL de type LONGUEUR de l'objet (nom GDL stable = texte
	//    inséré dans la formule, valeur en mètres dans l'occurrence).
	if (!objectName.IsEmpty ()) {
		GS::Array<CWGdlParam> params;
		GS::UniString note;
		GS::UniString alert;
		if (ModelReader::GetLibraryPartParameters (objectName, params, note, alert)) {
			for (UIndex p = 0; p < params.GetSize (); ++p) {
				if (params[p].type != FR ("longueur"))
					continue;
				CWKeyEntry entry;
				entry.id = params[p].name;
				entry.group = FR ("Paramètre GDL (m)");
				entry.name = params[p].label.IsEmpty () ? params[p].name : params[p].label;
				outVariables.Push (entry);
			}
			if (alert.IsEmpty () == false)
				outNote = alert;
		} else {
			outNote = FR ("Paramètres GDL de « ") + objectName + FR (" » illisibles : ")
					  + note;
		}
	}

	// 2) Quantités Archicad de l'élément posé (libellés exacts des
	//    quantités lues par COSTWAVES — variables de la formule).
	struct StaticQuantity {
		const char* id;
		const char* group;
	};
	static const StaticQuantity kElementQuantities[] = {
		// Fenêtre / porte.
		{ "Largeur",					"Fenêtre / Porte" },
		{ "Hauteur",					"Fenêtre / Porte" },
		{ "Surface",					"Fenêtre / Porte" },
		{ "Surface brute",				"Fenêtre / Porte" },
		{ "Volume",						"Fenêtre / Porte" },
		{ "Hauteur appui",				"Fenêtre / Porte" },
		// Mur hôte d'une fenêtre / porte.
		{ "Épaisseur mur hôte",			"Mur hôte (fenêtre/porte)" },
		{ "Contour ouverture",			"Mur hôte (fenêtre/porte)" },
		{ "Surface tableau",			"Mur hôte (fenêtre/porte)" },
		// Objet / lampe.
		{ "Largeur A",					"Objet / Lampe" },
		{ "Profondeur B",				"Objet / Lampe" },
		{ "Hauteur ZZYZX",				"Objet / Lampe" }
	};
	const USize kQuantityCount = sizeof (kElementQuantities) / sizeof (kElementQuantities[0]);
	for (USize q = 0; q < kQuantityCount; ++q) {
		// Pas de doublon avec un paramètre GDL de même nom.
		bool duplicate = false;
		for (UIndex v = 0; v < outVariables.GetSize (); ++v) {
			if (outVariables[v].id == FR (kElementQuantities[q].id)) {
				duplicate = true;
				break;
			}
		}
		if (duplicate)
			continue;
		CWKeyEntry entry;
		entry.id = FR (kElementQuantities[q].id);
		entry.group = FR (kElementQuantities[q].group);
		entry.name = entry.id;
		outVariables.Push (entry);
	}
}

} // namespace CostWaves
