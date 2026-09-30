#include "CostWavesPrecompiledHeader.hpp"

#include "RuleLibrary.hpp"

#include "UniStringWStringConversion.hpp"

#include "Exporter.hpp"

#include <string>
#include <vector>

namespace CostWaves {

namespace {

// Littéral UTF-8 -> GS::UniString (les sources sont compilees avec /utf-8).
GS::UniString FR (const char* utf8Text)
{
	return GS::UniString (utf8Text, CC_UTF8);
}

// Littéral -> GS::UniString (départ de chaîne pour l'opérateur +).
GS::UniString US (const char* utf8Text)
{
	return GS::UniString (utf8Text, CC_UTF8);
}

// --- Mini-analyse JSON (format des règles) --------------------------------------
// Le format est volontairement simple :
//   { "rules": [ { "type": "composite", "name": "MUR_EXT_30", "article": "CW-001",
//       "mode": "element", "quantity": "Surface nette", "ignored": false }, … ] }

struct RuleJsonValue {
	enum class Type { Null, Bool, Number, String, Array, Object };

	Type										type = Type::Null;
	bool										boolValue = false;
	std::string									stringValue;		// UTF-8 brut
	std::vector<RuleJsonValue>					arrayValue;
	std::vector<std::pair<std::string, RuleJsonValue>>	objectValue;	// ordre préservé

	const RuleJsonValue* Find (const char* key) const
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

class RuleJsonParser {
public:
	explicit RuleJsonParser (const std::string& source) : s (source) {}

	bool Parse (RuleJsonValue& outValue, size_t& outErrorPos)
	{
		pos = 0;
		errorPos = 0;
		if (!ParseValue (outValue)) {
			outErrorPos = errorPos;
			return false;
		}
		SkipSpaces ();
		if (pos < s.size ()) {
			errorPos = pos;
			outErrorPos = errorPos;
			return false;
		}
		outErrorPos = errorPos;
		return true;
	}

private:
	const std::string&	s;
	size_t				pos = 0;
	size_t				errorPos = 0;

	void SkipSpaces ()
	{
		while (pos < s.size () && (s[pos] == ' ' || s[pos] == '\t' || s[pos] == '\r' || s[pos] == '\n'))
			++pos;
	}

	bool ParseValue (RuleJsonValue& outValue)
	{
		SkipSpaces ();
		if (pos >= s.size ()) {
			errorPos = pos;
			return false;
		}
		const char c = s[pos];
		if (c == '{') return ParseObject (outValue);
		if (c == '[') return ParseArray (outValue);
		if (c == '"') { outValue.type = RuleJsonValue::Type::String; return ParseString (outValue.stringValue); }
		if (c == 't' || c == 'f') return ParseBool (outValue);
		if (c == 'n') { // null
			pos += 4;
			outValue.type = RuleJsonValue::Type::Null;
			return true;
		}
		return ParseNumber (outValue);
	}

	bool ParseObject (RuleJsonValue& outValue)
	{
		outValue.type = RuleJsonValue::Type::Object;
		++pos;	// {
		SkipSpaces ();
		if (pos < s.size () && s[pos] == '}') { ++pos; return true; }
		for (;;) {
			SkipSpaces ();
			std::string key;
			if (pos >= s.size () || s[pos] != '"' || !ParseString (key)) { errorPos = pos; return false; }
			SkipSpaces ();
			if (pos >= s.size () || s[pos] != ':') { errorPos = pos; return false; }
			++pos;
			RuleJsonValue value;
			if (!ParseValue (value)) return false;
			outValue.objectValue.emplace_back (key, value);
			SkipSpaces ();
			if (pos < s.size () && s[pos] == ',') { ++pos; continue; }
			if (pos < s.size () && s[pos] == '}') { ++pos; return true; }
			errorPos = pos;
			return false;
		}
	}

	bool ParseArray (RuleJsonValue& outValue)
	{
		outValue.type = RuleJsonValue::Type::Array;
		++pos;	// [
		SkipSpaces ();
		if (pos < s.size () && s[pos] == ']') { ++pos; return true; }
		for (;;) {
			RuleJsonValue value;
			if (!ParseValue (value)) return false;
			outValue.arrayValue.push_back (value);
			SkipSpaces ();
			if (pos < s.size () && s[pos] == ',') { ++pos; continue; }
			if (pos < s.size () && s[pos] == ']') { ++pos; return true; }
			errorPos = pos;
			return false;
		}
	}

	bool ParseString (std::string& outText)
	{
		++pos;	// "
		while (pos < s.size ()) {
			const unsigned char c = static_cast<unsigned char> (s[pos]);
			if (c == '"') { ++pos; return true; }
			if (c == '\\') {
				++pos;
				if (pos >= s.size ()) break;
				const char e = s[pos];
				if (e == 'u') {
					bool ok = true;
					const unsigned int code = ParseHex4 (s, pos + 1, ok);
					if (!ok) { errorPos = pos; return false; }
					pos += 4;
					AppendUtf8 (outText, code);
					++pos;
				} else {
					switch (e) {
						case '"': outText += '"'; break;
						case '\\': outText += '\\'; break;
						case '/': outText += '/'; break;
						case 'b': outText += '\b'; break;
						case 'f': outText += '\f'; break;
						case 'n': outText += '\n'; break;
						case 'r': outText += '\r'; break;
						case 't': outText += '\t'; break;
						default: errorPos = pos; return false;
					}
					++pos;
				}
			} else {
				outText += static_cast<char> (c);
				++pos;
			}
		}
		errorPos = pos;
		return false;
	}

	bool ParseBool (RuleJsonValue& outValue)
	{
		outValue.type = RuleJsonValue::Type::Bool;
		if (s.compare (pos, 4, "true") == 0) { outValue.boolValue = true; pos += 4; return true; }
		if (s.compare (pos, 5, "false") == 0) { outValue.boolValue = false; pos += 5; return true; }
		errorPos = pos;
		return false;
	}

	bool ParseNumber (RuleJsonValue& outValue)
	{
		outValue.type = RuleJsonValue::Type::Number;
		const size_t start = pos;
		while (pos < s.size () && ((s[pos] >= '0' && s[pos] <= '9') || s[pos] == '.' || s[pos] == '-'
			|| s[pos] == '+' || s[pos] == 'e' || s[pos] == 'E'))
			++pos;
		if (pos == start) { errorPos = start; return false; }
		return true;
	}
};

// Valeur JSON : texte échappé ET encadré de guillemets ("..."). Sans les
// guillemets, le fichier écrit est du JSON invalide et devient illisible
// au chargement (règles perdues à la réouverture).
GS::UniString EscapeRuleText (const GS::UniString& text)
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

GS::UniString JsonFieldText (const RuleJsonValue& object, const char* key)
{
	const RuleJsonValue* field = object.Find (key);
	if (field == nullptr || field->type != RuleJsonValue::Type::String)
		return GS::UniString ();
	return GS::UniString (field->stringValue.c_str (), CC_UTF8);
}

// Clés JSON stables des types de structures (spec §6).
const char* StructureTypeKey (CWStructureType structureType)
{
	switch (structureType) {
		case CWStructureType::BuildingMaterial:	return "material";
		case CWStructureType::Composite:		return "composite";
		case CWStructureType::Profile:			return "profile";
		case CWStructureType::Favorite:			return "favorite";
		case CWStructureType::LibraryPart:		return "object";
		case CWStructureType::LibraryPartBool:	return "objectBool";
	}
	return "composite";
}

} // namespace


GS::UniString RuleLibrary::RulesFilePath ()
{
	// Bibliothèque INDÉPENDANTE des projets (spec §6) : dossier Documents de
	// l'utilisateur, pas à côté du PLN.
	API_SpecFolderID specFolder = API_UserDocumentsFolderID;
	IO::Location documentsLocation;
	if (ACAPI_ProjectSettings_GetSpecFolder (&specFolder, &documentsLocation) != NoError)
		return GS::UniString ();

	GS::UniString documentsPath;
	if (documentsLocation.ToPath (&documentsPath) != NoError || documentsPath.IsEmpty ())
		return GS::UniString ();

	return documentsPath + "/" + US ("CostWaves-regles.json");
}


bool RuleLibrary::LoadRules (GS::Array<CWMapRule>& outRules, GS::UniString& outError)
{
	outError.Clear ();
	outRules.Clear ();

	const GS::UniString path = RulesFilePath ();
	if (path.IsEmpty ()) {
		outError = FR ("Impossible de déterminer le dossier Documents.");
		return false;
	}

	// Lecture UTF-8 (BOM toléré).
	std::string content;
	{
		const std::wstring pathW = GS::ToWString (path);
		FILE* file = _wfopen (pathW.c_str (), L"rb");
		if (file == nullptr)
			return true;		// fichier absent : bibliothèque vide, sans erreur
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
			content.append (buffer + offset, read - offset);
		}
		fclose (file);
	}

	if (content.empty ())
		return true;

	RuleJsonParser parser (content);
	RuleJsonValue root;
	size_t errorPos = 0;
	if (!parser.Parse (root, errorPos)) {
		outError = FR ("CostWaves-regles.json illisible (vers l'octet ")
			+ GS::ToUniString (std::to_wstring (static_cast<int> (errorPos))) + FR (").");
		return false;
	}

	// Format : {"rules": [...]} (ou directement un tableau).
	const RuleJsonValue* list = (root.type == RuleJsonValue::Type::Array) ? &root : root.Find ("rules");
	if (list == nullptr || list->type != RuleJsonValue::Type::Array)
		return true;		// rien de lisible : bibliothèque vide

	for (size_t i = 0; i < list->arrayValue.size (); ++i) {
		const RuleJsonValue& entry = list->arrayValue[i];
		if (entry.type != RuleJsonValue::Type::Object)
			continue;

		CWMapRule rule;
		rule.structureName = JsonFieldText (entry, "name");
		rule.articleId = JsonFieldText (entry, "article");
		rule.quantity = JsonFieldText (entry, "quantity");
		rule.keyId = JsonFieldText (entry, "key");
		rule.keyName = JsonFieldText (entry, "keyName");
		rule.valueKeyId = JsonFieldText (entry, "valueKey");
		rule.valueKeyName = JsonFieldText (entry, "valueKeyName");
		if (rule.structureName.IsEmpty ())
			continue;

		CWStructureType structureType = CWStructureType::Composite;
		StructureTypeFromName (JsonFieldText (entry, "type"), structureType);
		rule.structureType = structureType;

		const RuleJsonValue* modeField = entry.Find ("mode");
		if (modeField != nullptr && modeField->type == RuleJsonValue::Type::String
			&& JsonFieldText (entry, "mode") == US ("component"))
			rule.mode = CWQuantMode::Component;

		// Réglages de calcul (fenêtre de correspondance, tous types).
		rule.unit = JsonFieldText (entry, "unit");
		const GS::UniString calcModeKey = JsonFieldText (entry, "calcMode");
		if (calcModeKey == US ("conditionnelle"))
			rule.calcMode = CWCalcMode::Conditionnelle;
		else if (calcModeKey == US ("nette"))
			rule.calcMode = CWCalcMode::Nette;
		const RuleJsonValue* openingsField = entry.Find ("deductOpenings");
		if (openingsField != nullptr && openingsField->type == RuleJsonValue::Type::Bool)
			rule.deductOpenings = openingsField->boolValue;
		const RuleJsonValue* holesField = entry.Find ("deductHoles");
		if (holesField != nullptr && holesField->type == RuleJsonValue::Type::Bool)
			rule.deductHoles = holesField->boolValue;

		const RuleJsonValue* ignoredField = entry.Find ("ignored");
		if (ignoredField != nullptr && ignoredField->type == RuleJsonValue::Type::Bool)
			rule.ignored = ignoredField->boolValue;

		outRules.Push (rule);
	}

	return true;
}


bool RuleLibrary::SaveRules (const GS::Array<CWMapRule>& rules, GS::UniString& outError)
{
	outError.Clear ();

	const GS::UniString path = RulesFilePath ();
	if (path.IsEmpty ()) {
		outError = FR ("Impossible de déterminer le dossier Documents.");
		return false;
	}

	GS::UniString json;
	json += US ("{\n  \"rules\": [\n");
	for (UIndex r = 0; r < rules.GetSize (); ++r) {
		const CWMapRule& rule = rules[r];
		json += US ("    { \"type\": \"") + US (StructureTypeKey (rule.structureType))
			+ US ("\", \"name\": ") + EscapeRuleText (rule.structureName)
			+ US (", \"article\": ") + EscapeRuleText (rule.articleId)
			+ US (", \"mode\": \"") + US (rule.mode == CWQuantMode::Component ? "component" : "element")
			+ US ("\", \"quantity\": ") + EscapeRuleText (rule.quantity)
			+ US (", \"key\": ") + EscapeRuleText (rule.keyId)
			+ US (", \"keyName\": ") + EscapeRuleText (rule.keyName)
			+ US (", \"valueKey\": ") + EscapeRuleText (rule.valueKeyId)
			+ US (", \"valueKeyName\": ") + EscapeRuleText (rule.valueKeyName)
			+ US (", \"unit\": ") + EscapeRuleText (rule.unit)
			+ US (", \"calcMode\": \"") + US (CWCalcModeKey (rule.calcMode))
			+ US ("\", \"deductOpenings\": ") + US (rule.deductOpenings ? "true" : "false")
			+ US (", \"deductHoles\": ") + US (rule.deductHoles ? "true" : "false")
			+ US (", \"ignored\": ") + US (rule.ignored ? "true" : "false")
			+ US (" }");
		if (r + 1 < rules.GetSize ())
			json += US (",");
		json += US ("\n");
	}
	json += US ("  ]\n}\n");

	if (!Exporter::WriteUtf8File (path, json, false)) {
		outError = FR ("Écriture de CostWaves-regles.json impossible (")
			+ path + FR (").");
		return false;
	}
	return true;
}


const CWMapRule* RuleLibrary::FindRule (const GS::Array<CWMapRule>& rules,
										CWStructureType structureType, const GS::UniString& structureName)
{
	for (UIndex r = 0; r < rules.GetSize (); ++r) {
		if (rules[r].structureType == structureType && rules[r].structureName == structureName)
			return &rules[r];
	}
	return nullptr;
}


GS::UniString RuleLibrary::StructureTypeName (CWStructureType structureType)
{
	switch (structureType) {
		case CWStructureType::BuildingMaterial:	return FR ("Matériau");
		case CWStructureType::Composite:		return FR ("Composite");
		case CWStructureType::Profile:			return FR ("Profil");
		case CWStructureType::Favorite:			return FR ("Favori");
		case CWStructureType::LibraryPart:		return FR ("Objet de bibliothèque");
		case CWStructureType::LibraryPartBool:	return FR ("Article hérité");
	}
	return FR ("Composite");
}


bool RuleLibrary::StructureTypeFromName (const GS::UniString& name, CWStructureType& outType)
{
	if (name == US ("material") || name == FR ("Matériau")) { outType = CWStructureType::BuildingMaterial; return true; }
	if (name == US ("composite") || name == FR ("Composite")) { outType = CWStructureType::Composite; return true; }
	if (name == US ("profile") || name == FR ("Profil")) { outType = CWStructureType::Profile; return true; }
	if (name == US ("favorite") || name == FR ("Favori")) { outType = CWStructureType::Favorite; return true; }
	if (name == US ("object") || name == FR ("Objet de bibliothèque")) { outType = CWStructureType::LibraryPart; return true; }
	if (name == US ("objectBool") || name == FR ("Article hérité")) { outType = CWStructureType::LibraryPartBool; return true; }
	return false;
}


void RuleLibrary::CollectAvailableStructures (CWStructureType structureType,
											 GS::Array<GS::UniString>& outNames)
{
	outNames.Clear ();

	if (structureType == CWStructureType::LibraryPart) {
		// Objets de bibliothèque chargés : nombre via ACAPI_LibraryPart_GetNum,
		// puis 1..partCount en IGNORANT les index illisibles (trous possibles
		// — un break au premier échec tronquerait la liste).
		Int32 partCount = 0;
		if (ACAPI_LibraryPart_GetNum (&partCount) != NoError || partCount <= 0)
			return;
		for (Int32 i = 1; i <= partCount; ++i) {
			API_LibPart libPart;
			BNZeroMemory (&libPart, sizeof (libPart));
			libPart.index = i;
			if (ACAPI_LibraryPart_Get (&libPart) != NoError)
				continue;		// index illisible : passer au suivant
			// Ne proposer que les objets .gsm POSABLES (objet, porte, fenêtre,
			// lampe, châssis) — pas les macros, images, étiquettes…
			const bool isPlaceable = (libPart.typeID == APILib_ObjectID
								  || libPart.typeID == APILib_DoorID
								  || libPart.typeID == APILib_WindowID
								  || libPart.typeID == APILib_LampID
								  || libPart.typeID == APILib_SkylightID);
			if (isPlaceable) {
				const GS::UniString name (libPart.docu_UName);
				if (!name.IsEmpty () && !outNames.Contains (name))
					outNames.Push (name);
			}
			// ACAPI_LibraryPart_Get alloue libPart.location : le libérer.
			delete libPart.location;
			libPart.location = nullptr;
		}
		return;
	}

	API_AttrTypeID typeID = API_CompWallID;
	switch (structureType) {
		case CWStructureType::BuildingMaterial:	typeID = API_BuildingMaterialID; break;
		case CWStructureType::Composite:		typeID = API_CompWallID; break;
		case CWStructureType::Profile:			typeID = API_ProfileID; break;
		default: return;		// favoris : saisie manuelle (listing ultérieur)
	}

	GS::Array<API_Attribute> attributes;
	if (ACAPI_Attribute_GetAttributesByType (typeID, attributes) != NoError)
		return;

	for (UIndex i = 0; i < attributes.GetSize (); ++i) {
		if (!attributes[i].header.index.IsPositive ())
			continue;
		const GS::UniString name (attributes[i].header.name, CC_UTF8);
		if (!name.IsEmpty ())
			outNames.Push (name);
	}
}

} // namespace CostWaves
