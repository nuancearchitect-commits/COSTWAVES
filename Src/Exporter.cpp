#include "CostWavesPrecompiledHeader.hpp"

#include "Exporter.hpp"
#include "ModelReader.hpp"
#include "ArticleManager.hpp"

#include "UniStringWStringConversion.hpp"

#include <cwchar>

namespace CostWaves {

namespace {

GS::UniString FR (const char* utf8Text)
{
	return GS::UniString (utf8Text, CC_UTF8);
}

// Littéral -> GS::UniString (départ de chaîne pour l'opérateur +).
GS::UniString US (const char* utf8Text)
{
	return GS::UniString (utf8Text, CC_UTF8);
}

// Article correspondant à l'identifiant de classe (nullptr si aucun).
const CWArticle* FindArticle (const GS::Array<CWArticle>& articles, const GS::UniString& articleId)
{
	if (articleId.IsEmpty ())
		return nullptr;

	for (UIndex i = 0; i < articles.GetSize (); ++i) {
		if (articles[i].id == articleId)
			return &articles[i];
	}

	return nullptr;
}

// Ligne d'un élément par GUID (nullptr si absente) — phase 4.
const CWElementRow* FindRowByGuid (const GS::Array<CWElementRow>& rows, const API_Guid& guid)
{
	for (UIndex i = 0; i < rows.GetSize (); ++i) {
		if (rows[i].guid == guid)
			return &rows[i];
	}
	return nullptr;
}

// "0.1234" (séparateur point, zéros finaux retirés) — pour JSON.
GS::UniString FormatDouble (double value)
{
	wchar_t buffer[64];
	swprintf (buffer, 64, L"%.4f", value);

	std::wstring text (buffer);
	while (text.length () > 1 && text.back () == L'0')
		text.pop_back ();
	if (text.length () > 1 && text.back () == L'.')
		text.pop_back ();

	return GS::ToUniString (text);
}

} // namespace


bool Exporter::ResolveProjectLocation (GS::UniString& outFolder, GS::UniString& outProjectName)
{
	outFolder.Clear ();
	outProjectName = FR ("SansTitre");

	// Dossier + nom du projet enregistré.
	API_ProjectInfo projectInfo;
	BNZeroMemory (&projectInfo, sizeof (projectInfo));

	if (ACAPI_ProjectOperation_Project (&projectInfo) == NoError) {
		if (!projectInfo.untitled && projectInfo.location != nullptr) {
			GS::UniString fullPath;
			if (projectInfo.location->ToPath (&fullPath) == NoError) {
				std::wstring pathW = GS::ToWString (fullPath);
				for (wchar_t& ch : pathW) {
					if (ch == L'\\')
						ch = L'/';
				}

				const std::size_t slash = pathW.find_last_of (L'/');
				if (slash != std::wstring::npos) {
					const std::wstring fileBase = pathW.substr (slash + 1);
					const std::size_t dot = fileBase.find_last_of (L'.');
					outProjectName = GS::ToUniString (dot != std::wstring::npos
						? fileBase.substr (0, dot)
						: fileBase);
					outFolder = GS::ToUniString (pathW.substr (0, slash));
				}
			}
		}
	}

	// Repli : dossier Documents si le projet n'est pas enregistré.
	if (outFolder.IsEmpty ()) {
		API_SpecFolderID specFolder = API_UserDocumentsFolderID;
		IO::Location documentsLocation;
		if (ACAPI_ProjectSettings_GetSpecFolder (&specFolder, &documentsLocation) == NoError) {
			GS::UniString documentsPath;
			if (documentsLocation.ToPath (&documentsPath) == NoError && !documentsPath.IsEmpty ()) {
				outFolder = documentsPath;
			}
		}
	}

	return !outFolder.IsEmpty ();
}


bool Exporter::BuildExportPath (const char* extension, GS::UniString& outPath, GS::UniString& outError)
{
	GS::UniString folder;
	GS::UniString projectName;
	if (!ResolveProjectLocation (folder, projectName)) {
		outError = FR ("Impossible de déterminer le dossier d'export (enregistrez le projet puis réessayez).");
		return false;
	}

	outPath = folder + "/" + projectName + "_CostWaves_" + Timestamp () + "." + US (extension);
	return true;
}


bool Exporter::WriteUtf8File (const GS::UniString& path, const GS::UniString& content, bool withBom)
{
#if defined (WINDOWS)
	// Chemin Unicode natif Windows : UniString -> wstring -> _wfopen.
	const std::wstring pathW = GS::ToWString (path);
	FILE* file = _wfopen (pathW.c_str (), L"wb");
#else
	const auto pathCStr = path.ToCStr (CC_Default);
	FILE* file = fopen (pathCStr.Get (), "wb");
#endif
	if (file == nullptr)
		return false;

	if (withBom) {
		const unsigned char bom[3] = { 0xEF, 0xBB, 0xBF };
		fwrite (bom, 1, 3, file);
	}

	const auto contentCStr = content.ToCStr (CC_UTF8);
	fwrite (contentCStr.Get (), 1, strlen (contentCStr.Get ()), file);

	fclose (file);
	return true;
}


GS::UniString Exporter::Timestamp ()
{
	const std::time_t now = std::time (nullptr);
	std::tm localTime;
#if defined (WINDOWS)
	localtime_s (&localTime, &now);
#else
	localtime_r (&now, &localTime);
#endif

	wchar_t buffer[32];
	swprintf (buffer, 32, L"%04d%02d%02d_%02d%02d%02d",
			  localTime.tm_year + 1900, localTime.tm_mon + 1, localTime.tm_mday,
			  localTime.tm_hour, localTime.tm_min, localTime.tm_sec);

	return GS::ToUniString (std::wstring (buffer));
}


GS::UniString Exporter::EscapeJson (const GS::UniString& text)
{
	std::wstring source = GS::ToWString (text);
	std::wstring escaped;
	escaped.reserve (source.length () + 8);

	for (const wchar_t ch : source) {
		switch (ch) {
			case L'"':	escaped += L"\\\"";	break;
			case L'\\':	escaped += L"\\\\";	break;
			case L'\n':	escaped += L"\\n";		break;
			case L'\r':	escaped += L"\\r";		break;
			case L'\t':	escaped += L"\\t";		break;
			default:
				if (ch < 0x20) {
					wchar_t code[8];
					swprintf (code, 8, L"\\u%04x", static_cast<unsigned int> (ch));
					escaped += code;
				} else {
					escaped += ch;
				}
				break;
		}
	}

	return GS::ToUniString (escaped);
}


GS::UniString Exporter::JsonString (const GS::UniString& text)
{
	return US ("\"") + EscapeJson (text) + "\"";
}


GSErrCode Exporter::ExportJSON (const GS::UniString& systemName, const GS::Array<CWElementRow>& rows,
								const CWScanReport& report, const GS::Array<CWArticle>& articles,
								GS::UniString& outPath, GS::UniString& outError)
{
	if (!BuildExportPath ("json", outPath, outError))
		return APIERR_GENERAL;

	GS::UniString json;
	json += "{\n";
	json += "  \"tool\": \"CostWaves Add-On\",\n";
	json += "  \"formatVersion\": 1,\n";
	json += US ("  \"exportedAt\": \"") + Timestamp () + "\",\n";
	json += US ("  \"classificationSystem\": ") + JsonString (systemName) + ",\n";
	json += "  \"counts\": {\n";
	json += US ("    \"scannedElements\": ") + GS::ToUniString (std::to_wstring (static_cast<int> (report.scannedElements))) + ",\n";
	json += US ("    \"classifiedElements\": ") + GS::ToUniString (std::to_wstring (static_cast<int> (report.classifiedElements))) + ",\n";
	json += US ("    \"components\": ") + GS::ToUniString (std::to_wstring (static_cast<int> (report.componentCount))) + ",\n";
	json += US ("    \"skins\": ") + GS::ToUniString (std::to_wstring (static_cast<int> (report.skinCount))) + ",\n";
	json += US ("    \"groups\": ") + GS::ToUniString (std::to_wstring (static_cast<int> (report.groupCount))) + ",\n";
	json += US ("    \"consumedElements\": ") + GS::ToUniString (std::to_wstring (static_cast<int> (report.consumedElements))) + "\n";
	json += "  },\n";

	// Récapitulatif par article (phase 4) : totaux facturés.
	{
		GS::Array<CWArticleSummary> summary;
		ArticleManager::BuildArticleSummary (rows, articles, summary);

		json += "  \"summary\": [\n";
		for (UIndex s = 0; s < summary.GetSize (); ++s) {
			const CWArticleSummary& entry = summary[s];
			json += "    { \"articleId\": " + JsonString (entry.articleId)
				+ ", \"name\": " + JsonString (entry.articleName)
				+ ", \"unit\": " + JsonString (entry.unit)
				+ ", \"elementCount\": " + GS::ToUniString (std::to_wstring (static_cast<int> (entry.elementCount)))
				+ ", \"groupCount\": " + GS::ToUniString (std::to_wstring (static_cast<int> (entry.groupCount)))
				+ ", \"totalQuantity\": " + FormatDouble (entry.totalQuantity) + " }";
			if (s + 1 < summary.GetSize ())
				json += ",";
			json += "\n";
		}
		json += "  ],\n";
	}

	json += "  \"elements\": [\n";

	for (UIndex e = 0; e < rows.GetSize (); ++e) {
		const CWElementRow& row = rows[e];

		// Ligne « Ensemble » (phase 4) : bloc spécifique avec ses membres
		// consommés et la quantité facturée.
		if (row.isGroupRow) {
			const CWArticle* groupArticle = FindArticle (articles, row.classItemId);
			GS::UniString billedUnit = FR ("?");
			const double billedQuantity = (groupArticle != nullptr)
				? ArticleManager::ComputeBilledQuantity (*groupArticle, row, rows, billedUnit)
				: 0.0;

			json += "    {\n";
			json += US ("      \"kind\": \"group\",\n");
			json += US ("      \"groupId\": ") + JsonString (row.groupId) + ",\n";
			json += US ("      \"floorIndex\": ") + GS::ToUniString (std::to_wstring (static_cast<int> (row.floorInd))) + ",\n";
			json += US ("      \"story\": ") + JsonString (row.storyName) + ",\n";
			json += "      \"classification\": {\n";
			json += US ("        \"itemId\": ") + JsonString (row.classItemId) + ",\n";
			json += US ("        \"itemName\": ") + JsonString (row.classItemName) + "\n";
			json += "      },\n";
			if (groupArticle != nullptr) {
				json += US ("      \"article\": { \"id\": ") + JsonString (groupArticle->id)
					+ US (", \"name\": ") + JsonString (groupArticle->name)
					+ US (", \"unit\": ") + JsonString (groupArticle->unit) + " },\n";
			} else {
				json += "      \"article\": null,\n";
			}
			json += US ("      \"billedQuantity\": ") + FormatDouble (billedQuantity) + ",\n";
			json += US ("      \"billedUnit\": ") + JsonString (billedUnit) + ",\n";
			json += "      \"members\": [\n";
			for (UIndex m = 0; m < row.groupMembers.GetSize (); ++m) {
				const CWElementRow* member = FindRowByGuid (rows, row.groupMembers[m]);
				if (member == nullptr)
					continue;
				json += US ("        { \"guid\": ") + JsonString (APIGuidToString (member->guid))
					+ US (", \"type\": ") + JsonString (member->typeName)
					+ US (", \"elementId\": ") + JsonString (member->elementId) + " }";
				if (m + 1 < row.groupMembers.GetSize ())
					json += ",";
				json += "\n";
			}
			json += "      ]\n";
			json += "    }";
			if (e + 1 < rows.GetSize ())
				json += ",";
			json += "\n";
			continue;
		}

		json += "    {\n";
		json += US ("      \"guid\": ") + JsonString (APIGuidToString (row.guid)) + ",\n";
		json += US ("      \"type\": ") + JsonString (row.typeName) + ",\n";
		json += US ("      \"elementId\": ") + JsonString (row.elementId) + ",\n";
		json += US ("      \"floorIndex\": ") + GS::ToUniString (std::to_wstring (static_cast<int> (row.floorInd))) + ",\n";
		json += US ("      \"story\": ") + JsonString (row.storyName) + ",\n";
		json += "      \"classification\": {\n";
		json += US ("        \"itemId\": ") + JsonString (row.classItemId) + ",\n";
		json += US ("        \"itemName\": ") + JsonString (row.classItemName) + "\n";
		json += "      },\n";

		// Article CostWaves correspondant à la classe (null si non reconnu).
		const CWArticle* article = FindArticle (articles, row.classItemId);
		if (article != nullptr) {
			json += US ("      \"article\": { \"id\": ") + JsonString (article->id)
				  + US (", \"name\": ") + JsonString (article->name)
				  + US (", \"unit\": ") + JsonString (article->unit) + " },\n";
		} else {
			json += "      \"article\": null,\n";
		}

		// Membre d'un ensemble (phase 4) : consommé, facturé via l'ensemble.
		if (!row.groupId.IsEmpty ()) {
			json += US ("      \"groupId\": ") + JsonString (row.groupId) + ",\n";
			json += "      \"consumed\": true,\n";
		}

		json += "      \"quantities\": [";
		for (UIndex q = 0; q < row.quantities.GetSize (); ++q) {
			if (q > 0)
				json += ", ";
			json += US ("{ \"label\": ") + JsonString (row.quantities[q].label)
				  + ", \"value\": " + FormatDouble (row.quantities[q].value)
				  + ", \"unit\": " + JsonString (row.quantities[q].unit) + " }";
		}
		json += "],\n";

		json += "      \"components\": [";
		bool firstComponent = true;
		for (UIndex c = 0; c < row.components.GetSize (); ++c) {
			const CWComponentRow& component = row.components[c];

			if (!firstComponent)
				json += ", ";
			firstComponent = false;

			if (component.kind == RowKind::Skin) {
				json += US ("{ \"kind\": \"skin\", \"material\": ") + JsonString (component.label);
				if (!component.compositeName.IsEmpty ()) {
					json += US (", \"composite\": ") + JsonString (component.compositeName);
					if (component.skinIndex >= 0 && component.skinCount > 0) {
						json += US (", \"skinIndex\": ")
							+ GS::ToUniString (std::to_wstring (component.skinIndex))
							+ US (", \"skinCount\": ")
							+ GS::ToUniString (std::to_wstring (component.skinCount));
					}
					if (component.coreSkin)
						json += ", \"core\": true";
					if (component.finishSkin)
						json += ", \"finish\": true";
				}
				json += ", \"quantities\": [";
				for (UIndex q = 0; q < component.quantities.GetSize (); ++q) {
					if (q > 0)
						json += ", ";
					json += US ("{ \"label\": ") + JsonString (component.quantities[q].label)
						  + ", \"value\": " + FormatDouble (component.quantities[q].value)
						  + ", \"unit\": " + JsonString (component.quantities[q].unit) + " }";
				}
				json += "] }";
			} else {
				// Propriétés du composant, lues à l'export.
				API_ElemComponentID componentId;
				BNZeroMemory (&componentId, sizeof (componentId));
				componentId.elemGuid = row.guid;
				componentId.componentID.componentGuid = component.guid;

				const GS::Array<CWPropertyEntry> properties = ModelReader::GetComponentProperties (componentId);

				json += US ("{ \"kind\": \"component\", \"guid\": ") + JsonString (APIGuidToString (component.guid))
					  + ", \"properties\": [";
				for (UIndex p = 0; p < properties.GetSize (); ++p) {
					if (p > 0)
						json += ", ";
					json += US ("{ \"name\": ") + JsonString (properties[p].name)
						  + ", \"value\": " + JsonString (properties[p].value) + " }";
				}
				json += "] }";
			}
		}
		json += "],\n";

		json += "    }";
		if (e + 1 < rows.GetSize ())
			json += ",";
		json += "\n";
	}

	json += "  ]\n";
	json += "}\n";

	if (!WriteUtf8File (outPath, json, false)) {
		outError = FR ("Échec d'écriture du fichier JSON.");
		return APIERR_GENERAL;
	}

	return NoError;
}


GSErrCode Exporter::ExportCSV (const GS::UniString& systemName, const GS::Array<CWElementRow>& rows,
							   const CWScanReport& report, const GS::Array<CWArticle>& articles,
							   GS::UniString& outPath, GS::UniString& outError)
{
	(void) systemName;
	(void) report;

	if (!BuildExportPath ("csv", outPath, outError))
		return APIERR_GENERAL;

	GS::UniString csv;
	csv += FR ("Type;GUID;ID élément;Étage;Classe;Article;Libellé;Valeur;Unité\n");

	// Séparateur CSV : la valeur peut contenir ';' — on protège par des guillemets.
	const auto protect = [] (const GS::UniString& value) -> GS::UniString {
		if (value.IsEmpty ())
			return value;
		return US ("\"") + value + "\"";
	};

	for (UIndex e = 0; e < rows.GetSize (); ++e) {
		const CWElementRow& row = rows[e];

		const GS::UniString floorText = GS::ToUniString (std::to_wstring (static_cast<int> (row.floorInd)));
		const GS::UniString story = row.storyName.IsEmpty ()
			? floorText
			: floorText + " - " + row.storyName;

		const GS::UniString classe = row.classItemId.IsEmpty ()
			? row.classItemName
			: row.classItemId + " - " + row.classItemName;

		// Article de l'élément (identifiant CostWaves reconnu parmi les articles).
		const CWArticle* article = FindArticle (articles, row.classItemId);
		const GS::UniString articleId = (article != nullptr) ? article->id : GS::UniString ();

		// Type de ligne : élément, membre consommé, ou ensemble (phase 4).
		const GS::UniString rowType = row.isGroupRow
			? FR ("Ensemble")
			: (row.consumed ? FR ("Membre (consommé)") : FR ("Élément"));

		const GS::UniString elementPrefix = rowType + ";"
			+ protect (APIGuidToString (row.guid)) + ";"
			+ protect (row.elementId) + ";"
			+ protect (story) + ";"
			+ protect (classe) + ";"
			+ protect (articleId) + ";";

		if (row.isGroupRow) {
			// Ensemble : une ligne « Quantité facturée » (phase 4).
			GS::UniString billedUnit = FR ("?");
			const double billedQuantity = (article != nullptr)
				? ArticleManager::ComputeBilledQuantity (*article, row, rows, billedUnit)
				: 0.0;
			csv += elementPrefix
				+ protect (FR ("Quantité facturée")) + ";"
				+ protect (FormatDouble (billedQuantity)) + ";"
				+ protect (billedUnit) + "\n";
		} else if (row.quantities.IsEmpty ()) {
			csv += elementPrefix + ";;;\n";
		} else {
			for (UIndex q = 0; q < row.quantities.GetSize (); ++q) {
				csv += elementPrefix
					+ protect (row.quantities[q].label) + ";"
					+ protect (FormatDouble (row.quantities[q].value)) + ";"
					+ protect (row.quantities[q].unit) + "\n";
			}
		}

		for (UIndex c = 0; c < row.components.GetSize (); ++c) {
			const CWComponentRow& component = row.components[c];

			GS::UniString kindLabel;
			if (component.kind == RowKind::Skin) {
				kindLabel = FR ("Composant (skin) — ") + component.label;
				if (!component.compositeName.IsEmpty ())
					kindLabel += FR (" · ") + component.compositeName;
				if (component.coreSkin)
					kindLabel += FR (" (cœur)");
			} else {
				kindLabel = FR ("Composant — ") + component.label;
			}

			const GS::UniString componentGuid = (component.guid == APINULLGuid)
				? GS::UniString ()
				: APIGuidToString (component.guid);

			const GS::UniString componentPrefix = protect (kindLabel) + ";"
				+ protect (componentGuid) + ";"
				+ protect (row.elementId) + ";"
				+ protect (story) + ";"
				+ protect (classe) + ";"
				+ protect (articleId) + ";";

			if (component.quantities.IsEmpty ()) {
				csv += componentPrefix + ";;;\n";
			} else {
				for (UIndex q = 0; q < component.quantities.GetSize (); ++q) {
					csv += componentPrefix
						+ protect (component.quantities[q].label) + ";"
						+ protect (FormatDouble (component.quantities[q].value)) + ";"
						+ protect (component.quantities[q].unit) + "\n";
				}
			}
		}
	}

	// Récapitulatif par article (phase 4) : une ligne par article facturé.
	{
		GS::Array<CWArticleSummary> summary;
		ArticleManager::BuildArticleSummary (rows, articles, summary);

		if (!summary.IsEmpty ()) {
			csv += "\n";
			for (UIndex s = 0; s < summary.GetSize (); ++s) {
				const CWArticleSummary& entry = summary[s];
				csv += FR ("Récapitulatif") + ";;;;;"
					+ protect (entry.articleId) + ";"
					+ protect (entry.articleName) + ";"
					+ protect (FormatDouble (entry.totalQuantity)) + ";"
					+ protect (entry.unit) + "\n";
			}
		}
	}

	if (!WriteUtf8File (outPath, csv, true)) {
		outError = FR ("Échec d'écriture du fichier CSV.");
		return APIERR_GENERAL;
	}

	return NoError;
}

} // namespace CostWaves
