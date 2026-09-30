// Validation Node du demo IFC + des appels API web-ifc utilisés par l'app.
// Usage : node demo/test-parse.mjs
import * as WebIFC from "web-ifc";
import { readFileSync } from "fs";

const api = new WebIFC.IfcAPI();
await api.Init();

const data = readFileSync(new URL("./costwaves-demo.ifc", import.meta.url));
const modelID = api.OpenModel(new Uint8Array(data));
console.log("modelID:", modelID);

// --- Elements par type ---
const types = {
	"Mur (Wall)": WebIFC.IFCWALL,
	"Mur standard": WebIFC.IFCWALLSTANDARDCASE,
	"Fenetre": WebIFC.IFCWINDOW,
};
for (const [label, type] of Object.entries(types)) {
	const ids = api.GetLineIDsWithType(modelID, type);
	console.log(label + ":", ids.size());
	for (let i = 0; i < ids.size(); ++i) {
		const el = api.GetLine(modelID, ids.get(i));
		console.log("  #" + ids.get(i), el.Name?.value ?? el.Name, "| Tag:", el.Tag?.value ?? el.Tag,
					"| GlobalId:", el.GlobalId?.value ?? el.GlobalId);
	}
}

// --- Psets / quantites (via IfcRelDefinesByProperties) ---
const rels = api.GetLineIDsWithType(modelID, WebIFC.IFCRELDEFINESBYPROPERTIES);
console.log("\nRelDefinesByProperties:", rels.size());
for (let i = 0; i < rels.size(); ++i) {
	const rel = api.GetLine(modelID, rels.get(i));
	const defRef = rel.RelatingPropertyDefinition;
	const def = api.GetLine(modelID, defRef.value);
	console.log("Rel", rels.get(i), "-> handle:", JSON.stringify(defRef),
				"Name:", def.Name?.value ?? def.Name);
	if (Array.isArray(def.HasProperties)) {
		for (const pRef of def.HasProperties) {
			const p = api.GetLine(modelID, pRef.value);
			console.log("   Pset prop:", p.Name?.value ?? p.Name,
						"=", p.NominalValue?.value, "(" + p.NominalValue?.type + ")");
		}
	} else if (Array.isArray(def.Quantities)) {
		for (const qRef of def.Quantities) {
			const q = api.GetLine(modelID, qRef.value);
			const val = q.AreaValue ?? q.LengthValue ?? q.VolumeValue ?? q.CountValue ?? q.WeightValue;
			console.log("   Quantite:", q.Name?.value ?? q.Name, "=", val?.value ?? val);
		}
	}
}

// --- Materiaux (IfcRelAssociatesMaterial) ---
const mats = api.GetLineIDsWithType(modelID, WebIFC.IFCRELASSOCIATESMATERIAL);
console.log("\nRelAssociatesMaterial:", mats.size());
for (let i = 0; i < mats.size(); ++i) {
	const rel = api.GetLine(modelID, mats.get(i));
	const matRef = rel.RelatingMaterial;
	const mat = api.GetLine(modelID, matRef.value);
	console.log("Rel", mats.get(i), "-> handle:", JSON.stringify(matRef),
				"Name:", mat.Name?.value ?? mat.Name);
	if (matRef && mat.ForLayerSet) {
		const set = api.GetLine(modelID, mat.ForLayerSet.value);
		console.log("   LayerSet:", set.LayerSetName?.value ?? set.LayerSetName);
		for (const lRef of set.MaterialLayers) {
			const layer = api.GetLine(modelID, lRef.value);
			const m = api.GetLine(modelID, layer.Material.value);
			console.log("   Layer:", m.Name?.value ?? m.Name, "ep. =", layer.LayerThickness?.value ?? layer.LayerThickness);
		}
	}
}

// --- Contenance spatiale (etage) ---
const conts = api.GetLineIDsWithType(modelID, WebIFC.IFCRELCONTAINEDINSPATIALSTRUCTURE);
console.log("\nRelContainedInSpatialStructure:", conts.size());
for (let i = 0; i < conts.size(); ++i) {
	const rel = api.GetLine(modelID, conts.get(i));
	const storey = api.GetLine(modelID, rel.RelatingStructure.value);
	console.log("Etage:", storey.Name?.value ?? storey.Name,
				"elements:", rel.RelatedElements.length);
}

// --- Classification ---
const classifs = api.GetLineIDsWithType(modelID, WebIFC.IFCRELASSOCIATESCLASSIFICATION);
console.log("\nRelAssociatesClassification:", classifs.size());
for (let i = 0; i < classifs.size(); ++i) {
	const rel = api.GetLine(modelID, classifs.get(i));
	const ref = api.GetLine(modelID, rel.RelatingClassification.value);
	console.log("Classification:", ref.ItemReference?.value ?? ref.ItemReference,
				"Name:", ref.Name?.value ?? ref.Name);
}

api.CloseModel(modelID);
console.log("\nOK : parsing et API valides.");
