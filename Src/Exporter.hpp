#ifndef COSTWAVES_EXPORTER_HPP
#define COSTWAVES_EXPORTER_HPP

#include "ACAPinc.h"
#include "DataTypes.hpp"

namespace CostWaves {

// Export des données lues :
//  - JSON (machine, pour CostWaves)
//  - CSV  (format long, ouvrable dans Excel)
//
// Les fichiers sont écrits à côté du .PLN (ou dans Documents si projet non
// enregistré) sous la forme :
//   <Projet>_CostWaves_<AAAAMMJJ_HHMMSS>.json / .csv
class Exporter {
public:

	// Helpers réutilisés par le module de communication CostWaves (phase 6).
	static bool			WriteUtf8File (const GS::UniString& path, const GS::UniString& content, bool withBom);
	static GS::UniString	JsonString (const GS::UniString& text);
	static GS::UniString	Timestamp ();
	// Dossier du projet (à côté du .PLN) ou dossier Documents si le projet
	// n'est pas enregistré. Nom du projet ("SansTitre" si non enregistré).
	static bool		ResolveProjectLocation (GS::UniString& outFolder, GS::UniString& outProjectName);

	static GSErrCode	ExportJSON (const GS::UniString&				systemName,
								   const GS::Array<CWElementRow>&	rows,
								   const CWScanReport&				report,
								   const GS::Array<CWArticle>&		articles,
								   GS::UniString&					outPath,
								   GS::UniString&					outError);

	static GSErrCode	ExportCSV (const GS::UniString&				systemName,
								  const GS::Array<CWElementRow>&	rows,
								  const CWScanReport&				report,
								  const GS::Array<CWArticle>&		articles,
								  GS::UniString&					outPath,
								  GS::UniString&					outError);

private:
	static bool			BuildExportPath (const char* extension, GS::UniString& outPath, GS::UniString& outError);
	static GS::UniString	EscapeJson (const GS::UniString& text);
};

} // namespace CostWaves

#endif // COSTWAVES_EXPORTER_HPP
