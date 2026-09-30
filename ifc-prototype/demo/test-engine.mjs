// Test du moteur de index.html en Node (extraction + concaténation, sans échappement fragile).
// Usage : node demo/test-engine.mjs
import { readFileSync, writeFileSync, mkdirSync, rmSync } from "fs";
import { execSync } from "child_process";
import { fileURLToPath } from "url";
import path from "path";

const here = path.dirname(fileURLToPath(import.meta.url));
const root = path.join(here, "..");
const outDir = path.join(root, ".arena-test");
mkdirSync(outDir, { recursive: true });

// 1) Extraire le script module de la page
const html = readFileSync(path.join(root, "index.html"), "utf-8");
const m = html.match(/<script type="module">([\s\S]*?)<\/script>/);
if (!m) throw new Error("script module introuvable dans index.html");
let src = m[1];
src = src.replace("import * as WebIFC from './vendor/web-ifc-api.js';",
	"import * as WebIFC from 'web-ifc';\n" +
	"import { readFileSync } from 'fs';\n" +
	"WebIFC.IfcAPI.prototype.SetWasmPath = () => {};");
const cut = src.indexOf("/* ======================= \u00e9v\u00e9nements");
if (cut < 0) throw new Error("ancre section \u00e9v\u00e9nements introuvable");
src = src.slice(0, cut);
// Le script utilise localStorage au chargement de l'\u00e9tat -> stub global
src = "const localStorage = { getItem: () => null, setItem: () => {}, removeItem: () => {} };\n" + src;

// 2) Corps du test (s\u00e9par\u00e9 pour rester lisible)
const TEST = `
/* ================= TEST ================= */
log = (msg, c) => console.log((c === 'w' ? '[warn] ' : c === 'e' ? '[ERR] ' : '') + msg);
const buffer = readFileSync(DEMO_IFC_PATH);
const { rows } = await parseIfc(buffer);
state.rules = normalizeRules(DEMO_RULES);
const lines = computeLines(rows, state.rules);

console.log("\\n=== ELEMENTS ===");
for (const r of rows) console.log(r.typeLabel, "|", r.name, "|", r.storey, "|", r.layerSetName || r.materialName,
	"| quants:", Object.keys(r.quants).length, "| props:", Object.keys(r.props).map(p => p + (r.props[p].isBool ? "(bool)" : "")).join(","));
console.log("\\n=== METRE ===");
for (const l of lines) console.log(l.articleId, "|", l.variant || "-", "|", l.unit, "|", l.qty, "| x" + l.count, "|", l.sources.join(" + "));

const L = (a, v) => lines.find(l => l.articleId === a && l.variant === v);
const approx = (x, y) => Math.abs(x - y) < 1e-9;
const checks = [
	["CW-BET-25 (250 mm) m3 = NetVolume mur 1", () => approx(L("CW-BET-25", "250 mm").qty, 3.15)],
	["CW-END-02 (20 mm) m2 = NetSideArea mur 1 (mode nette)", () => approx(L("CW-END-02", "20 mm").qty, 9.8)],
	["CW-FEN-01 m2 = formule Perimeter*LargeurTablette", () => approx(L("CW-FEN-01", "").qty, 5.4 * 0.15)],
	["CW-TAB-01 ml = formule Width, variante 150 mm", () => approx(L("CW-TAB-01", "150 mm").qty, 1.2)],
	["CW-BET-25 sans variante = mur 2 (materiau direct) NetVolume", () => approx(L("CW-BET-25", "").qty, 2.4)],
	["5 lignes de metre", () => lines.length === 5],
];
let fail = 0;
console.log("\\n=== ASSERTIONS METRE ===");
for (const [label, fn] of checks) {
	let ok; try { ok = fn(); } catch (e) { ok = false; console.log("   (exception: " + e.message + ")"); }
	console.log((ok ? "PASS" : "FAIL") + " - " + label);
	if (!ok) ++fail;
}
console.log("\\n=== FORMULES ===");
const cases = [
	["1 + 2 * 3", {}, 7], ["(1 + 2) * 3", {}, 9], ["2 × 3.5", {}, 7],
	["Perimeter * LargeurTablette", { Perimeter: 5.4, LargeurTablette: 0.15 }, 0.81],
	["LargeurTablette + HauteurAllage", { LargeurTablette: 0.15, HauteurAllage: 1 }, 1.15],
	["10 / 4", {}, 2.5], ["-3 + 5", {}, 2], ["1,5 + 1,5", {}, 3],
	["PerimeterPerimeter * 2", { PerimeterPerimeter: 5 }, 10],
	["Perimeter * 2", { Perimeter: 5, PerimeterPerimeter: 100 }, 10],
];
for (const [f, vars, expected] of cases) {
	try { const got = evaluateFormula(f, vars);
		console.log((approx(got, expected) ? "PASS" : "FAIL") + ' - "' + f + '" = ' + got + " (attendu " + expected + ")");
		if (!approx(got, expected)) ++fail;
	} catch (e) { console.log('FAIL - "' + f + '" : ' + e.message); ++fail; }
}
for (const bad of ["1 +", "(1 + 2", "1 / 0", "foo * 2", "1 $ 2"]) {
	try { evaluateFormula(bad, {}); console.log('FAIL - "' + bad + '" aurait du echouer'); ++fail; }
	catch (e) { console.log('PASS - "' + bad + '" -> ' + e.message); }
}
console.log(fail === 0 ? "\\nTOUT PASS" : "\\n" + fail + " ECHEC(S)");
process.exit(fail === 0 ? 0 : 1);
`;

const demoIfc = JSON.stringify(path.join(here, "costwaves-demo.ifc"));
writeFileSync(path.join(outDir, "engine.mjs"),
	"const DEMO_IFC_PATH = " + demoIfc + ";\n" + src + TEST);
execSync("node " + JSON.stringify(path.join(outDir, "engine.mjs")), { stdio: "inherit", cwd: root });
rmSync(outDir, { recursive: true, force: true });
console.log("(harnais nettoyé)");
