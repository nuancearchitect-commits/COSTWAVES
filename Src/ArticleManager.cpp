#include "CostWavesPrecompiledHeader.hpp"

#include "ArticleManager.hpp"

#include "Exporter.hpp"
#include "RuleLibrary.hpp"

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

// Littéral -> GS::UniString (départ de chaîne pour l'opérateur +).
GS::UniString US (const char* utf8Text)
{
	return GS::UniString (utf8Text, CC_UTF8);
}

GS::UniString ErrorCodeText (GSErrCode err)
{
	return GS::ToUniString (std::to_wstring (static_cast<int> (err)));
}

// Préfixe des groupes numérotés dans CW_Group_ID ("CW-N-12" = groupe n° 12).
// Les autres valeurs non vides ("CW-E-…", "CW-G-…" historiques) désignent des ensembles.
const char* NumberedGroupPrefix = "CW-N-";

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


// Parcours récursif des chapitres / sous-chapitres de la base CostWaves
// (spec §2) : chaque article porte le chemin de son chapitre et une profondeur
// d'indentation correspondant au niveau du chapitre.
void CollectChapterArticles (const JsonValue& chapters, const GS::UniString& parentPath,
							 short depth, GS::Array<CWArticle>& outArticles)
{
	for (UIndex c = 0; c < chapters.arrayValue.size (); ++c) {
		const JsonValue& chapter = chapters.arrayValue[c];
		if (chapter.type != JsonValue::Type::Object)
			continue;

		GS::UniString chapterName = ArticleFieldText (chapter, "nom");
		if (chapterName.IsEmpty ())
			chapterName = ArticleFieldText (chapter, "name");
		const GS::UniString chapterId = ArticleFieldText (chapter, "id");
		if (!chapterName.IsEmpty () && !chapterId.IsEmpty ())
			chapterName = chapterId + US (" - ") + chapterName;

		const GS::UniString chapterPath = chapterName.IsEmpty ()
			? parentPath
			: (parentPath.IsEmpty () ? chapterName : parentPath + US (" / ") + chapterName);

		const JsonValue* articles = chapter.Find ("articles");
		if (articles != nullptr && articles->type == JsonValue::Type::Array) {
			for (UIndex a = 0; a < articles->arrayValue.size (); ++a) {
				const JsonValue& entry = articles->arrayValue[a];
				if (entry.type != JsonValue::Type::Object)
					continue;
				const GS::UniString id = ArticleFieldText (entry, "id");
				if (id.IsEmpty ())
					continue;

				CWArticle article (id, ArticleFieldText (entry, "name"), ArticleFieldText (entry, "unit"));
				article.calcQuantity = ArticleFieldText (entry, "calcQuantity");
				if (article.calcQuantity.IsEmpty ())
					article.calcQuantity = ArticleFieldText (entry, "quantity");
				article.calcFormula = ArticleFieldText (entry, "calcFormula");
				if (article.calcFormula.IsEmpty ())
					article.calcFormula = ArticleFieldText (entry, "formula");
				article.chapter = chapterPath;
				article.depth = depth;
				outArticles.Push (article);
			}
		}

		const JsonValue* subChapters = chapter.Find ("chapitres");
		if (subChapters != nullptr && subChapters->type == JsonValue::Type::Array)
			CollectChapterArticles (*subChapters, chapterPath, static_cast<short> (depth + 1), outArticles);
	}
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

	// Base CostWaves (nouvelle architecture, spec §2) : chapitres imbriqués.
	// {"chapitres": [{"nom": "02 Murs", "articles": […], "chapitres": […]}]}
	const JsonValue* chapters = (root.type == JsonValue::Type::Object) ? root.Find ("chapitres") : nullptr;
	if (chapters != nullptr && chapters->type == JsonValue::Type::Array) {
		CollectChapterArticles (*chapters, GS::UniString (), 0, outArticles);
		if (outArticles.IsEmpty ()) {
			outError = FR ("Aucun article trouvé dans les chapitres du fichier.");
			return false;
		}
		return true;
	}

	const JsonValue* list = nullptr;
	if (root.type == JsonValue::Type::Array) {
		list = &root;
	} else if (root.type == JsonValue::Type::Object) {
		list = root.Find ("articles");
	}

	if (list == nullptr || list->type != JsonValue::Type::Array) {
		outError = FR ("Format attendu : {\"chapitres\": […]} (base CostWaves), {\"articles\": […]} ou [{\"id\", \"name\", \"unit\"}, …].");
		return false;
	}

	for (UIndex i = 0; i < list->arrayValue.size (); ++i) {
		const JsonValue& entry = list->arrayValue[i];
		if (entry.type != JsonValue::Type::Object)
			continue;

		const GS::UniString id = ArticleFieldText (entry, "id");
		if (id.IsEmpty ())
			continue;	// article sans identifiant : ignoré

		CWArticle article (id, ArticleFieldText (entry, "name"), ArticleFieldText (entry, "unit"));
		// Règle de calcul optionnelle (fenêtre « Règles de calcul ») :
		// "calcQuantity" ou l'alias court "quantity".
		article.calcQuantity = ArticleFieldText (entry, "calcQuantity");
		if (article.calcQuantity.IsEmpty ())
			article.calcQuantity = ArticleFieldText (entry, "quantity");
		// Formule dérivée optionnelle (ex. "calcFormula": "Contour ouverture * Épaisseur mur hôte").
		article.calcFormula = ArticleFieldText (entry, "calcFormula");
		if (article.calcFormula.IsEmpty ())
			article.calcFormula = ArticleFieldText (entry, "formula");
		article.chapter = ArticleFieldText (entry, "chapter");
		outArticles.Push (article);
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

	// Collecte hiérarchique : les articles gardent leur profondeur pour
	// l'affichage indenté (listes « comme la classification »).
	GS::Array<API_ClassificationItem> items;
	GS::Array<short> depths;
	CollectItems (systemGuid, items, depths);

	for (UIndex i = 0; i < items.GetSize (); ++i) {
		if (items[i].id.IsEmpty ())
			continue;
		CWArticle article (items[i].id, items[i].name, GS::UniString ());
		article.depth = (i < depths.GetSize () ? depths[i] : 0);
		outArticles.Push (article);
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


API_Guid ArticleManager::EnsureTextProperty (const char* nameUtf8, const GS::UniString& description,
											  GS::UniString& outError)
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

	// 2) Définition dans ce groupe.
	const GS::UniString propertyName (nameUtf8, CC_UTF8);

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
	definition.description = description;
	definition.valueType = API_PropertyStringValueType;
	definition.collectionType = API_PropertySingleCollectionType;
	definition.measureType = API_PropertyDefaultMeasureType;

	// Valeur par défaut EXPLICITE (chaîne vide) : sans cela le variant par
	// défaut reste de type « indéfini », ce qui peut faire échouer la
	// création — l'exemple Property_Test du DevKit définit toujours la
	// valeur par défaut en cohérence avec le type de la propriété.
	definition.defaultValue.hasExpression = false;
	definition.defaultValue.basicValue.variantStatus = API_VariantStatusNormal;
	definition.defaultValue.basicValue.singleVariant.variant.type = API_PropertyStringValueType;
	definition.defaultValue.basicValue.singleVariant.variant.uniStringValue = GS::UniString ();

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

	outError = FR ("Impossible de créer la propriété « ") + propertyName
			 + FR (" » (code ") + ErrorCodeText (err) + FR (").");
	return APINULLGuid;
}


API_Guid ArticleManager::EnsureArticleIdProperty (GS::UniString& outError)
{
	return EnsureTextProperty (ArticleIdPropertyName (),
							   FR ("Identifiant de l'article CostWaves affecté à l'élément."),
							   outError);
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

				// 2) Propriété CW_Article_ID (best effort : ne compte pas comme
				//    échec). Posée même si la classification a échoué — c'est
				//    le repli de classe des dessins 2D (spec §8/§9).
				if (articleIdPropGuid != APINULLGuid && !articleId.IsEmpty ()) {
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


GSErrCode ArticleManager::CreateClassificationItem (const API_Guid& systemGuid, const API_Guid& parentItemGuid,
													const GS::UniString& itemId, const GS::UniString& itemName,
													API_Guid& outItemGuid, GS::UniString& outError)
{
	outItemGuid = APINULLGuid;

	if (itemId.IsEmpty ()) {
		outError = FR ("L'identifiant de la classe est vide.");
		return APIERR_BADNAME;
	}

	API_ClassificationItem item;
	item.id = itemId;
	item.name = itemName.IsEmpty () ? itemId : itemName;
	const GSErrCode err = ACAPI_Classification_CreateClassificationItem (item, systemGuid, parentItemGuid, APINULLGuid);
	if (err == NoError) {
		outItemGuid = item.guid;
		return NoError;
	}
	if (err == APIERR_NAMEALREADYUSED) {
		outError = FR ("L'identifiant de classe « ") + itemId
				 + FR (" » est déjà utilisé dans ce système.");
		return err;
	}

	outError = FR ("Échec de création de la classe « ") + itemId
			 + FR (" » (code ") + ErrorCodeText (err) + FR (").");
	return err;
}


namespace {

// Identifiants purement numériques ("1", "2", "10"…) d'une liste d'items.
// Les identifiants texte sont ignorés.
GS::Array<int> NumericIdsOf (const GS::Array<API_ClassificationItem>& items)
{
	GS::Array<int> result;
	for (UIndex i = 0; i < items.GetSize (); ++i) {
		const std::wstring text = GS::ToWString (items[i].id);
		if (text.empty ())
			continue;

		bool numeric = true;
		for (UIndex c = 0; c < text.length (); ++c) {
			if (text[c] < L'0' || text[c] > L'9') {
				numeric = false;
				break;
			}
		}
		if (numeric)
			result.Push (std::stoi (text));
	}
	return result;
}

} // namespace


GS::UniString ArticleManager::FirstAvailableChildId (const API_Guid& systemGuid, const API_Guid& parentItemGuid)
{
	GS::Array<API_ClassificationItem> children;
	if (parentItemGuid == APINULLGuid)
		ACAPI_Classification_GetClassificationSystemRootItems (systemGuid, children);
	else
		ACAPI_Classification_GetClassificationItemChildren (parentItemGuid, children);

	const GS::Array<int> used = NumericIdsOf (children);

	// Premier entier libre parmi les enfants ET dans tout le système.
	for (int candidate = 1; candidate < 100000; ++candidate) {
		bool takenByChild = false;
		for (UIndex i = 0; i < used.GetSize (); ++i) {
			if (used[i] == candidate) {
				takenByChild = true;
				break;
			}
		}
		if (takenByChild)
			continue;

		const GS::UniString candidateText = GS::ToUniString (std::to_wstring (candidate));
		if (FindItemGuid (systemGuid, candidateText) != APINULLGuid)
			continue;	// pris ailleurs dans le système

		return candidateText;
	}

	return GS::UniString ("1");
}


namespace {

void CollectItemsRecursive (const API_ClassificationItem& item, short depth,
							GS::Array<API_ClassificationItem>& outItems, GS::Array<short>& outDepths)
{
	outItems.Push (item);
	outDepths.Push (depth);

	GS::Array<API_ClassificationItem> children;
	if (ACAPI_Classification_GetClassificationItemChildren (item.guid, children) != NoError)
		return;

	for (UIndex i = 0; i < children.GetSize (); ++i)
		CollectItemsRecursive (children[i], static_cast<short> (depth + 1), outItems, outDepths);
}

} // namespace


bool ArticleManager::CollectItems (const API_Guid& systemGuid,
								   GS::Array<API_ClassificationItem>& outItems,
								   GS::Array<short>& outDepths)
{
	outItems.Clear ();
	outDepths.Clear ();

	GS::Array<API_ClassificationItem> roots;
	if (ACAPI_Classification_GetClassificationSystemRootItems (systemGuid, roots) != NoError)
		return false;

	for (UIndex i = 0; i < roots.GetSize (); ++i)
		CollectItemsRecursive (roots[i], 0, outItems, outDepths);

	return true;
}


GSErrCode ArticleManager::CreateMaterialWithClass (const GS::UniString& materialName,
												   const CWMaterialAttributes& attributes,
												   bool createNewClass,
												   const API_Guid& systemGuid,
												   const API_Guid& parentItemGuid,
												   const GS::UniString& classId,
												   const GS::UniString& className,
												   const API_Guid& existingItemGuid,
												   API_Guid& outItemGuid,
												   bool& outMaterialCreated,
												   bool& outClassCreated,
												   GS::UniString& outError)
{
	outItemGuid = APINULLGuid;
	outMaterialCreated = false;
	outClassCreated = false;

	if (materialName.IsEmpty ()) {
		outError = FR ("Le nom du matériau est vide.");
		return APIERR_BADNAME;
	}

	if (systemGuid == APINULLGuid || (!createNewClass && existingItemGuid == APINULLGuid)) {
		outError = FR ("Aucune classe cible : choisissez une classe existante ou créez-en une nouvelle.");
		return APIERR_BADID;
	}

	API_Guid		itemGuid = APINULLGuid;
	bool			classCreated = false;
	bool			materialCreated = false;
	GS::UniString	errorNote;

	// NB : la création d'attributs n'est pas annulable (limite de l'API) ; la
	// partie classification est regroupée dans la même commande annulable.
	const GSErrCode result = ACAPI_CallUndoableCommand (FR ("CostWaves : création d'un matériau"),
		[&]() -> GSErrCode {
			// 1) Classe de classification (nouvelle ou existante).
			if (createNewClass) {
				const GSErrCode classErr = CreateClassificationItem (systemGuid, parentItemGuid,
																	 classId, className, itemGuid, errorNote);
				if (classErr != NoError)
					return classErr;
				classCreated = true;
			} else {
				itemGuid = existingItemGuid;
			}

			// 2) Matériau de construction : créé s'il n'existe pas, mis à
			//    jour sinon (recherche par nom).
			GS::UniString nameCopy = materialName;

			API_Attr_Head searchHead;
			BNZeroMemory (&searchHead, sizeof (searchHead));
			searchHead.typeID = API_BuildingMaterialID;
			searchHead.uniStringNamePtr = &nameCopy;

			bool exists = false;
			if (ACAPI_Attribute_Search (&searchHead) == NoError && searchHead.index.IsPositive ())
				exists = true;

			API_Attribute attribute;
			BNZeroMemory (&attribute, sizeof (attribute));
			attribute.header.typeID = API_BuildingMaterialID;
			attribute.header.index = exists ? searchHead.index : APIInvalidAttributeIndex;
			attribute.header.uniStringNamePtr = &nameCopy;
			attribute.buildingMaterial.connPriority = attributes.connPriority;
			attribute.buildingMaterial.cutFill = attributes.cutFill;
			attribute.buildingMaterial.cutFillPen = attributes.cutFillPen;
			attribute.buildingMaterial.cutFillBackgroundPen = attributes.cutFillBackgroundPen;
			attribute.buildingMaterial.cutMaterial = attributes.cutMaterial;

			API_AttributeDef defs;
			BNZeroMemory (&defs, sizeof (defs));

			GSErrCode err;
			if (exists)
				err = ACAPI_Attribute_Modify (&attribute, &defs);
			else
				err = ACAPI_Attribute_Create (&attribute, &defs);
			ACAPI_DisposeAttrDefsHdls (&defs);

			if (err != NoError) {
				errorNote = FR ("Impossible de créer le matériau « ") + materialName
						 + FR (" » (code ") + ErrorCodeText (err) + FR (").");
				return err;
			}
			materialCreated = !exists;

			// 3) Affectation de la classe au matériau (remplace la classe
			//    précédente dans ce système).
			API_ClassificationItem current;
			const GSErrCode getErr = ACAPI_Attribute_GetClassificationInSystem (attribute.header,
																				systemGuid, current);
			if (getErr == NoError && current.guid == itemGuid)
				return NoError;		// déjà affectée

			if (getErr == NoError && current.guid != APINULLGuid) {
				const GSErrCode removeErr = ACAPI_Attribute_RemoveClassificationItem (attribute.header, current.guid);
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

			return NoError;
		});

	outItemGuid = itemGuid;
	outMaterialCreated = materialCreated;
	outClassCreated = classCreated;

	if (result != NoError && outError.IsEmpty ()) {
		outError = errorNote.IsEmpty ()
			? FR ("Création du matériau impossible (code ") + ErrorCodeText (result) + FR (").")
			: errorNote;
	}

	return result;
}

// --- Phase 4 : ensembles CostWaves --------------------------------------------

namespace {

// Normalise une unité pour comparaison : majuscules, "m²" -> "M2", "m³" -> "M3".
GS::UniString NormalizeUnit (const GS::UniString& unit)
{
	std::wstring text = GS::ToWString (unit.ToUpperCase ());
	for (wchar_t& ch : text) {
		if (ch == L'\u00B2')
			ch = L'2';
		else if (ch == L'\u00B3')
			ch = L'3';
	}
	return GS::ToUniString (text);
}

// Ligne d'un élément par GUID (nullptr si absente).
const CWElementRow* FindRowByGuid (const GS::Array<CWElementRow>& rows, const API_Guid& guid)
{
	for (UIndex i = 0; i < rows.GetSize (); ++i) {
		if (rows[i].guid == guid)
			return &rows[i];
	}
	return nullptr;
}

} // namespace


const char* ArticleManager::GroupPropertyName ()
{
	return "CW_Group_ID";
}


API_Guid ArticleManager::EnsureGroupIdProperty (GS::UniString& outError)
{
	return EnsureTextProperty (GroupPropertyName (),
							   FR ("Identifiant de l'ensemble CostWaves auquel appartient l'élément (les membres d'un ensemble sont facturés via l'ensemble)."),
							   outError);
}


API_Guid ArticleManager::FindArticleIdPropertyGuid ()
{
	const GS::UniString groupName (PropertyGroupName (), CC_UTF8);
	const GS::UniString propertyName (ArticleIdPropertyName (), CC_UTF8);

	GS::Array<API_PropertyGroup> groups;
	if (ACAPI_Property_GetPropertyGroups (groups) != NoError)
		return APINULLGuid;

	for (UIndex g = 0; g < groups.GetSize (); ++g) {
		if (groups[g].name != groupName)
			continue;

		GS::Array<API_PropertyDefinition> definitions;
		if (ACAPI_Property_GetPropertyDefinitions (groups[g].guid, definitions) == NoError) {
			for (UIndex d = 0; d < definitions.GetSize (); ++d) {
				if (definitions[d].name == propertyName)
					return definitions[d].guid;
			}
		}
	}

	return APINULLGuid;
}


API_Guid ArticleManager::FindGroupIdPropertyGuid ()
{
	const GS::UniString groupName (PropertyGroupName (), CC_UTF8);
	const GS::UniString propertyName (GroupPropertyName (), CC_UTF8);

	GS::Array<API_PropertyGroup> groups;
	if (ACAPI_Property_GetPropertyGroups (groups) != NoError)
		return APINULLGuid;

	for (UIndex g = 0; g < groups.GetSize (); ++g) {
		if (groups[g].name != groupName)
			continue;

		GS::Array<API_PropertyDefinition> definitions;
		if (ACAPI_Property_GetPropertyDefinitions (groups[g].guid, definitions) == NoError) {
			for (UIndex d = 0; d < definitions.GetSize (); ++d) {
				if (definitions[d].name == propertyName)
					return definitions[d].guid;
			}
		}
	}

	return APINULLGuid;
}


GS::UniString ArticleManager::GenerateGroupId (const GS::Array<GS::UniString>& existingIds)
{
	const std::time_t now = std::time (nullptr);
	std::tm localTime;
#if defined (WINDOWS)
	localtime_s (&localTime, &now);
#else
	localtime_r (&now, &localTime);
#endif

	wchar_t buffer[32];
	swprintf (buffer, 32, L"CW-E-%04d%02d%02d-%02d%02d%02d",
			  localTime.tm_year + 1900, localTime.tm_mon + 1, localTime.tm_mday,
			  localTime.tm_hour, localTime.tm_min, localTime.tm_sec);
	GS::UniString base = GS::ToUniString (std::wstring (buffer));

	// Suffixe numérique si l'identifiant est déjà pris.
	GS::UniString candidate = base;
	for (USize suffix = 2; ; ++suffix) {
		bool taken = false;
		for (UIndex i = 0; i < existingIds.GetSize (); ++i) {
			if (existingIds[i] == candidate) {
				taken = true;
				break;
			}
		}
		if (!taken)
			return candidate;
		candidate = base + "-" + GS::ToUniString (std::to_wstring (static_cast<int> (suffix)));
	}
}


bool ArticleManager::IsNumberedGroupValue (const GS::UniString& groupValue)
{
	int number = 0;
	return ParseNumberedGroupValue (groupValue, number);
}


bool ArticleManager::ParseNumberedGroupValue (const GS::UniString& groupValue, int& outNumber)
{
	outNumber = 0;

	const std::wstring prefix = GS::ToWString (GS::UniString (NumberedGroupPrefix, CC_UTF8));
	const std::wstring text = GS::ToWString (groupValue);
	if (text.length () <= prefix.length ())
		return false;

	if (text.compare (0, prefix.length (), prefix) != 0)
		return false;

	const std::wstring numberText = text.substr (prefix.length ());
	if (numberText.empty ())
		return false;

	for (UIndex i = 0; i < numberText.length (); ++i) {
		if (numberText[i] < L'0' || numberText[i] > L'9')
			return false;
	}

	outNumber = std::stoi (numberText);
	return outNumber > 0;
}


GS::UniString ArticleManager::NumberedGroupValue (int number)
{
	return GS::UniString (NumberedGroupPrefix, CC_UTF8) + GS::ToUniString (std::to_wstring (number));
}


int ArticleManager::NextGroupNumber (const GS::Array<GS::UniString>& existingValues)
{
	int maxNumber = 0;
	for (UIndex i = 0; i < existingValues.GetSize (); ++i) {
		int number = 0;
		if (ParseNumberedGroupValue (existingValues[i], number) && number > maxNumber)
			maxNumber = number;
	}
	return maxNumber + 1;
}


GSErrCode ArticleManager::CreateGroupFromElements (const GS::Array<API_Guid>& elemGuids,
												   const API_Guid& systemGuid, const API_Guid& itemGuid,
												   const GS::UniString& articleId, const API_Guid& articleIdPropGuid,
												   const GS::UniString& groupId, const API_Guid& groupIdPropGuid,
												   const GS::UniString& undoTitle,
												   USize& outChangedCount, USize& outFailedCount,
												   GS::UniString& outError)
{
	outChangedCount = 0;
	outFailedCount = 0;

	if (elemGuids.IsEmpty ()) {
		outError = FR ("Aucun élément à traiter.");
		return APIERR_GENERAL;
	}

	if (groupIdPropGuid == APINULLGuid) {
		outError = FR ("Propriété CW_Group_ID indisponible.");
		return APIERR_GENERAL;
	}

	USize	 changedCount = 0;
	USize	 failedCount = 0;
	GS::UniString firstError;

	const GSErrCode result = ACAPI_CallUndoableCommand (undoTitle.IsEmpty () ? FR ("CostWaves : création d'un groupe") : undoTitle,
		[&]() -> GSErrCode {
			for (UIndex i = 0; i < elemGuids.GetSize (); ++i) {
				const API_Guid& elemGuid = elemGuids[i];
				bool		elementChanged = false;
				GSErrCode	step = NoError;

				// 1) Classification de l'article (comme AssignArticleToElements).
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

				// 2) Propriété CW_Article_ID (best effort).
				if (step == NoError && articleIdPropGuid != APINULLGuid && !articleId.IsEmpty ()) {
					API_Property property;
					property.definition.guid = articleIdPropGuid;
					property.isDefault = false;
					property.value.singleVariant.variant.type = API_PropertyStringValueType;
					property.value.singleVariant.variant.uniStringValue = articleId;

					if (ACAPI_Element_SetProperty (elemGuid, property) == NoError)
						elementChanged = true;
				}

				// 3) Propriété CW_Group_ID (l'appartenance à l'ensemble).
				if (step == NoError) {
					API_Property property;
					property.definition.guid = groupIdPropGuid;
					property.isDefault = false;
					property.value.singleVariant.variant.type = API_PropertyStringValueType;
					property.value.singleVariant.variant.uniStringValue = groupId;

					step = ACAPI_Element_SetProperty (elemGuid, property);
					if (step == NoError)
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
			// globalement dès qu'au moins un élément a passé.
			return NoError;
		});

	outChangedCount = changedCount;
	outFailedCount = failedCount;

	if (changedCount == 0 && failedCount > 0) {
		outError = firstError.IsEmpty () ? FR ("Création de l'ensemble impossible.") : firstError;
		return APIERR_GENERAL;
	}

	return result;
}


GSErrCode ArticleManager::DissolveGroupFromElements (const GS::Array<API_Guid>& elemGuids,
													 const API_Guid& groupIdPropGuid,
													 USize& outChangedCount, USize& outFailedCount,
													 GS::UniString& outError)
{
	outChangedCount = 0;
	outFailedCount = 0;

	if (elemGuids.IsEmpty ()) {
		outError = FR ("Aucun élément à traiter.");
		return APIERR_GENERAL;
	}

	if (groupIdPropGuid == APINULLGuid) {
		outError = FR ("Propriété CW_Group_ID indisponible.");
		return APIERR_GENERAL;
	}

	USize	 changedCount = 0;
	USize	 failedCount = 0;
	GS::UniString firstError;

	const GSErrCode result = ACAPI_CallUndoableCommand (FR ("CostWaves : dissolution d'un ensemble"),
		[&]() -> GSErrCode {
			for (UIndex i = 0; i < elemGuids.GetSize (); ++i) {
				const API_Guid& elemGuid = elemGuids[i];

				// CW_Group_ID vidée : l'élément quitte l'ensemble (la classe et
				// CW_Article_ID sont conservées).
				API_Property property;
				property.definition.guid = groupIdPropGuid;
				property.isDefault = false;
				property.value.singleVariant.variant.type = API_PropertyStringValueType;
				property.value.singleVariant.variant.uniStringValue = GS::UniString ();

				const GSErrCode step = ACAPI_Element_SetProperty (elemGuid, property);
				if (step != NoError) {
					++failedCount;
					if (firstError.IsEmpty ())
						firstError = FR ("Élément ") + APIGuidToString (elemGuid)
								   + FR (" : code ") + ErrorCodeText (step) + FR (".");
				} else {
					++changedCount;
				}
			}

			return NoError;
		});

	outChangedCount = changedCount;
	outFailedCount = failedCount;

	if (changedCount == 0 && failedCount > 0) {
		outError = firstError.IsEmpty () ? FR ("Dissolution de l'ensemble impossible.") : firstError;
		return APIERR_GENERAL;
	}

	return result;
}


double ArticleManager::QuantityForUnit (const GS::Array<CWQuantity>& quantities, const GS::UniString& unit)
{
	const GS::UniString wanted = NormalizeUnit (unit);
	for (UIndex q = 0; q < quantities.GetSize (); ++q) {
		if (NormalizeUnit (quantities[q].unit) == wanted)
			return quantities[q].value;
	}
	return 0.0;
}


bool ArticleManager::IsEnsUnit (const GS::UniString& unit)
{
	return unit.IsEmpty () || NormalizeUnit (unit) == GS::UniString ("ENS");
}


GS::UniString ArticleManager::NormalizedUnit (const GS::UniString& unit)
{
	return NormalizeUnit (unit);
}


// --- Formules dérivées ------------------------------------------------------------
// Une formule est une expression arithmétique dont les variables sont les
// LIBELLÉS des quantités de la ligne facturée : « Contour ouverture * Épaisseur
// mur hôte » (enduit latéral des tableaux), « Périmètre * Hauteur », etc.
// Priorité : * / sur + - ; parenthèses, nombres (point ou virgule), signe -
// et les variantes typographiques × · ÷ sont acceptées.

class FormulaEvaluator {
public:
	FormulaEvaluator (const GS::Array<CWQuantity>& quantities) : quantities (quantities) {}

	bool Evaluate (const GS::UniString& formula, double& outValue, GS::UniString& outError)
	{
		text = GS::ToWString (formula);
		pos = 0;
		error.Clear ();
		SkipSpaces ();
		if (pos >= text.size ()) {
			outError = FR ("Formule vide.");
			return false;
		}

		double value = 0.0;
		if (!ParseSum (value))
			return false;

		SkipSpaces ();
		if (pos < text.size ()) {
			outError = FR ("Caractère inattendu : « ")
				+ GS::ToUniString (std::wstring (1, text[pos])) + FR (" ».");
			return false;
		}

		outValue = value;
		return true;
	}

private:
	const GS::Array<CWQuantity>&	quantities;
	std::wstring					text;
	size_t							pos = 0;
	GS::UniString				error;

	void SkipSpaces ()
	{
		while (pos < text.size ()
				&& (text[pos] == L' ' || text[pos] == L'\t' || text[pos] == 0x00A0))
			++pos;
	}

	bool ParseSum (double& out)
	{
		double value = 0.0;
		if (!ParseProduct (value))
			return false;
		for (;;) {
			SkipSpaces ();
			if (pos < text.size () && (text[pos] == L'+' || text[pos] == L'-')) {
				const wchar_t op = text[pos];
				++pos;
				double right = 0.0;
				if (!ParseProduct (right))
					return false;
				value = (op == L'+') ? value + right : value - right;
			} else {
				break;
			}
		}
		out = value;
		return true;
	}

	bool ParseProduct (double& out)
	{
		double value = 0.0;
		if (!ParseFactor (value))
			return false;
		for (;;) {
			SkipSpaces ();
			if (pos >= text.size ())
				break;
			const wchar_t ch = text[pos];
			if (ch == L'*' || ch == 0x00D7 || ch == 0x00B7) {		// * × ·
				++pos;
				double right = 0.0;
				if (!ParseFactor (right))
					return false;
				value *= right;
			} else if (ch == L'/' || ch == 0x00F7) {				// / ÷
				++pos;
				double right = 0.0;
				if (!ParseFactor (right))
					return false;
				if (right == 0.0) {
					error = FR ("Division par zéro.");
					return false;
				}
				value /= right;
			} else {
				break;
			}
		}
		out = value;
		return true;
	}

	bool ParseFactor (double& out)
	{
		SkipSpaces ();
		if (pos >= text.size ()) {
			error = FR ("Formule incomplète.");
			return false;
		}

		const wchar_t ch = text[pos];
		if (ch == L'(') {
			++pos;
			double value = 0.0;
			if (!ParseSum (value))
				return false;
			SkipSpaces ();
			if (pos >= text.size () || text[pos] != L')') {
				error = FR ("Parenthèse fermante manquante.");
				return false;
			}
			++pos;
			out = value;
			return true;
		}
		if (ch == L'-') {
			++pos;
			double value = 0.0;
			if (!ParseFactor (value))
				return false;
			out = -value;
			return true;
		}
		if (ch == L'+') {
			++pos;
			return ParseFactor (out);
		}
		if (ch >= L'0' && ch <= L'9')
			return ParseNumber (out);
		return ParseLabel (out);
	}

	bool ParseNumber (double& out)
	{
		std::wstring number;
		bool hasSeparator = false;
		while (pos < text.size ()) {
			const wchar_t ch = text[pos];
			if (ch >= L'0' && ch <= L'9') {
				number += ch;
				++pos;
			} else if ((ch == L'.' || ch == L',') && !hasSeparator) {
				hasSeparator = true;
				number += L'.';
				++pos;
			} else {
				break;
			}
		}
		out = wcstod (number.c_str (), nullptr);
		return true;
	}

	bool ParseLabel (double& out)
	{
		// Libellé le plus long à la position courante (« Surface nette » avant
		// « Surface ») : les libellés peuvent contenir espaces et accents.
		UIndex bestIndex = 0;
		size_t bestLength = 0;
		for (UIndex q = 0; q < quantities.GetSize (); ++q) {
			const std::wstring label = GS::ToWString (quantities[q].label);
			if (label.empty ())
				continue;
			if (text.compare (pos, label.size (), label) == 0 && label.size () > bestLength) {
				bestIndex = q;
				bestLength = label.size ();
			}
		}

		if (bestLength == 0) {
			size_t end = pos;
			while (end < text.size () && text[end] != L' ' && text[end] != L'\t'
				&& text[end] != L'+' && text[end] != L'-' && text[end] != L'*' && text[end] != L'/'
				&& text[end] != L'(' && text[end] != L')')
				++end;
			error = FR ("Quantité inconnue : « ")
				+ GS::ToUniString (text.substr (pos, end - pos)) + FR (" ».");
			return false;
		}

		out = quantities[bestIndex].value;
		pos += bestLength;
		return true;
	}
};


bool ArticleManager::ValidateFormula (const GS::UniString& formula, const GS::Array<CWQuantity>& availableQuantities,
									  GS::UniString& outError)
{
	FormulaEvaluator evaluator (availableQuantities);
	double value = 0.0;
	return evaluator.Evaluate (formula, value, outError);
}


double ArticleManager::QuantityForArticle (const CWArticle& article, const GS::Array<CWQuantity>& quantities)
{
	// Formule dérivée (« Règles de calcul », spec : enduit latéral = contour
	// de l'ouverture × épaisseur du mur hôte) : prioritaire sur le libellé.
	// Si la ligne ne possède pas une quantité référencée, repli sur la règle.
	if (!article.calcFormula.IsEmpty ()) {
		FormulaEvaluator evaluator (quantities);
		double value = 0.0;
		GS::UniString formulaError;
		if (evaluator.Evaluate (article.calcFormula, value, formulaError))
			return value;
	}

	// Règle explicite (« Règles de calcul ») : la quantité adoptée est celle
	// qui porte exactement ce libellé (Surface nette, Surface brute, Volume
	// conditionné, Surface projetée…). Si la ligne ne la possède pas (autre
	// type d'élément), repli sur la première quantité de l'unité.
	if (!article.calcQuantity.IsEmpty ()) {
		for (UIndex q = 0; q < quantities.GetSize (); ++q) {
			if (quantities[q].label == article.calcQuantity)
				return quantities[q].value;
		}
	}

	return QuantityForUnit (quantities, article.unit);
}


// --- Règles de calcul : persistance (CostWaves-calcul.json) ----------------------

namespace {

// Chemin du fichier de règles : à côté du PLN (repli : Documents).
GS::UniString CalcRulesFilePath ()
{
	GS::UniString folder;
	GS::UniString projectName;
	if (!Exporter::ResolveProjectLocation (folder, projectName))
		return GS::UniString ();
	return folder + "/" + US ("CostWaves-calcul.json");
}

// Échappement minimal d'une chaîne JSON.
// Valeur JSON : texte échappé ET encadré de guillemets ("...") — sans les
// guillemets le fichier écrit est du JSON invalide (illisible au rechargement).
GS::UniString EscapeJsonText (const GS::UniString& text)
{
	std::wstring source = GS::ToWString (text);
	std::wstring escaped;
	escaped += L'"';
	for (wchar_t ch : source) {
		if (ch == L'\\' || ch == L'"')
			escaped += L'\\';
		if (ch < 0x20) {
			wchar_t buffer[8];
			swprintf (buffer, 8, L"\\u%04x", static_cast<unsigned int> (ch));
			escaped += buffer;
		} else {
			escaped += ch;
		}
	}
	escaped += L'"';
	return GS::ToUniString (escaped);
}

} // namespace


bool ArticleManager::LoadCalcRules (GS::Array<CWArticle>& ioArticles, GS::UniString& outError)
{
	outError.Clear ();

	const GS::UniString path = CalcRulesFilePath ();
	if (path.IsEmpty ())
		return true;	// dossier inconnu : aucune règle, sans erreur

	std::string content;
	if (!ReadUtf8File (path, content))
		return true;	// fichier absent : aucune règle

	JsonParser parser (content);
	JsonValue root;
	size_t errorPos = 0;
	if (!parser.Parse (root, errorPos)) {
		outError = FR ("CostWaves-calcul.json illisible (vers l'octet ")
				   + GS::ToUniString (std::to_wstring (static_cast<int> (errorPos))) + FR (").");
		return false;
	}

	// Format : {"rules": {"ID": "Libellé"}} (ou l'objet directement).
	const JsonValue* rules = (root.type == JsonValue::Type::Object) ? root.Find ("rules") : nullptr;
	if (rules == nullptr && root.type == JsonValue::Type::Object)
		rules = &root;
	if (rules == nullptr || rules->type != JsonValue::Type::Object)
		return true;	// pas de règles lisibles : comportement automatique

	for (UIndex a = 0; a < ioArticles.GetSize (); ++a) {
		// Les clés JSON sont stockées en UTF-8 brut (JsonValue::objectValue).
		const auto idUtf8 = ioArticles[a].id.ToCStr (CC_UTF8);
		const JsonValue* rule = rules->Find (idUtf8.Get ());
		if (rule != nullptr && rule->type == JsonValue::Type::String)
			ioArticles[a].calcQuantity = GS::UniString (rule->stringValue.c_str (), CC_UTF8);
	}

	// Formules dérivées : {"formulas": {"ID": "Contour ouverture * Épaisseur mur hôte"}}.
	const JsonValue* formulas = (root.type == JsonValue::Type::Object) ? root.Find ("formulas") : nullptr;
	if (formulas != nullptr && formulas->type == JsonValue::Type::Object) {
		for (UIndex a = 0; a < ioArticles.GetSize (); ++a) {
			const auto idUtf8 = ioArticles[a].id.ToCStr (CC_UTF8);
			const JsonValue* formula = formulas->Find (idUtf8.Get ());
			if (formula != nullptr && formula->type == JsonValue::Type::String)
				ioArticles[a].calcFormula = GS::UniString (formula->stringValue.c_str (), CC_UTF8);
		}
	}

	return true;
}


bool ArticleManager::SaveCalcRules (const GS::Array<CWArticle>& articles, GS::UniString& outError)
{
	outError.Clear ();

	const GS::UniString path = CalcRulesFilePath ();
	if (path.IsEmpty ()) {
		outError = FR ("Impossible de déterminer le dossier du projet (enregistrez le PLN puis réessayez).");
		return false;
	}

	GS::UniString json;
	json += US ("{\n  \"rules\": {\n");
	bool first = true;
	for (UIndex a = 0; a < articles.GetSize (); ++a) {
		if (articles[a].calcQuantity.IsEmpty ())
			continue;	// article en automatique : rien à mémoriser
		json += (first ? US ("") : US (",\n")) + US ("    ") + EscapeJsonText (articles[a].id)
			+ US (": ") + EscapeJsonText (articles[a].calcQuantity);
		first = false;
	}
	json += US ("\n  },\n  \"formulas\": {\n");
	first = true;
	for (UIndex a = 0; a < articles.GetSize (); ++a) {
		if (articles[a].calcFormula.IsEmpty ())
			continue;	// pas de formule dérivée : rien à mémoriser
		json += (first ? US ("") : US (",\n")) + US ("    ") + EscapeJsonText (articles[a].id)
			+ US (": ") + EscapeJsonText (articles[a].calcFormula);
		first = false;
	}
	json += US ("\n  }\n}\n");

	if (!Exporter::WriteUtf8File (path, json, false)) {
		outError = FR ("Écriture de CostWaves-calcul.json impossible.");
		return false;
	}
	return true;
}


const CWArticle* ArticleManager::FindArticle (const GS::Array<CWArticle>& articles, const GS::UniString& articleId)
{
	for (UIndex a = 0; a < articles.GetSize (); ++a) {
		if (articles[a].id == articleId)
			return &articles[a];
	}
	return nullptr;
}


const CWArticle* ArticleManager::ArticleForRow (const GS::Array<CWArticle>& articles, const CWElementRow& row)
{
	return FindArticle (articles, RowArticleId (row));
}


const CWArticle* ArticleManager::ArticleForComponent (const GS::Array<CWArticle>& articles,
													  const CWComponentRow& component)
{
	return FindArticle (articles, ComponentArticleId (component));
}


GS::UniString ArticleManager::LocalArticlesFilePath ()
{
	// Articles créés depuis Archicad (spec §10) : indépendants des projets.
	API_SpecFolderID specFolder = API_UserDocumentsFolderID;
	IO::Location documentsLocation;
	if (ACAPI_ProjectSettings_GetSpecFolder (&specFolder, &documentsLocation) != NoError)
		return GS::UniString ();

	GS::UniString documentsPath;
	if (documentsLocation.ToPath (&documentsPath) != NoError || documentsPath.IsEmpty ())
		return GS::UniString ();

	return documentsPath + "/" + US ("CostWaves-articles-locaux.json");
}


GS::UniString ArticleManager::ArticleBaseFilePath ()
{
	// Base d'articles CostWaves (fichier local pendant le dev, API plus tard).
	API_SpecFolderID specFolder = API_UserDocumentsFolderID;
	IO::Location documentsLocation;
	if (ACAPI_ProjectSettings_GetSpecFolder (&specFolder, &documentsLocation) != NoError)
		return GS::UniString ();

	GS::UniString documentsPath;
	if (documentsLocation.ToPath (&documentsPath) != NoError || documentsPath.IsEmpty ())
		return GS::UniString ();

	return documentsPath + "/" + US ("CostWaves-base.json");
}


bool ArticleManager::LoadArticleBase (GS::Array<CWArticle>& outArticles, GS::UniString& outError)
{
	outError.Clear ();
	outArticles.Clear ();

	// 1) Base persistée dans Documents (absente = base vide, première utilisation).
	const GS::UniString base = ArticleBaseFilePath ();
	if (!base.IsEmpty ()) {
		GS::Array<CWArticle> fromBase;
		GS::UniString baseError;
		if (ImportFromJsonFile (base, fromBase, baseError))
			outArticles = fromBase;
	}

	// 2) Articles locaux fusionnés (créés depuis l'add-on).
	GS::UniString localError;
	AppendLocalArticles (outArticles, localError);		// best effort

	return true;
}


bool ArticleManager::SaveArticleBase (const GS::Array<CWArticle>& articles, GS::UniString& outError)
{
	outError.Clear ();

	const GS::UniString path = ArticleBaseFilePath ();
	if (path.IsEmpty ()) {
		outError = FR ("Impossible de déterminer le dossier Documents.");
		return false;
	}

	GS::UniString json;
	json += US ("{\n  \"articles\": [\n");
	for (UIndex a = 0; a < articles.GetSize (); ++a) {
		json += US ("    { \"id\": ") + EscapeJsonText (articles[a].id)
			+ US (", \"name\": ") + EscapeJsonText (articles[a].name)
			+ US (", \"unit\": ") + EscapeJsonText (articles[a].unit)
			+ US (", \"chapter\": ") + EscapeJsonText (articles[a].chapter)
			+ US (", \"calcQuantity\": ") + EscapeJsonText (articles[a].calcQuantity)
			+ US (", \"calcFormula\": ") + EscapeJsonText (articles[a].calcFormula)
			+ US (" }");
		if (a + 1 < articles.GetSize ())
			json += US (",");
		json += US ("\n");
	}
	json += US ("  ]\n}\n");

	if (!Exporter::WriteUtf8File (path, json, false)) {
		outError = FR ("Écriture de CostWaves-base.json impossible (") + path + FR (").");
		return false;
	}
	return true;
}


bool ArticleManager::AppendLocalArticles (GS::Array<CWArticle>& ioArticles, GS::UniString& outError)
{
	outError.Clear ();

	const GS::UniString path = LocalArticlesFilePath ();
	if (path.IsEmpty ())
		return true;		// dossier inconnu : rien à fusionner

	GS::Array<CWArticle> local;
	if (!ImportFromJsonFile (path, local, outError))
		return false;		// fichier illisible : signaler, ne pas bloquer

	// Fusion par identifiant : le catalogue déjà chargé garde la priorité.
	for (UIndex l = 0; l < local.GetSize (); ++l) {
		if (FindArticle (ioArticles, local[l].id) != nullptr)
			continue;
		ioArticles.Push (local[l]);
	}
	return true;
}


bool ArticleManager::SaveLocalArticle (const CWArticle& article, GS::UniString& outError)
{
	outError.Clear ();

	const GS::UniString path = LocalArticlesFilePath ();
	if (path.IsEmpty ()) {
		outError = FR ("Impossible de déterminer le dossier Documents.");
		return false;
	}

	// Charger la liste locale existante, remplacer l'identifiant s'il existe.
	GS::Array<CWArticle> local;
	GS::UniString loadError;
	ImportFromJsonFile (path, local, loadError);		// absent = liste vide

	bool replaced = false;
	for (UIndex l = 0; l < local.GetSize (); ++l) {
		if (local[l].id == article.id) {
			local[l] = article;
			replaced = true;
			break;
		}
	}
	if (!replaced)
		local.Push (article);

	GS::UniString json;
	json += US ("{\n  \"articles\": [\n");
	for (UIndex l = 0; l < local.GetSize (); ++l) {
		json += US ("    { \"id\": ") + EscapeJsonText (local[l].id)
			+ US (", \"name\": ") + EscapeJsonText (local[l].name)
			+ US (", \"unit\": ") + EscapeJsonText (local[l].unit)
			+ US (", \"chapter\": ") + EscapeJsonText (local[l].chapter)
			+ US (", \"calcQuantity\": ") + EscapeJsonText (local[l].calcQuantity)
			+ US (", \"calcFormula\": ") + EscapeJsonText (local[l].calcFormula)
			+ US (" }");
		if (l + 1 < local.GetSize ())
			json += US (",");
		json += US ("\n");
	}
	json += US ("  ]\n}\n");

	if (!Exporter::WriteUtf8File (path, json, false)) {
		outError = FR ("Écriture de CostWaves-articles-locaux.json impossible.");
		return false;
	}
	return true;
}


double ArticleManager::ComputeBilledQuantity (const CWArticle& article, const CWElementRow& row,
											  const GS::Array<CWElementRow>& allRows,
											  GS::UniString& outUnit)
{
	// Nouvelle architecture (spec §4) : la RÈGLE de correspondance peut imposer
	// la quantité à adopter — elle prime sur la règle de calcul du catalogue.
	const CWArticle* effectiveArticle = &article;
	CWArticle ruleAdjusted;
	if (!row.ruleQuantity.IsEmpty ()) {
		ruleAdjusted = article;
		ruleAdjusted.calcQuantity = row.ruleQuantity;
		effectiveArticle = &ruleAdjusted;
	}

	outUnit = effectiveArticle->unit.IsEmpty () ? FR ("ENS") : effectiveArticle->unit;

	// Membre d'un ensemble/groupe : facturé via son groupe, jamais seul.
	if (row.consumed)
		return 0.0;

	// Groupe numéroté : 1 par groupe — le nombre de groupes est la quantité
	// réelle du métré (3 groupes = 3).
	if (row.isNumberedGroup)
		return 1.0;

	// Facturation à l'ensemble (forfait) : 1 par ligne facturée.
	if (IsEnsUnit (effectiveArticle->unit))
		return 1.0;

	if (row.isGroupRow) {
		// Ensemble facturé dans l'unité de l'article : somme des quantités
		// des membres selon la règle de calcul de l'article.
		double total = 0.0;
		for (UIndex m = 0; m < row.groupMembers.GetSize (); ++m) {
			const CWElementRow* member = FindRowByGuid (allRows, row.groupMembers[m]);
			if (member != nullptr)
				total += QuantityForArticle (*effectiveArticle, member->quantities);
		}
		return total;
	}

	return QuantityForArticle (*effectiveArticle, row.quantities);
}


void ArticleManager::BuildArticleSummary (const GS::Array<CWElementRow>& rows,
										  const GS::Array<CWArticle>& articles,
										  CWQuantMode mode,
										  GS::Array<CWArticleSummary>& outSummary)
{
	outSummary.Clear ();

	// Entrée du récapitulatif pour l'article donné (créée si absente).
	auto findEntry = [&] (const GS::UniString& articleId, const GS::UniString& articleName,
						  UIndex& outIndex) -> CWArticleSummary& {
		for (UIndex s = 0; s < outSummary.GetSize (); ++s) {
			if (outSummary[s].articleId == articleId) {
				outIndex = s;
				return outSummary[s];
			}
		}
		CWArticleSummary entry;
		entry.articleId = articleId;
		entry.articleName = articleName;
		entry.unit = FR ("?");
		outSummary.Push (entry);
		outIndex = outSummary.GetSize () - 1;
		return outSummary[outIndex];
	};

	// Article du catalogue correspondant à l'identifiant (nullptr si inconnu).
	auto findArticle = [&] (const GS::UniString& articleId) -> const CWArticle* {
		for (UIndex a = 0; a < articles.GetSize (); ++a) {
			if (articles[a].id == articleId)
				return &articles[a];
		}
		return nullptr;
	};

	for (UIndex i = 0; i < rows.GetSize (); ++i) {
		const CWElementRow& row = rows[i];

		// Les membres consommés sont facturés via leur ensemble/groupe.
		if (row.consumed)
			continue;

		// Article effectif (nouvelle architecture) : règle prioritaire,
		// classification en repli.
		const GS::UniString effectiveArticleId = RowArticleId (row);
		if (effectiveArticleId.IsEmpty ())
			continue;

		// Mode de métré : celui de la RÈGLE si présente (spec §11 — le choix
		// appartient à la règle), sinon le mode global de la palette.
		const CWQuantMode rowMode = row.hasRule ? row.ruleMode : mode;

		// Mode de quantification BIM (§3) : l'ÉLÉMENT ou ses COMPOSANTS,
		// jamais les deux. Les dessins 2D (catégorie indépendante) sont
		// toujours facturés comme éléments.
		const bool billElement = (rowMode != CWQuantMode::Component) || row.is2D;

		if (billElement) {
			UIndex entryIndex = 0;
			const CWArticle* catalogArticle = findArticle (effectiveArticleId);
			CWArticleSummary& entry = findEntry (effectiveArticleId,
												 catalogArticle != nullptr ? catalogArticle->name : row.classItemName,
												 entryIndex);

			const CWArticle* article = catalogArticle;
			if (article != nullptr) {
				entry.articleName = article->name;
				GS::UniString unit;
				entry.totalQuantity += ComputeBilledQuantity (*article, row, rows, unit);
				entry.unit = unit;
			}
			// Article inconnu : comptage sans total (unité "?").

			if (row.isNumberedGroup)
				++entry.numberedGroupCount;
			else if (row.isGroupRow)
				++entry.groupCount;
			else
				++entry.elementCount;
		}

		// Skins classés (phase 5) : facturés sur l'article de leur matériau
		// (règle du matériau prioritaire) en mode Composants uniquement
		// (jamais avec l'élément parent).
		if (rowMode != CWQuantMode::Component)
			continue;

		for (UIndex c = 0; c < row.components.GetSize (); ++c) {
			const CWComponentRow& component = row.components[c];
			if (component.kind != RowKind::Skin || ComponentArticleId (component).IsEmpty ())
				continue;

			const CWArticle* skinArticle = findArticle (ComponentArticleId (component));
			UIndex skinEntryIndex = 0;
			CWArticleSummary& skinEntry = findEntry (ComponentArticleId (component),
													 skinArticle != nullptr ? skinArticle->name : component.classItemName,
													 skinEntryIndex);

			if (skinArticle != nullptr) {
				skinEntry.articleName = skinArticle->name;
				GS::UniString unit;
				// La règle du matériau peut imposer la quantité à adopter.
				CWArticle skinEffective = *skinArticle;
				if (!component.ruleQuantity.IsEmpty ())
					skinEffective.calcQuantity = component.ruleQuantity;
				const double quantity = IsEnsUnit (skinEffective.unit)
					? 1.0
					: QuantityForArticle (skinEffective, component.quantities);
				skinEntry.totalQuantity += quantity;
				skinEntry.unit = skinEffective.unit.IsEmpty () ? FR ("ENS") : skinEffective.unit;
			}
			++skinEntry.skinCount;
		}
	}
}

// --- Fenêtre « Quantitatif » : lignes par article quantifié -----------------------

namespace {

// Dimension portée par une unité — détermine les paramètres de calcul
// disponibles (m² -> surface, ml/m -> longueur, m³ -> volume, sinon comptage).
CWQtyDimension DimensionForUnit (const GS::UniString& unit)
{
	const GS::UniString normalized = NormalizeUnit (unit);
	if (normalized == US ("M2"))
		return CWQtyDimension::Surface;
	if (normalized == US ("ML") || normalized == US ("M"))
		return CWQtyDimension::Length;
	if (normalized == US ("M3"))
		return CWQtyDimension::Volume;
	return CWQtyDimension::Unitary;
}

// Somme des quantités portant l'un des deux libellés (déductions).
double SumQuantityLabels (const GS::Array<CWQuantity>& quantities,
						  const char* label1Utf8, const char* label2Utf8)
{
	double total = 0.0;
	for (UIndex q = 0; q < quantities.GetSize (); ++q) {
		if (quantities[q].label == FR (label1Utf8)
			|| (label2Utf8 != nullptr && quantities[q].label == FR (label2Utf8)))
			total += quantities[q].value;
	}
	return total;
}

// Quantité d'une contribution selon le mode de calcul (fenêtre Quantitatif).
// La BASE est la règle de calcul de l'article (catalogue / règle de
// correspondance) ; le mode l'ajuste :
//  - Brute : la base telle quelle (géométrie principale) ;
//  - Conditionnelle : le VOLUME CONDITIONNÉ des connexions si disponible ;
//  - Nette : la base MOINS les ouvertures et trous déduits (surfaces).
double QuantityByMode (CWQtyDimension dimension, CWCalcMode mode,
					   bool deductOpenings, bool deductHoles,
					   const GS::Array<CWQuantity>& quantities, double base)
{
	switch (mode) {
		case CWCalcMode::Brute:
			return base;

		case CWCalcMode::Conditionnelle:
			if (dimension == CWQtyDimension::Volume) {
				for (UIndex q = 0; q < quantities.GetSize (); ++q) {
					if (quantities[q].label == FR ("Volume conditionné"))
						return quantities[q].value;
				}
			}
			return base;

		case CWCalcMode::Nette: {
			if (dimension != CWQtyDimension::Surface)
				return base;
			double deduction = 0.0;
			if (deductOpenings)
				deduction += SumQuantityLabels (quantities, "Surface fenêtres", "Surface portes");
			if (deductHoles)
				deduction += SumQuantityLabels (quantities, "Surface trous vides", "Surface trous");
			return base - deduction;
		}
	}
	return base;
}

// Origine Archicade d'une ligne facturée (traçabilité) : la RÈGLE de
// correspondance si présente (composite, profil, matériau, objet GDL),
// sinon la classe de classification portée par l'élément.
void SourceForRow (const CWElementRow& row, const GS::Array<CWMapRule>& rules,
				   CWSourceType& outType, GS::UniString& outText, GS::UniString& outDetail)
{
	outDetail.Clear ();
	if (row.hasRule) {
		switch (row.structureType) {
			case CWStructureType::Composite:
			case CWStructureType::Profile: {
				outType = (row.structureType == CWStructureType::Composite)
					? CWSourceType::CompositeRule : CWSourceType::ProfileRule;
				outText = (row.structureType == CWStructureType::Composite)
					? FR ("Composite — ") + row.structureName
					: FR ("Profil — ") + row.structureName;
				const CWMapRule* rule = RuleLibrary::FindRule (rules, row.structureType,
															   row.structureName);
				if (rule != nullptr)
					outDetail = (rule->mode == CWQuantMode::Element)
						? FR ("calculé pour lui-même (règle)")
						: FR ("calculé par ses couches (règle)");
				return;
			}
			case CWStructureType::BuildingMaterial:
				outType = CWSourceType::MaterialRule;
				outText = FR ("Matériau — ") + row.structureName;
				return;
			case CWStructureType::LibraryPart: {
				outType = CWSourceType::LibraryPartRule;
				outText = FR ("Objet GDL — ") + row.structureName;
				const CWMapRule* rule = RuleLibrary::FindRule (rules, CWStructureType::LibraryPart,
															   row.structureName);
				if (rule != nullptr && !rule->keyName.IsEmpty ())
					outDetail = FR ("valeur clé : ") + rule->keyName;
				return;
			}
			case CWStructureType::LibraryPartBool: {
				// Article hérité : le BOOLÉEN activé fait naître l'article,
				// quel que soit l'objet qui le porte (règle globale).
				outType = CWSourceType::InheritedBoolRule;
				const CWMapRule* rule = nullptr;
				for (UIndex r = 0; r < rules.GetSize () && rule == nullptr; ++r) {
					if (rules[r].structureType == CWStructureType::LibraryPartBool
						&& rules[r].keyId == row.ruleKeyId)
						rule = &rules[r];
				}
				outText = FR ("Booléen — ")
						  + (rule != nullptr && !rule->keyName.IsEmpty () ? rule->keyName : row.ruleKeyId);
				if (!row.structureName.IsEmpty ())
					outDetail = FR ("objet porteur : ") + row.structureName;
				if (rule != nullptr && !rule->valueKeyName.IsEmpty ())
					outDetail += (outDetail.IsEmpty () ? GS::UniString () : FR (" · "))
								 + FR ("valeur clé : ") + rule->valueKeyName;
				return;
			}
			default:
				break;
		}
	}
	outType = CWSourceType::Classification;
	outText = row.classItemName.IsEmpty ()
		? row.typeName
		: FR ("Classe — ") + row.classItemName;
}

} // namespace


void ArticleManager::BuildQuantityLines (const GS::Array<CWElementRow>&		rows,
										 const GS::Array<CWArticle>&			articles,
										 const GS::Array<CWMapRule>&			rules,
										 CWQuantMode							globalMode,
										 const GS::Array<CWQuantityLine>&	previousLines,
										 GS::Array<CWQuantityLine>&			outLines)
{
	outLines.Clear ();

	// Article du catalogue correspondant à l'identifiant.
	auto findArticle = [&] (const GS::UniString& articleId) -> const CWArticle* {
		for (UIndex a = 0; a < articles.GetSize (); ++a) {
			if (articles[a].id == articleId)
				return &articles[a];
		}
		return nullptr;
	};

	// Contribution d'une quantité (élément ou skin) à un article : crée la
	// ligne au premier passage. Les RÉGLAGES DE CALCUL (unité, mode,
	// déductions) viennent de la RÈGLE de correspondance (fenêtres
	// Matériaux / objets GDL / articles hérités) ; seule la correction
	// manuelle de la quantité est conservée de l'affichage précédent.
	auto addContribution = [&] (const CWArticle& article, const CWMapRule* rule,
								const GS::Array<CWQuantity>& quantities,
								CWSourceType sourceType, const GS::UniString& sourceText,
								const GS::UniString& sourceDetail) {
		CWQuantityLine* linePtr = nullptr;
		for (UIndex l = 0; l < outLines.GetSize (); ++l) {
			if (outLines[l].articleId == article.id) {
				linePtr = &outLines[l];
				break;
			}
		}
		if (linePtr == nullptr) {
			CWQuantityLine line;
			line.articleId = article.id;
			line.articleName = article.name;
			// Unité : celle de la RÈGLE si elle en impose une, sinon l'article.
			if (rule != nullptr && !rule->unit.IsEmpty ())
				line.unit = rule->unit;
			else
				line.unit = article.unit.IsEmpty () ? FR ("u") : article.unit;
			if (rule != nullptr) {
				line.calcMode = rule->calcMode;
				line.deductOpenings = rule->deductOpenings;
				line.deductHoles = rule->deductHoles;
			}
			for (UIndex p = 0; p < previousLines.GetSize (); ++p) {
				if (previousLines[p].articleId == article.id) {
					line.manualOverride = previousLines[p].manualOverride;
					line.retainedQuantity = previousLines[p].retainedQuantity;
					break;
				}
			}
			outLines.Push (line);
			linePtr = &outLines[outLines.GetSize () - 1];
		}

		CWQuantityLine& line = *linePtr;
		line.dimension = DimensionForUnit (line.unit);

		// Base : la règle de correspondance (« quantité à adopter ») prime
		// sur la règle de calcul du catalogue (calcQuantity/calcFormula),
		// sinon première quantité de l'unité.
		double quantity = 0.0;
		if (line.dimension == CWQtyDimension::Unitary) {
			// Unité non géométrique (u, kg, ENS…) : comptage — 1 par
			// élément facturé (3 groupes = 3).
			quantity = 1.0;
		} else {
			CWArticle effective = article;
			effective.unit = line.unit;
			if (rule != nullptr && !rule->quantity.IsEmpty ())
				effective.calcQuantity = rule->quantity;
			const double base = QuantityForArticle (effective, quantities);
			quantity = QuantityByMode (line.dimension, line.calcMode,
									   line.deductOpenings, line.deductHoles,
									   quantities, base);
		}

		// Disponibilités lues dans la maquette (notes du mode courant).
		if (SumQuantityLabels (quantities, "Surface fenêtres", "Surface portes") > 0.0)
			line.hasOpenings = true;
		if (SumQuantityLabels (quantities, "Surface trous vides", "Surface trous") > 0.0)
			line.hasHoles = true;
		for (UIndex q = 0; q < quantities.GetSize (); ++q) {
			if (quantities[q].label == FR ("Volume conditionné")) {
				line.hasConditioned = true;
				break;
			}
		}

		// Origine : comptage + sous-total par source.
		bool sourceFound = false;
		for (UIndex s = 0; s < line.sources.GetSize (); ++s) {
			if (line.sources[s].type == sourceType && line.sources[s].text == sourceText) {
				++line.sources[s].count;
				line.sources[s].subtotal += quantity;
				sourceFound = true;
				break;
			}
		}
		if (!sourceFound) {
			CWQuantitySource source;
			source.type = sourceType;
			source.text = sourceText;
			source.detail = sourceDetail;
			source.count = 1;
			source.subtotal = quantity;
			line.sources.Push (source);
		}

		++line.elementCount;
		line.calculatedQuantity += quantity;
	};

	for (UIndex i = 0; i < rows.GetSize (); ++i) {
		const CWElementRow& row = rows[i];

		// Les membres consommés sont facturés via leur ensemble/groupe.
		if (row.consumed)
			continue;

		// LE MOTEUR SUIT LA CORRESPONDANCE : pour chaque élément il cherche
		// la règle dans la bibliothèque (fenêtre Matériaux, composites et
		// profils). La correspondance décide SANS repli : règle avec article
		// en mode Élément -> l'élément LUI-MÊME ; tout le reste (règle mode
		// Composants, article vide, aucune règle) -> « quantifié par matériau
		// décomposé » : SES COUCHES, facturées sur les articles des règles
		// matériaux. Le repli classification ne s'applique PAS aux
		// structures : la fenêtre de correspondance promet la décomposition
		// (« composite/profil sans classe = quantifié par ses matériaux »).
		// Jamais l'élément ET ses couches à la fois.
		const bool isStructureRow = ((row.structureType == CWStructureType::Composite
									   || row.structureType == CWStructureType::Profile)
									  && !row.structureName.IsEmpty ());
		bool billElementItself;
		if (isStructureRow) {
			billElementItself = (row.hasRule
								 && !row.ruleArticleId.IsEmpty ()
								 && row.ruleMode == CWQuantMode::Element);
		} else {
			billElementItself = (globalMode != CWQuantMode::Component) || row.is2D;
		}

		const GS::UniString effectiveArticleId = RowArticleId (row);

		if (billElementItself && !effectiveArticleId.IsEmpty ()) {
			const CWArticle* article = findArticle (effectiveArticleId);
			if (article != nullptr) {
				CWSourceType sourceType = CWSourceType::Unknown;
				GS::UniString sourceText;
				GS::UniString sourceDetail;
				SourceForRow (row, rules, sourceType, sourceText, sourceDetail);
				// Règle de la ligne : les articles hérités se retrouvent par
				// leur CLÉ « bool:<nom> » (la règle est GLOBALE, structureName
				// ne porte que l'objet d'origine) ; les autres par structure.
				const CWMapRule* rowRule = nullptr;
				if (row.structureType == CWStructureType::LibraryPartBool && !row.ruleKeyId.IsEmpty ()) {
					for (UIndex r = 0; r < rules.GetSize () && rowRule == nullptr; ++r) {
						if (rules[r].structureType == CWStructureType::LibraryPartBool
							&& rules[r].keyId == row.ruleKeyId)
							rowRule = &rules[r];
					}
				} else if (row.hasRule) {
					rowRule = RuleLibrary::FindRule (rules, row.structureType, row.structureName);
				}
				addContribution (*article, rowRule, row.quantities,
								 sourceType, sourceText, sourceDetail);
			}
		}

		// Ses couches (mode Composants de la règle, ou structure décomposée) :
		// article du MATÉRIAU de chaque couche — jamais avec l'élément.
		if (billElementItself)
			continue;

		for (UIndex c = 0; c < row.components.GetSize (); ++c) {
			const CWComponentRow& component = row.components[c];
			if (component.kind != RowKind::Skin || ComponentArticleId (component).IsEmpty ())
				continue;

			const CWArticle* skinArticle = findArticle (ComponentArticleId (component));
			if (skinArticle == nullptr)
				continue;

			// Traçabilité de la décomposition : le matériau, via le
			// composite de la couche quand il est connu.
			GS::UniString skinDetail;
			if (!component.compositeName.IsEmpty ())
				skinDetail = FR ("via ") + component.compositeName;

			const CWMapRule* materialRule = RuleLibrary::FindRule (rules,
																	 CWStructureType::BuildingMaterial,
																	 component.label);
			addContribution (*skinArticle, materialRule, component.quantities,
							 CWSourceType::MaterialRule,
							 FR ("Matériau — ") + component.label, skinDetail);
		}
	}

	// Notes (limites du mode courant) et quantité retenue : sans correction
	// manuelle, la retenue EST la quantité calculée — jamais perdue.
	for (UIndex l = 0; l < outLines.GetSize (); ++l) {
		CWQuantityLine& line = outLines[l];
		if (line.dimension == CWQtyDimension::Surface && line.calcMode == CWCalcMode::Nette
			&& !line.hasOpenings && !line.hasHoles) {
			line.note = FR ("aucune déduction disponible — surface brute conservée");
		} else if (line.calcMode == CWCalcMode::Conditionnelle
				   && !(line.dimension == CWQtyDimension::Volume && line.hasConditioned)) {
			line.note = FR ("aucune condition disponible — géométrie principale");
		} else {
			line.note.Clear ();
		}

		if (!line.manualOverride)
			line.retainedQuantity = line.calculatedQuantity;
	}
}

} // namespace CostWaves
