#include "CostWavesPrecompiledHeader.hpp"

#include "CostWavesApi.hpp"

#include "ArticleManager.hpp"
#include "Exporter.hpp"

#include "UniStringWStringConversion.hpp"

#include <cwchar>
#include <string>

// Client HTTP/HTTPS du système (Windows seul — la cible est Windows, spéc. §14).
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <winhttp.h>
#pragma comment (lib, "winhttp.lib")

namespace CostWaves {

namespace {

// Littéral UTF-8 -> GS::UniString (les sources sont compilees avec /utf-8).
GS::UniString FR (const char* utf8Text)
{
	return GS::UniString (utf8Text, CC_UTF8);
}

GS::UniString US (const char* utf8Text)
{
	return GS::UniString (utf8Text, CC_UTF8);
}

// Échappe une chaîne pour JSON (délégué à l'Exporter, format identique).
GS::UniString JsonStr (const GS::UniString& text)
{
	return Exporter::JsonString (text);
}

GS::UniString Num (double value)
{
	wchar_t buffer[64];
	swprintf (buffer, 64, L"%.4f", value);

	std::wstring text (buffer);
	while (text.length () > 1 && text.back () == L'0')
		text.pop_back ();
	if (text.length () > 1 && text.back () == L'.')
		text.pop_back ();
	if (text == L"-0")
		text = L"0";

	return GS::ToUniString (text);
}

GS::UniString IntNum (USize value)
{
	return GS::ToUniString (std::to_wstring (static_cast<int> (value)));
}

// Chemin du fichier de réglages : à côté du PLN (repli : Documents).
GS::UniString SettingsFilePath ()
{
	GS::UniString folder;
	GS::UniString projectName;
	if (!Exporter::ResolveProjectLocation (folder, projectName))
		return GS::UniString ();
	return folder + "/" + US ("CostWaves-settings.json");
}

// --- Mini-extraction JSON (réponses du serveur) ------------------------------
// Le format définitif sera décidé avec l'API CostWaves (spéc. §13) : on lit
// les champs convenus ci-dessous s'ils sont présents, sans exiger de schéma.
// Analyse sur std::wstring (recherche/accès direct sûrs).

bool IsSpace (wchar_t ch)
{
	return ch == L' ' || ch == L'\t' || ch == L'\n' || ch == L'\r';
}

// Position de la valeur qui suit « "key" : », ou npos.
size_t FindJsonValue (const std::wstring& json, const std::wstring& key, size_t searchFrom = 0)
{
	const std::wstring needle = L"\"" + key + L"\"";
	size_t pos = searchFrom;
	while (true) {
		const size_t keyPos = json.find (needle, pos);
		if (keyPos == std::wstring::npos)
			return std::wstring::npos;

		size_t p = keyPos + needle.length ();
		while (p < json.length () && IsSpace (json[p]))
			++p;
		if (p >= json.length () || json[p] != L':') {
			pos = keyPos + 1;
			continue;
		}
		++p;
		while (p < json.length () && IsSpace (json[p]))
			++p;
		return p;
	}
}

// Valeur chaîne de la clé (false si absente).
bool ExtractJsonString (const std::wstring& json, const std::wstring& key, GS::UniString& outValue)
{
	const size_t p = FindJsonValue (json, key);
	if (p == std::wstring::npos || p >= json.length () || json[p] != L'"')
		return false;

	std::wstring value;
	size_t i = p + 1;
	while (i < json.length () && json[i] != L'"') {
		if (json[i] == L'\\' && i + 1 < json.length ()) {
			++i;
			switch (json[i]) {
				case L'n': value += L'\n'; break;
				case L't': value += L'\t'; break;
				case L'r': value += L'\r'; break;
				default: value += json[i]; break;
			}
		} else {
			value += json[i];
		}
		++i;
	}
	outValue = GS::ToUniString (value);
	return true;
}

// Valeur numérique de la clé (0 si absente).
double ExtractJsonNumber (const std::wstring& json, const std::wstring& key)
{
	const size_t p = FindJsonValue (json, key);
	if (p == std::wstring::npos)
		return 0.0;

	std::wstring number;
	size_t i = p;
	while (i < json.length ()
		   && (json[i] == L'-' || json[i] == L'+' || json[i] == L'.'
			   || (json[i] >= L'0' && json[i] <= L'9'))) {
		number += json[i];
		++i;
	}
	if (number.empty ())
		return 0.0;
	return wcstod (number.c_str (), nullptr);
}

// Tableau de chaînes de la clé ("key": [ "a", "b" ]).
GS::Array<GS::UniString> ExtractJsonStringArray (const std::wstring& json, const std::wstring& key)
{
	GS::Array<GS::UniString> values;
	const size_t p = FindJsonValue (json, key);
	if (p == std::wstring::npos || p >= json.length () || json[p] != L'[')
		return values;

	std::wstring current;
	bool inString = false;
	for (size_t i = p + 1; i < json.length () && json[i] != L']'; ++i) {
		const wchar_t ch = json[i];
		if (!inString && ch == L'"') {
			inString = true;
			current.clear ();
		} else if (inString && ch == L'"') {
			values.Push (GS::ToUniString (current));
			current.clear ();
			inString = false;
		} else if (inString && ch == L'\\' && i + 1 < json.length ()) {
			++i;
			current += json[i];
		} else if (inString) {
			current += ch;
		}
	}
	return values;
}

} // namespace


GS::UniString CostWavesApi::QuantityKey (const GS::UniString& label)
{
	// Libellés connus -> clés JSON stables (spéc. §13).
	struct KnownKey { const wchar_t* label; const char* key; };
	static const KnownKey knownKeys[] = {
		{ L"Surface", "surface" },
		{ L"Surface nette", "netSurface" },
		{ L"Surface projetée", "projectedSurface" },
		{ L"Volume", "volume" },
		{ L"Longueur", "length" },
		{ L"Longueur 3D", "length3d" },
		{ L"Périmètre", "perimeter" },
		{ L"Épaisseur", "thickness" },
		{ L"Hauteur", "height" },
		{ L"Largeur", "width" },
		{ L"Quantité", "quantity" }
	};

	const std::wstring labelW = GS::ToWString (label);
	for (UIndex i = 0; i < sizeof (knownKeys) / sizeof (knownKeys[0]); ++i) {
		if (labelW == knownKeys[i].label)
			return US (knownKeys[i].key);
	}

	// Clé générique : minuscule initiale, séparateurs retirés, accents
	// conservés en minuscules (« Surface latérale » -> « surfaceLaterale »).
	std::wstring key;
	bool first = true;
	bool upperNext = false;
	for (wchar_t ch : labelW) {
		if (ch == L' ' || ch == L'-' || ch == L'_' || ch == L'/' || ch == L'.') {
			if (!first && !key.empty ())
				upperNext = true;
			continue;
		}
		if (first) {
			if (ch >= L'A' && ch <= L'Z')
				ch = static_cast<wchar_t> (ch - L'A' + L'a');
			else if (ch == L'É') ch = L'é';
			else if (ch == L'È') ch = L'è';
			else if (ch == L'Ê') ch = L'ê';
			first = false;
		} else if (upperNext) {
			if (ch >= L'a' && ch <= L'z')
				ch = static_cast<wchar_t> (ch - L'a' + L'A');
			else if (ch == L'é') ch = L'É';
			upperNext = false;
		}
		key += ch;
	}
	if (key.empty ())
		key = L"value";
	return GS::ToUniString (key);
}


bool CostWavesApi::LoadSettings (CWApiSettings& outSettings, GS::UniString& outError)
{
	outError.Clear ();

	// Défauts (spéc. §6 : « Ajouter au projet uniquement »).
	outSettings.endpointUrl.Clear ();
	outSettings.apiKey.Clear ();
	outSettings.unknownArticleMode = US ("project_only");

	const GS::UniString path = SettingsFilePath ();
	if (path.IsEmpty ())
		return true;	// dossier inconnu : défauts, sans erreur

#if defined (WINDOWS)
	const std::wstring pathW = GS::ToWString (path);
	FILE* file = _wfopen (pathW.c_str (), L"rb");
#else
	const auto pathCStr = path.ToCStr (CC_Default);
	FILE* file = fopen (pathCStr.Get (), "rb");
#endif
	if (file == nullptr)
		return true;	// fichier absent : défauts

	std::wstring contentW;
	{
		char buffer[4096];
		size_t read = 0;
		bool first = true;
		while ((read = fread (buffer, 1, sizeof (buffer), file)) > 0) {
			size_t offset = 0;
			if (first) {
				first = false;
				if (read >= 3
					&& static_cast<unsigned char> (buffer[0]) == 0xEF
					&& static_cast<unsigned char> (buffer[1]) == 0xBB
					&& static_cast<unsigned char> (buffer[2]) == 0xBF)
					offset = 3;
			}
			const int wideLength = MultiByteToWideChar (CP_UTF8, 0, buffer + offset,
														static_cast<int> (read - offset), nullptr, 0);
			if (wideLength <= 0)
				break;
			const size_t oldSize = contentW.size ();
			contentW.resize (oldSize + static_cast<size_t> (wideLength));
			MultiByteToWideChar (CP_UTF8, 0, buffer + offset, static_cast<int> (read - offset),
								 &contentW[oldSize], wideLength);
		}
	}
	fclose (file);

	if (contentW.empty ())
		return true;

	ExtractJsonString (contentW, L"endpointUrl", outSettings.endpointUrl);
	ExtractJsonString (contentW, L"apiKey", outSettings.apiKey);
	ExtractJsonString (contentW, L"unknownArticleMode", outSettings.unknownArticleMode);
	if (outSettings.unknownArticleMode.IsEmpty ())
		outSettings.unknownArticleMode = US ("project_only");

	return true;
}


bool CostWavesApi::SaveSettings (const CWApiSettings& settings, GS::UniString& outError)
{
	outError.Clear ();

	const GS::UniString path = SettingsFilePath ();
	if (path.IsEmpty ()) {
		outError = FR ("Impossible de déterminer le dossier du projet (enregistrez le PLN puis réessayez).");
		return false;
	}

	GS::UniString json;
	json += US ("{\n");
	json += US ("  \"endpointUrl\": ") + JsonStr (settings.endpointUrl) + US (",\n");
	json += US ("  \"apiKey\": ") + JsonStr (settings.apiKey) + US (",\n");
	json += US ("  \"unknownArticleMode\": ") + JsonStr (settings.unknownArticleMode) + US ("\n");
	json += US ("}\n");

	if (!Exporter::WriteUtf8File (path, json, false)) {
		outError = FR ("Écriture de CostWaves-settings.json impossible.");
		return false;
	}
	return true;
}


GS::UniString CostWavesApi::BuildPayload (const GS::UniString& projectId, const GS::UniString& projectName,
										  const GS::Array<CWElementRow>& rows,
										  const GS::Array<CWArticle>& articles,
										  const GS::Array<CWArticleSummary>& summary,
										  const CWApiSettings& settings)
{
	GS::UniString json;
	json += US ("{\n");

	json += US ("  \"projectId\": ") + JsonStr (projectId) + US (",\n");
	json += US ("  \"projectName\": ") + JsonStr (projectName) + US (",\n");
	json += US ("  \"unknownArticleMode\": ") + JsonStr (settings.unknownArticleMode) + US (",\n");
	json += US ("  \"generatedAt\": ") + JsonStr (Exporter::Timestamp ()) + US (",\n");

	// Catalogue d'articles (id, libellé, unité).
	json += US ("  \"articles\": [");
	for (UIndex a = 0; a < articles.GetSize (); ++a) {
		json += (a > 0) ? US (", ") : US (" ");
		json += US ("{ \"id\": ") + JsonStr (articles[a].id)
			+ US (", \"name\": ") + JsonStr (articles[a].name)
			+ US (", \"unit\": ") + JsonStr (articles[a].unit) + US (" }");
	}
	json += US (" ],\n");

	// Récapitulatif facturé par article.
	json += US ("  \"summary\": [");
	for (UIndex s = 0; s < summary.GetSize (); ++s) {
		json += (s > 0) ? US (", ") : US (" ");
		json += US ("{ \"articleId\": ") + JsonStr (summary[s].articleId)
			+ US (", \"name\": ") + JsonStr (summary[s].articleName)
			+ US (", \"unit\": ") + JsonStr (summary[s].unit)
			+ US (", \"elementCount\": ") + IntNum (summary[s].elementCount)
			+ US (", \"groupCount\": ") + IntNum (summary[s].groupCount)
			+ US (", \"numberedGroupCount\": ") + IntNum (summary[s].numberedGroupCount)
			+ US (", \"skinCount\": ") + IntNum (summary[s].skinCount)
			+ US (", \"totalQuantity\": ") + Num (summary[s].totalQuantity) + US (" }");
	}
	json += US (" ],\n");

	// Éléments : éléments classés, ensembles, groupes numérotés ; les membres
	// consommés ne sont pas facturés individuellement (spéc. §8/§9) et les
	// éléments sans classe n'apparaissent que via leurs skins classés.
	json += US ("  \"elements\": [\n");
	bool firstElement = true;
	for (UIndex i = 0; i < rows.GetSize (); ++i) {
		const CWElementRow& row = rows[i];
		if (row.consumed)
			continue;
		if (row.classItemId.IsEmpty ())
			continue;

		const CWArticle* article = nullptr;
		for (UIndex a = 0; a < articles.GetSize (); ++a) {
			if (articles[a].id == row.classItemId) {
				article = &articles[a];
				break;
			}
		}

		GS::UniString type;
		if (row.isNumberedGroup)
			type = US ("numberedGroup");
		else if (row.isGroupRow)
			type = US ("group");
		else
			type = US ("element");

		GS::UniString unit;
		const double billed = (article != nullptr)
			? ArticleManager::ComputeBilledQuantity (*article, row, rows, unit)
			: 0.0;

		json += firstElement ? US ("    {\n") : US (",\n    {\n");
		firstElement = false;

		json += US ("      \"guid\": ") + JsonStr (APIGuidToString (row.guid)) + US (",\n");
		json += US ("      \"type\": ") + JsonStr (type) + US (",\n");
		json += US ("      \"class\": ") + JsonStr (row.classItemId) + US (",\n");
		json += US ("      \"className\": ") + JsonStr (row.classItemName) + US (",\n");
		json += US ("      \"articleId\": ") + JsonStr (row.classItemId) + US (",\n");
		if (!row.elementId.IsEmpty ())
			json += US ("      \"elementId\": ") + JsonStr (row.elementId) + US (",\n");
		if (!row.storyName.IsEmpty ())
			json += US ("      \"story\": ") + JsonStr (row.storyName) + US (",\n");
		if (!row.groupId.IsEmpty ()) {
			json += US ("      \"groupId\": ") + JsonStr (row.groupId) + US (",\n");
			if (row.isNumberedGroup)
				json += US ("      \"groupNumber\": ") + IntNum (static_cast<USize> (row.groupNumber)) + US (",\n");
		}
		json += US ("      \"billedQuantity\": ") + Num (billed) + US (",\n");
		json += US ("      \"unit\": ") + JsonStr (unit) + US (",\n");

		// Toutes les quantités disponibles (spéc. §3), clés normalisées.
		json += US ("      \"quantities\": {");
		for (UIndex q = 0; q < row.quantities.GetSize (); ++q) {
			if (q > 0)
				json += US (", ");
			json += US ("\"") + QuantityKey (row.quantities[q].label) + US ("\": ")
				+ Num (row.quantities[q].value);
		}
		json += US ("},\n");

		// Skins classés (spéc. §4) — exportés indépendamment des éléments.
		json += US ("      \"components\": [");
		bool firstComponent = true;
		for (UIndex c = 0; c < row.components.GetSize (); ++c) {
			const CWComponentRow& component = row.components[c];
			if (component.kind != RowKind::Skin || component.classItemId.IsEmpty ())
				continue;

			json += firstComponent ? US (" ") : US (", ");
			firstComponent = false;

			json += US ("{ \"type\": \"skin\", \"parentGuid\": ") + JsonStr (APIGuidToString (row.guid))
				+ US (", \"material\": ") + JsonStr (component.label)
				+ US (", \"class\": ") + JsonStr (component.classItemId)
				+ US (", \"className\": ") + JsonStr (component.classItemName)
				+ US (", \"articleId\": ") + JsonStr (component.classItemId)
				+ US (", \"quantities\": {");
			for (UIndex q = 0; q < component.quantities.GetSize (); ++q) {
				if (q > 0)
					json += US (", ");
				json += US ("\"") + QuantityKey (component.quantities[q].label) + US ("\": ")
					+ Num (component.quantities[q].value);
			}
			json += US ("} }");
		}
		json += US (" ]\n");

		json += US ("    }");
	}
	json += US ("\n  ]\n");

	json += US ("}\n");
	return json;
}


CWApiSendResult CostWavesApi::Send (const CWApiSettings& settings, const GS::UniString& payload)
{
	CWApiSendResult result;

	if (settings.endpointUrl.IsEmpty ()) {
		result.err = APIERR_GENERAL;
		result.error = FR ("L'URL du serveur CostWaves n'est pas définie.");
		return result;
	}

	// Décomposition de l'URL (schéma + hôte + chemin).
	URL_COMPONENTS parts;
	ZeroMemory (&parts, sizeof (parts));
	parts.dwStructSize = sizeof (parts);

	wchar_t hostName[256] = L"";
	wchar_t path[1024] = L"";
	parts.lpszHostName = hostName;
	parts.dwHostNameLength = 255;
	parts.lpszUrlPath = path;
	parts.dwUrlPathLength = 1023;

	const std::wstring urlW = GS::ToWString (settings.endpointUrl);
	if (urlW.empty () || !WinHttpCrackUrl (urlW.c_str (), static_cast<DWORD> (urlW.length ()), 0, &parts)) {
		result.err = APIERR_GENERAL;
		result.error = FR ("URL du serveur invalide (format attendu : https://hote/chemin).");
		return result;
	}
	const bool isHttps = (parts.nScheme == INTERNET_SCHEME_HTTPS);
	const INTERNET_PORT port = parts.nPort;

	HINTERNET session = WinHttpOpen (L"CostWaves Add-On", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
									 WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
	if (session == nullptr) {
		result.err = APIERR_GENERAL;
		result.error = FR ("Initialisation réseau impossible (WinHttpOpen).");
		return result;
	}

	WinHttpSetTimeouts (session, 10000, 10000, 30000, 30000);

	HINTERNET connection = WinHttpConnect (session, hostName, port, 0);
	if (connection == nullptr) {
		result.err = APIERR_GENERAL;
		result.error = FR ("Connexion au serveur impossible (hôte injoignable).");
		WinHttpCloseHandle (session);
		return result;
	}

	HINTERNET request = WinHttpOpenRequest (connection, L"POST", path,
											nullptr, WINHTTP_NO_REFERER,
											WINHTTP_DEFAULT_ACCEPT_TYPES,
											isHttps ? WINHTTP_FLAG_SECURE : 0);
	if (request == nullptr) {
		result.err = APIERR_GENERAL;
		result.error = FR ("Préparation de la requête impossible.");
		WinHttpCloseHandle (connection);
		WinHttpCloseHandle (session);
		return result;
	}

	// En-têtes : JSON + clé API (Authorization: Bearer).
	GS::UniString headers = US ("Content-Type: application/json\r\nAccept: application/json\r\n");
	if (!settings.apiKey.IsEmpty ())
		headers += US ("Authorization: Bearer ") + settings.apiKey + US ("\r\n");

	const std::wstring headersW = GS::ToWString (headers);

	// Corps en UTF-8 (les libellés peuvent porter des accents).
	const std::wstring payloadW = GS::ToWString (payload);
	const int utf8Length = WideCharToMultiByte (CP_UTF8, 0, payloadW.c_str (), static_cast<int> (payloadW.length ()),
												nullptr, 0, nullptr, nullptr);
	if (utf8Length <= 0) {
		result.err = APIERR_GENERAL;
		result.error = FR ("Encodage du contenu impossible.");
		WinHttpCloseHandle (request);
		WinHttpCloseHandle (connection);
		WinHttpCloseHandle (session);
		return result;
	}
	std::string utf8Body (static_cast<size_t> (utf8Length), '\0');
	WideCharToMultiByte (CP_UTF8, 0, payloadW.c_str (), static_cast<int> (payloadW.length ()),
						 &utf8Body[0], utf8Length, nullptr, nullptr);

	if (!WinHttpSendRequest (request, headersW.c_str (), static_cast<DWORD> (-1),
							 &utf8Body[0], static_cast<DWORD> (utf8Body.size ()),
							 static_cast<DWORD> (utf8Body.size ()), 0)) {
		result.err = APIERR_GENERAL;
		result.error = FR ("Échec de l'envoi de la requête (réseau/TLS).");
		WinHttpCloseHandle (request);
		WinHttpCloseHandle (connection);
		WinHttpCloseHandle (session);
		return result;
	}

	if (!WinHttpReceiveResponse (request, nullptr)) {
		result.err = APIERR_GENERAL;
		result.error = FR ("Aucune réponse du serveur (délai dépassé ?).");
		WinHttpCloseHandle (request);
		WinHttpCloseHandle (connection);
		WinHttpCloseHandle (session);
		return result;
	}

	DWORD statusCode = 0;
	DWORD statusCodeSize = sizeof (statusCode);
	WinHttpQueryHeaders (request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
						 WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &statusCodeSize, WINHTTP_NO_HEADER_INDEX);
	result.httpStatus = static_cast<int> (statusCode);

	// Corps de la réponse (réassemblé en UTF-16).
	std::wstring responseW;
	{
		DWORD available = 0;
		while (WinHttpQueryDataAvailable (request, &available) && available > 0) {
			std::string chunk (available, '\0');
			DWORD bytesRead = 0;
			if (!WinHttpReadData (request, &chunk[0], available, &bytesRead) || bytesRead == 0)
				break;
			chunk.resize (bytesRead);
			const int wideLength = MultiByteToWideChar (CP_UTF8, 0, chunk.c_str (), static_cast<int> (bytesRead),
														nullptr, 0);
			if (wideLength > 0) {
				const size_t oldSize = responseW.size ();
				responseW.resize (oldSize + static_cast<size_t> (wideLength));
				MultiByteToWideChar (CP_UTF8, 0, chunk.c_str (), static_cast<int> (bytesRead),
									 &responseW[oldSize], wideLength);
			}
			available = 0;
		}
	}
	result.rawResponse = GS::ToUniString (responseW);

	WinHttpCloseHandle (request);
	WinHttpCloseHandle (connection);
	WinHttpCloseHandle (session);

	// Champs convenus de la réponse (présents ou non).
	result.hasServerData = !result.rawResponse.IsEmpty ();
	if (result.hasServerData) {
		result.createdLines = static_cast<USize> (ExtractJsonNumber (responseW, L"createdLines");
		result.updatedLines = static_cast<USize> (ExtractJsonNumber (responseW, L"updatedLines");
		result.unknownArticles = ExtractJsonStringArray (responseW, L"unknownArticles");
		ExtractJsonString (responseW, L"message", result.message);
	}

	if (statusCode >= 400) {
		result.err = APIERR_GENERAL;
		if (result.error.IsEmpty ())
			result.error = FR ("Le serveur CostWaves a répondu une erreur (HTTP ")
				+ GS::ToUniString (std::to_wstring (static_cast<int> (statusCode))) + FR (").");
	}

	return result;
}

} // namespace CostWaves
