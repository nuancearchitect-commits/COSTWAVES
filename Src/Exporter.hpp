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
	static GSErrCode	ExportJSON (const GS::UniString&	systemName,
								   const GS::Array<CWElementRow>&	rows,
								   const CWScanReport&				report,
								   GS::UniString&					outPath,
								   GS::UniString&					outError);

	static GSErrCode	ExportCSV (const GS::UniString&		systemName,
								  const GS::Array<CWElementRow>&	rows,
								  const CWScanReport&				report,
								  GS::UniString&					outPath,
								  GS::UniString&					outError);

private:
	static bool			BuildExportPath (const char* extension, GS::UniString& outPath, GS::UniString& outError);
	static bool			WriteUtf8File (const GS::UniString& path, const GS::UniString& content, bool withBom);
	static GS::UniString	EscapeJson (const GS::UniString& text);
	static GS::UniString	JsonString (const GS::UniString& text);
	static GS::UniString	Timestamp ();
};

} // namespace CostWaves

#endif // COSTWAVES_EXPORTER_HPP
