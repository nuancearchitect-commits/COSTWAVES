#include "CostWavesPrecompiledHeader.hpp"

#include "ArticleManager.hpp"

#include "UniStringWStringConversion.hpp"

#include <chrono>
#include <cwchar>
#include <cstdlib>
#include <utility>

namespace CostWaves {

namespace {

GS::UniString FR (const char* utf8Text)
{
	return GS::UniString (utf8Text, CC_UTF8);
}

GS::UniString ErrorCodeText (GSErrCode err)
{
	return GS::ToUniString (std::to_wstring (static_cast<int> (err)));
}

// ---------------------------------------------------------------------------
// Lecture d'un fichier texte UTF-8 (BOM toléré) dans un std::string (octets).
// ---------------------------------------------------------------------------

bool ReadUtf8File (const GS::UniString& path, std::string& outContent)
{
#if defined (WINDOWS)
	const std::wstring pathW = GS::ToWString (path);
	FILE* file = _wfopen (pathW.c_str (), L"rb");
#else
	const auto pathCStr = path.ToCStr (CC_Default);
	FILE* file = fopen (pathCStr.Get (), "rb");
#endif
	if (file == nullptr)
		return false;

	outContent.clear ();
	char buffer[4096];
	size_t read = 0;
	while ((read = fread (buffer, 1, sizeof (buffer), file)) > 0)
		outContent.append (buffer, read);

	fclose (file);

	// Retirer un éventuel BOM UTF-8.
	if (outContent.size () >= 3
		&& static_cast<unsigned char> (outContent[0]) == 0xEF
		&& static_cast<unsigned char> (outContent[1]) == 0xBB
		&& static_cast<unsigned char> (outContent[2]) == 0xBF) {
		outContent.erase (0, 3);
	}

	return true;
}

// ---------------------------------------------------------------------------
// Mini-parseur JSON (phase 2) — juste ce qu'il faut pour les articles :
//  [ {"id":"...","name":"...","unit":"..."}, ... ]  ou  {"articles":[...]}
// ---------------------------------------------------------------------------

struct JsonValue {
	enum class Type { Null, Bool, Number, String, Array, Object };

	Type			type = Type::Null;
	bool			boolValue = false;
	double			numberValue = 0.0;
	std::string		stringValue;										// UTF-8 brut
	std::vector<JsonValue> arrayValue;
	std::vector<std::pair<std::string, JsonValue>> objectValue;			// ordre préservé

	const JsonValue* Find (const char* key) const
	{
		for (const auto& entry : objectValue) {
			if (entry.first == key)
				return &entry.second;
		}
		return nullptr;
	}
};

void AppendUtf8 (std::string& target, unsigned int codePoint)
{
	if (codePoint < 0x80) {
		target.push_back (static_cast<char> (codePoint));
	} else if (codePoint < 0x800) {
		target.push_back (static_cast<char> (0xC0 | (codePoint >> 6)));
		target.push_back (static_cast<char> (0x80 | (codePoint & 0x3F)));
	} else if (codePoint < 0x10000) {
		target.push_back (static_cast<char> (0xE0 | (codePoint >> 12)));
		target.push_back (static_cast<char> (0x80 | ((codePoint >> 6) & 0x3F)));
		target.push_back (static_cast<char> (0x80 | (codePoint & 0x3F)));
	} else {
		target.push_back (static_cast<char> (0xF0 | (codePoint >> 18)));
		target.push_back (static_cast<char> (0x80 | ((codePoint >> 12) & 0x3F)));
		target.push_back (static_cast<char> (0x80 | ((codePoint >> 6) & 0x3F)));
		target.push_back (static_cast<char> (0x80 | (codePoint & 0x3F)));
	}
}

unsigned int ParseHex4 (const std::string& s, size_t pos, bool& ok)
{
	unsigned int value = 0;
	for (size_t k = 0; k < 4; ++k) {
		if (pos + k >= s.size ()) { ok = false; return 0; }
		const char c = s[pos + k];
		value <<= 4;
		if (c >= '0' && c <= '9') value |= static_cast<unsigned int> (c - '0');
		else if (c >= 'a' && c <= 'f') value |= static_cast<unsigned int> (c - 'a' + 10);
		else if (c >= 'A' && c <= 'F') value |= static_cast<unsigned int> (c - 'A' + 10);
		else { ok = false; return 0; }
	}
	return value;
}

class JsonParser {
public:
	explicit JsonParser (const std::string& source) : s (source) {}

	bool Parse (JsonValue& outValue, size_t& outErrorPos)
	{
		size_t pos = 0;
		ok = true;
		ParseValue (outValue, pos);
		SkipWs (pos);
		if (ok && pos != s.size ())
			ok = false;		// contenu résiduel après la valeur
		outErrorPos = pos;
		return ok;
	}

private:
	const std::string&	s;
	bool				ok = true;

	void SkipWs (size_t& pos) const
	{
		while (pos < s.size () && (s[pos] == ' ' || s[pos] == '\t' || s[pos] == '\n' || s[pos] == '\r'))
			++pos;
	}

	void ParseValue (JsonValue& v, size_t& pos)
	{
		SkipWs (pos);
		if (pos >= s.size ()) { ok = false; return; }

		const char c = s[pos];
		if (c == '{') {
			ParseObject (v, pos);
		} else if (c == '[') {
			ParseArray (v, pos);
		} else if (c == '"') {
			v.type = JsonValue::Type::String;
			ParseString (v.stringValue, pos);
		} else if (c == 't' && s.compare (pos, 4, "true") == 0) {
			pos += 4;
			v.type = JsonValue::Type::Bool;
			v.boolValue = true;
		} else if (c == 'f' && s.compare (pos, 5, "false") == 0) {
			pos += 5;
			v.type = JsonValue::Type::Bool;
			v.boolValue = false;
		} else if (c == 'n' && s.compare (pos, 4, "null") == 0) {
			pos += 4;
			v.type = JsonValue::Type::Null;
		} else if (c == '-' || (c >= '0' && c <= '9')) {
			ParseNumber (v, pos);
		} else {
			ok = false;
		}
	}

	void ParseObject (JsonValue& v, size_t& pos)
	{
		v.type = JsonValue::Type::Object;
		++pos;	// '{'
		SkipWs (pos);
		if (pos < s.size () && s[pos] == '}') { ++pos; return; }

		while (ok) {
			SkipWs (pos);
			if (pos >= s.size () || s[pos] != '"') { ok = false; return; }
			std::string key;
			ParseString (key, pos);
			SkipWs (pos);
			if (pos >= s.size () || s[pos] != ':') { ok = false; return; }
			++pos;
			JsonValue value;
			ParseValue (value, pos);
			if (!ok)
				return;
			v.objectValue.emplace_back (std::move (key), std::move (value));
			SkipWs (pos);
			if (pos < s.size () && s[pos] == ',') { ++pos; continue; }
			if (pos < s.size () && s[pos] == '}') { ++pos; return; }
			ok = false;
			return;
		}
	}

	void ParseArray (JsonValue& v, size_t& pos)
	{
		v.type = JsonValue::Type::Array;
		++pos;	// '['
		SkipWs (pos);
		if (pos < s.size () && s[pos] == ']') { ++pos; return; }

		while (ok) {
			JsonValue value;
			ParseValue (value, pos);
			if (!ok)
				return;
			v.arrayValue.push_back (std::move (value));
			SkipWs (pos);
			if (pos < s.size () && s[pos] == ',') { ++pos; continue; }
			if (pos < s.size () && s[pos] == ']') { ++pos; return; }
			ok = false;
			return;
		}
	}

	void ParseString (std::string& outText, size_t& pos)
	{
		++pos;	// '"'
		outText.clear ();
		while (pos < s.size ()) {
			const char c = s[pos];
			if (c == '"') { ++pos; return; }
			if (c != '\\') {
				outText.push_back (c);
				++pos;
				continue;
			}
			// Séquence d'échappement.
			++pos;
			if (pos >= s.size ()) { ok = false; return; }
			const char e = s[pos];
			switch (e) {
				case '"':	outText.push_back ('"'); ++pos; break;
				case '\\':	outText.push_back ('\\'); ++pos; break;
				case '/':	outText.push_back ('/'); ++pos; break;
				case 'b':	outText.push_back ('\b'); ++pos; break;
				case 'f':	outText.push_back ('\f'); ++pos; break;
				case 'n':	outText.push_back ('\n'); ++pos; break;
				case 'r':	outText.push_back ('\r'); ++pos; break;
				case 't':	outText.push_back ('\t'); ++pos; break;
				case 'u': {
					++pos;
					bool hexOk = true;
					const unsigned int code = ParseHex4 (s, pos, hexOk);
					if (!hexOk) { ok = false; return; }
					pos += 4;
					unsigned int codePoint = code;
					if (code >= 0xD800 && code <= 0xDBFF) {
						// demi-zone haute : exiger une demi-zone basse.
						if (pos + 1 < s.size () && s[pos] == '\\' && s[pos + 1] == 'u') {
							const unsigned int low = ParseHex4 (s, pos + 2, hexOk);
							if (hexOk && low >= 0xDC00 && low <= 0xDFFF) {
								codePoint = 0x10000 + ((code - 0xD800) << 10) + (low - 0xDC00);
								pos += 6;
							}
						}
					}
					AppendUtf8 (outText, codePoint);
					break;
				}
				default:
					ok = false;
					return;
			}
		}
		ok = false;		// fin de chaîne non fermée
	}

	void ParseNumber (JsonValue& v, size_t& pos)
	{
		const size_t begin = pos;
		if (pos < s.size () && s[pos] == '-') ++pos;
		while (pos < s.size () && ((s[pos] >= '0' && s[pos] <= '9') || s[pos] == '.' || s[pos] == 'e'
								 || s[pos] == 'E' || s[pos] == '+' || s[pos] == '-')) {
			++pos;
		}
		if (pos == begin) { ok = false; return; }
		v.type = JsonValue::Type::Number;
		v.numberValue = std::strtod (s.substr (begin, pos - begin).c_str (), nullptr);
	}
};

// Champ d'article accepté comme chaîne ou comme nombre (converti).
GS::UniString ArticleFieldText (const JsonValue& articleObject, const char* key)
{
	const JsonValue* field = articleObject.Find (key);
	if (field == nullptr)
		return GS::UniString ();

	if (field->type == JsonValue::Type::String)
		return GS::UniString (field->stringValue.c_str (), CC_UTF8);

	if (field->type == JsonValue::Type::Number) {
		// "12" plutôt que "12.000000".
		wchar_t buffer[32];
		swprintf (buffer, 32, L"%.6g", field->numberValue);
		return GS::ToUniString (std::wstring (buffer));
	}

	if (field->type == JsonValue::Type::Bool)
		return field->boolValue ? FR ("true") : FR ("false");

	return GS::UniString ();
}

} // namespace


const char* ArticleManager::CostWavesSystemName ()
{
	return "CostWaves";
}


bool ArticleManager::ImportFromJsonFile (const GS::UniString& path, GS::Array<CWArticle>& outArticles,
										 GS::UniString& outError)
{
	outArticles.Clear ();

	std::string content;
	if (!ReadUtf8File (path, content)) {
		outError = FR ("Impossible d'ouvrir le fichier : ") + path;
		return false;
	}

	JsonParser parser (content);
	JsonValue root;
	size_t errorPos = 0;
	if (!parser.Parse (root, errorPos)) {
		outError = FR ("JSON invalide (vers l'octet ")
				   + GS::ToUniString (std::to_wstring (static_cast<int> (errorPos)))
				   + FR (").");
		return false;
	}

	const JsonValue* list = nullptr;
	if (root.type == JsonValue::Type::Array) {
		list = &root;
	} else if (root.type == JsonValue::Type::Object) {
		list = root.Find ("articles");
	}

	if (list == nullptr || list->type != JsonValue::Type::Array) {
		outError = FR ("Format attendu : [{\"id\", \"name\", \"unit\"}, …] ou {\"articles\": […]}.");
		return false;
	}

	for (UIndex i = 0; i < list->arrayValue.size (); ++i) {
		const JsonValue& entry = list->arrayValue[i];
		if (entry.type != JsonValue::Type::Object)
			continue;

		const GS::UniString id = ArticleFieldText (entry, "id");
		if (id.IsEmpty ())
			continue;	// article sans identifiant : ignoré

		outArticles.Push (CWArticle (id, ArticleFieldText (entry, "name"), ArticleFieldText (entry, "unit")));
	}

	if (outArticles.IsEmpty ()) {
		outError = FR ("Aucun article avec un identifiant non vide n'a été trouvé dans le fichier.");
		return false;
	}

	return true;
}


void ArticleManager::CollectChildren (const API_Guid& parentGuid, GS::Array<API_ClassificationItem>& outItems)
{
	GS::Array<API_ClassificationItem> children;
	if (ACAPI_Classification_GetClassificationItemChildren (parentGuid, children) != NoError)
		return;

	for (UIndex i = 0; i < children.GetSize (); ++i) {
		outItems.Push (children[i]);
		CollectChildren (children[i].guid, outItems);
	}
}


void ArticleManager::EnumerateItems (const API_Guid& systemGuid, GS::Array<API_ClassificationItem>& outItems)
{
	outItems.Clear ();

	GS::Array<API_ClassificationItem> roots;
	if (ACAPI_Classification_GetClassificationSystemRootItems (systemGuid, roots) != NoError)
		return;

	for (UIndex i = 0; i < roots.GetSize (); ++i) {
		outItems.Push (roots[i]);
		CollectChildren (roots[i].guid, outItems);
	}
}


bool ArticleManager::CollectFromClassification (const API_Guid& systemGuid, GS::Array<CWArticle>& outArticles)
{
	outArticles.Clear ();

	GS::Array<API_ClassificationItem> items;
	EnumerateItems (systemGuid, items);

	for (UIndex i = 0; i < items.GetSize (); ++i) {
		if (items[i].id.IsEmpty ())
			continue;
		outArticles.Push (CWArticle (items[i].id, items[i].name, GS::UniString ()));
	}

	return true;
}


API_Guid ArticleManager::FindCostWavesSystemGuid ()
{
	GS::Array<API_ClassificationSystem> systems;
	if (ACAPI_Classification_GetClassificationSystems (systems) != NoError)
		return APINULLGuid;

	const GS::UniString name (CostWavesSystemName (), CC_UTF8);
	for (UIndex i = 0; i < systems.GetSize (); ++i) {
		if (systems[i].name == name)
			return systems[i].guid;
	}

	return APINULLGuid;
}


API_Guid ArticleManager::FindItemGuid (const API_Guid& systemGuid, const GS::UniString& articleId)
{
	if (systemGuid == APINULLGuid || articleId.IsEmpty ())
		return APINULLGuid;

	GS::Array<API_ClassificationItem> items;
	EnumerateItems (systemGuid, items);

	for (UIndex i = 0; i < items.GetSize (); ++i) {
		if (items[i].id == articleId)
			return items[i].guid;
	}

	return APINULLGuid;
}


const char* ArticleManager::PropertyGroupName ()
{
	return "CostWaves";
}


const char* ArticleManager::ArticleIdPropertyName ()
{
	return "CW_Article_ID";
}


API_Guid ArticleManager::EnsureCostWavesSystem (GS::UniString& outError)
{
	const API_Guid existing = FindCostWavesSystemGuid ();
	if (existing != APINULLGuid)
		return existing;

	API_ClassificationSystem system;
	system.name = GS::UniString (CostWavesSystemName (), CC_UTF8);
	system.description = FR ("Articles CostWaves (générée par l'Add-On).");
	system.source = GS::UniString (CostWavesSystemName (), CC_UTF8);
	system.editionVersion = FR ("1.0");
	system.editionDate = std::chrono::year_month_day (std::chrono::year (2026),
													  std::chrono::month (1),
													  std::chrono::day (1));

	const GSErrCode err = ACAPI_Classification_CreateClassificationSystem (system);
	if (err != NoError) {
		outError = FR ("Échec de création du système « CostWaves » (code ")
				 + ErrorCodeText (err) + FR (").");
		return APINULLGuid;
	}

	return system.guid;
}


API_Guid ArticleManager::EnsureArticleItem (const API_Guid& systemGuid, const CWArticle& article,
											bool& outCreated, GS::UniString& outError)
{
	outCreated = false;

	const API_Guid existing = FindItemGuid (systemGuid, article.id);
	if (existing != APINULLGuid)
		return existing;

	API_ClassificationItem item;
	item.id = article.id;
	item.name = article.name;
	const GSErrCode err = ACAPI_Classification_CreateClassificationItem (item, systemGuid,
																		 APINULLGuid, APINULLGuid);
	if (err == NoError) {
		outCreated = true;
		return item.guid;
	}
	if (err == APIERR_NAMEALREADYUSED) {
		return FindItemGuid (systemGuid, article.id);
	}

	outError = FR ("Échec de création de l'item « ") + article.id
			 + FR (" » (code ") + ErrorCodeText (err) + FR (").");
	return APINULLGuid;
}


GSErrCode ArticleManager::EnsureCostWavesClassification (const GS::Array<CWArticle>& articles,
														 API_Guid& outSystemGuid, USize& outCreatedItems,
														 GS::UniString& outError)
{
	outSystemGuid = APINULLGuid;
	outCreatedItems = 0;

	if (articles.IsEmpty ()) {
		outError = FR ("Aucun article disponible — importez des articles ou choisissez un système.");
		return APIERR_GENERAL;
	}

	API_Guid		systemGuid = APINULLGuid;
	USize			createdItems = 0;
	GS::UniString	errorNote;

	const GSErrCode result = ACAPI_CallUndoableCommand (FR ("CostWaves : création de la classification"),
		[&]() -> GSErrCode {
			systemGuid = EnsureCostWavesSystem (errorNote);
			if (systemGuid == APINULLGuid)
				return APIERR_GENERAL;

			for (UIndex a = 0; a < articles.GetSize (); ++a) {
				bool created = false;
				const API_Guid itemGuid = EnsureArticleItem (systemGuid, articles[a], created, errorNote);
				if (itemGuid == APINULLGuid)
					return APIERR_GENERAL;
				if (created)
					++createdItems;
			}

			return NoError;
		});

	outSystemGuid = systemGuid;
	outCreatedItems = createdItems;

	if (result != NoError && outError.IsEmpty ()) {
		outError = errorNote.IsEmpty ()
			? FR ("Création de la classification impossible (code ") + ErrorCodeText (result) + FR (").")
			: errorNote;
	}

	return result;
}


API_Guid ArticleManager::EnsureArticleIdProperty (GS::UniString& outError)
{
	// 1) Groupe « CostWaves ».
	API_Guid groupGuid = APINULLGuid;

	const GS::UniString groupName (PropertyGroupName (), CC_UTF8);

	GS::Array<API_PropertyGroup> groups;
	if (ACAPI_Property_GetPropertyGroups (groups) == NoError) {
		for (UIndex i = 0; i < groups.GetSize (); ++i) {
			if (groups[i].name == groupName) {
				groupGuid = groups[i].guid;
				break;
			}
		}
	}

	if (groupGuid == APINULLGuid) {
		API_PropertyGroup group;
		group.name = groupName;
		group.description = FR ("Propriétés CostWaves (générées par l'Add-On).");

		const GSErrCode err = ACAPI_Property_CreatePropertyGroup (group);
		if (err == NoError) {
			groupGuid = group.guid;
		} else {
			// NAMEALREADYUSED (créé entre-temps) ou erreur : retenter la recherche.
			groups.Clear ();
			if (ACAPI_Property_GetPropertyGroups (groups) == NoError) {
				for (UIndex i = 0; i < groups.GetSize (); ++i) {
					if (groups[i].name == groupName)
						groupGuid = groups[i].guid;
				}
			}
		}

		if (groupGuid == APINULLGuid) {
			outError = FR ("Impossible de créer le groupe de propriétés « CostWaves ».");
			return APINULLGuid;
		}
	}

	// 2) Définition « CW_Article_ID » dans ce groupe.
	const GS::UniString propertyName (ArticleIdPropertyName (), CC_UTF8);

	GS::Array<API_PropertyDefinition> definitions;
	if (ACAPI_Property_GetPropertyDefinitions (groupGuid, definitions) == NoError) {
		for (UIndex i = 0; i < definitions.GetSize (); ++i) {
			if (definitions[i].name == propertyName)
				return definitions[i].guid;
		}
	}

	API_PropertyDefinition definition;
	definition.definitionType = API_PropertyCustomDefinitionType;
	definition.groupGuid = groupGuid;
	definition.name = propertyName;
	definition.description = FR ("Identifiant de l'article CostWaves affecté à l'élément.");
	definition.valueType = API_PropertyStringValueType;
	definition.collectionType = API_PropertySingleCollectionType;
	definition.measureType = API_PropertyDefaultMeasureType;

	const GSErrCode err = ACAPI_Property_CreatePropertyDefinition (definition);
	if (err == NoError)
		return definition.guid;

	// NAMEALREADYUSED : retenter la recherche.
	definitions.Clear ();
	if (ACAPI_Property_GetPropertyDefinitions (groupGuid, definitions) == NoError) {
		for (UIndex i = 0; i < definitions.GetSize (); ++i) {
			if (definitions[i].name == propertyName)
				return definitions[i].guid;
		}
	}

	outError = FR ("Impossible de créer la propriété « CW_Article_ID » (code ")
			 + ErrorCodeText (err) + FR (").");
	return APINULLGuid;
}


GSErrCode ArticleManager::AssignArticleToElements (const GS::Array<API_Guid>& elemGuids,
												   const API_Guid& systemGuid, const API_Guid& itemGuid,
												   const GS::UniString& articleId, const API_Guid& articleIdPropGuid,
												   USize& outChangedCount, USize& outFailedCount,
												   GS::UniString& outError)
{
	outChangedCount = 0;
	outFailedCount = 0;

	if (elemGuids.IsEmpty ()) {
		outError = FR ("Aucun élément à traiter.");
		return APIERR_GENERAL;
	}

	USize			changedCount = 0;
	USize			failedCount = 0;
	GS::UniString	firstError;

	const GSErrCode result = ACAPI_CallUndoableCommand (FR ("CostWaves : affectation d'articles"),
		[&]() -> GSErrCode {
			for (UIndex i = 0; i < elemGuids.GetSize (); ++i) {
				const API_Guid& elemGuid = elemGuids[i];
				bool			elementChanged = false;
				GSErrCode		step = NoError;

				// 1) Classification dans le système.
				API_ClassificationItem current;
				const GSErrCode getErr = ACAPI_Element_GetClassificationInSystem (elemGuid, systemGuid, current);
				if (getErr != NoError || current.guid != itemGuid) {
					if (getErr == NoError && current.guid != APINULLGuid) {
						step = ACAPI_Element_RemoveClassificationItem (elemGuid, current.guid);
						if (step == NoError)
							elementChanged = true;
					}

					if (step == NoError) {
						step = ACAPI_Element_AddClassificationItem (elemGuid, itemGuid);
						if (step == NoError)
							elementChanged = true;
					}
				}

				// 2) Propriété CW_Article_ID (best effort : ne compte pas comme échec).
				if (step == NoError && articleIdPropGuid != APINULLGuid && !articleId.IsEmpty ()) {
					API_Property property;
					property.definition.guid = articleIdPropGuid;
					property.isDefault = false;
					property.value.singleVariant.variant.type = API_PropertyStringValueType;
					property.value.singleVariant.variant.uniStringValue = articleId;

					if (ACAPI_Element_SetProperty (elemGuid, property) == NoError)
						elementChanged = true;
				}

				if (step != NoError) {
					++failedCount;
					if (firstError.IsEmpty ())
						firstError = FR ("Élément ") + APIGuidToString (elemGuid)
								   + FR (" : code ") + ErrorCodeText (step) + FR (".");
				} else if (elementChanged) {
					++changedCount;
				}
			}

			// Les échecs individuels sont comptés : la commande réussit
			// globalement dès qu'au moins une affectation a passé.
			return NoError;
		});

	outChangedCount = changedCount;
	outFailedCount = failedCount;

	if (changedCount == 0 && failedCount > 0) {
		outError = firstError.IsEmpty ()
			? FR ("Affectation impossible.")
			: firstError;
		return APIERR_GENERAL;
	}

	return result;
}


GSErrCode ArticleManager::CreateBuildingMaterials (const GS::Array<CWArticle>& articles,
												   API_Guid& outSystemGuid,
												   USize& outCreatedMaterials, USize& outCreatedItems,
												   USize& outAssigned,
												   GS::UniString& outError)
{
	outSystemGuid = APINULLGuid;
	outCreatedMaterials = 0;
	outCreatedItems = 0;
	outAssigned = 0;

	if (articles.IsEmpty ()) {
		outError = FR ("Aucun article disponible — importez des articles ou choisissez un système.");
		return APIERR_GENERAL;
	}

	API_Guid		systemGuid = APINULLGuid;
	USize			createdMaterials = 0;
	USize			createdItems = 0;
	USize			assigned = 0;
	GS::UniString	errorNote;

	// NB : la création d'attributs n'est pas annulable (limite de l'API) ;
	// la partie classification est regroupée dans une commande annulable.
	const GSErrCode result = ACAPI_CallUndoableCommand (FR ("CostWaves : création des matériaux"),
		[&]() -> GSErrCode {
			systemGuid = EnsureCostWavesSystem (errorNote);
			if (systemGuid == APINULLGuid)
				return APIERR_GENERAL;

			for (UIndex a = 0; a < articles.GetSize (); ++a) {
				const CWArticle& article = articles[a];

				// 1) Item de classification de l'article.
				bool itemCreated = false;
				const API_Guid itemGuid = EnsureArticleItem (systemGuid, article, itemCreated, errorNote);
				if (itemGuid == APINULLGuid)
					return APIERR_GENERAL;
				if (itemCreated)
					++createdItems;

				// 2) Matériau de construction « id — nom ». Idempotent : si un
				//    matériau de ce nom existe déjà, Create renvoie son index.
				GS::UniString materialName = article.id + US (" — ") + article.name;

				API_Attribute attribute;
				BNZeroMemory (&attribute, sizeof (attribute));
				attribute.header.typeID = API_BuildingMaterialID;
				attribute.header.uniStringNamePtr = &materialName;

				API_AttributeDef defs;
				BNZeroMemory (&defs, sizeof (defs));

				const GSErrCode err = ACAPI_Attribute_Create (&attribute, &defs);
				ACAPI_DisposeAttrDefsHdls (&defs);

				if (err != NoError) {
					errorNote = FR ("Impossible de créer le matériau « ") + materialName
							 + FR (" » (code ") + ErrorCodeText (err) + FR (").");
					return err;
				}
				++createdMaterials;

				// 3) Affecter l'item au matériau (remplace sa classe précédente
				//    dans ce système).
				API_ClassificationItem current;
				const GSErrCode getErr = ACAPI_Attribute_GetClassificationInSystem (attribute.header,
																					systemGuid, current);
				if (getErr == NoError && current.guid == itemGuid)
					continue;		// déjà affecté

				if (getErr == NoError && current.guid != APINULLGuid) {
					const GSErrCode removeErr = ACAPI_Attribute_RemoveClassificationItem (attribute.header,
																						  current.guid);
					if (removeErr != NoError) {
						errorNote = FR ("Impossible de retirer la classe précédente du matériau « ")
								 + materialName + FR (" » (code ") + ErrorCodeText (removeErr) + FR (").");
						return removeErr;
					}
				}

				const GSErrCode addErr = ACAPI_Attribute_AddClassificationItem (attribute.header, itemGuid);
				if (addErr != NoError) {
					errorNote = FR ("Impossible d'affecter la classe au matériau « ")
							 + materialName + FR (" » (code ") + ErrorCodeText (addErr) + FR (").");
					return addErr;
				}
				++assigned;
			}

			return NoError;
		});

	outSystemGuid = systemGuid;
	outCreatedMaterials = createdMaterials;
	outCreatedItems = createdItems;
	outAssigned = assigned;

	if (result != NoError && outError.IsEmpty ()) {
		outError = errorNote.IsEmpty ()
			? FR ("Création des matériaux impossible (code ") + ErrorCodeText (result) + FR (").")
			: errorNote;
	}

	return result;
}

} // namespace CostWaves
