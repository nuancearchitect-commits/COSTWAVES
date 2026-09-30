#ifndef COSTWAVES_RULE_LIBRARY_HPP
#define COSTWAVES_RULE_LIBRARY_HPP

#include "ACAPinc.h"
#include "DGModule.hpp"

#include "DataTypes.hpp"

namespace CostWaves {

// Bibliothèque de règles de correspondance (nouvelle architecture, spec §4–§8) :
//  - relie les structures natives Archicad (matériaux, composites, profils,
//    favoris, objets de bibliothèque) aux articles CostWaves ;
//  - stockée dans un JSON INDEPENDANT des projets (dossier Documents de
//    l'utilisateur), donc réutilisable entre projets et éditable sans
//    maquette ouverte ;
//  - les règles référencent l'IDENTIFIANT UNIQUE de l'article (pas son nom) :
//    un changement de désignation dans la base ne casse pas la règle (spec §9).

class RuleLibrary {
public:
	// Chemin du fichier : <Documents>/CostWaves-regles.json.
	static GS::UniString	RulesFilePath ();

	// Chargement (fichier absent = bibliothèque vide, sans erreur) et
	// enregistrement de toutes les règles.
	static bool		LoadRules (GS::Array<CWMapRule>& outRules, GS::UniString& outError);
	static bool		SaveRules (const GS::Array<CWMapRule>& rules, GS::UniString& outError);

	// Recherche d'une règle par type + nom de structure (nullptr si absente).
	static const CWMapRule*	FindRule (const GS::Array<CWMapRule>& rules,
									 CWStructureType structureType, const GS::UniString& structureName);

	// Libellés des types de structures (interface FR + clés JSON stables).
	static GS::UniString	StructureTypeName (CWStructureType structureType);
	static bool				StructureTypeFromName (const GS::UniString& name, CWStructureType& outType);

	// Structures disponibles dans l'environnement Archicad courant (pour
	// aider la saisie ; fonctionne mieux avec un projet ouvert) :
	//  - building materials, composites, profils (attributs du projet) ;
	//  - objets de bibliothèque chargés.
	static void		CollectAvailableStructures (CWStructureType structureType,
												 GS::Array<GS::UniString>& outNames);
};

} // namespace CostWaves

#endif // COSTWAVES_RULE_LIBRARY_HPP
