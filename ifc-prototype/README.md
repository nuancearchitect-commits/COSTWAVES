# COSTWAVES-IFC

**Métré COSTWAVES depuis un fichier IFC, entièrement dans le navigateur.**
Ce prototype est la déclinaison web du add-on Archicad
[COSTWAVES](https://github.com/nuancearchitect-commits/COSTWAVES) (C++ / Archicad DevKit) :
il reprend les mêmes concepts — **règles → articles → variantes par valeur clé → formules →
quantités retenues** — et consomme **le même fichier de règles `CostWaves-regles.json`**
que le add-on exporte.

Aucune donnée ne quitte le poste : le fichier IFC est parsé localement par
[web-ifc](https://github.com/ThatOpen/engine_web-ifc) (WASM, inclus dans ce dépôt), le métré
se calcule et se modifie en page, et s'exporte en CSV / JSON.

## Essayer

- **En ligne** (si GitHub Pages est activé sur le dépôt) :
  <https://nuancearchitect-commits.github.io/COSTWAVES-IFC/>
- **En local** : `python -m http.server 8000` à la racine du dépôt, puis
  <http://localhost:8000> (aucun build, aucune dépendance à installer).

### Démonstration intégrée

1. Ouvrez la page puis chargez [`demo/costwaves-demo.ifc`](demo/costwaves-demo.ifc)
   (2 murs — un composite `MUR_EXT_30` à 2 couches, un mur en matériau direct `BetON 25` —
   et une fenêtre avec propriétés, quantités de base et classification).
2. Cliquez **Règles de démonstration** (ou importez
   [`demo/costwaves-demo-regles.json`](demo/costwaves-demo-regles.json)).
3. Le métré attendu :

| Article | Variante | Unité | Qté | Source |
|---|---|---|---|---|
| CW-BET-25 | — | m³ | 2.40 | Mur en matériau direct (`NetVolume`) |
| CW-BET-25 | 250 mm | m³ | 3.15 | Couche `BetON 25` du composite (mode **brute** → `NetVolume` de l'élément) |
| CW-END-02 | 20 mm | m² | 9.80 | Couche `Enduit` du composite (mode **nette** → `NetSideArea`) |
| CW-FEN-01 | — | m² | 0.81 | Formule `Perimeter * LargeurTablette` (quantité × propriété) |
| CW-TAB-01 | 150 mm | ml | 1.20 | Booléen `HasTablette` + formule `Width`, variante = `LargeurTablette` |

## Utilisation avec un IFC Archicad

1. Exportez l'IFC en activant les **quantités de base** (base quantities) — sans elles, le
   repli se fait sur le comptage et la page l'indique. Les **propriétés** (Psets) doivent
   contenir les paramètres GDL utilisés par les formules (options d'export Archicad).
2. Importez le `CostWaves-regles.json` du add-on, ou créez les règles dans la page
   (type, structure, article, unité, valeur clé, formule — les variables disponibles
   s'affichent en cliquant un élément).
3. Ajustez les **quantités retenues** (mémorisées dans le navigateur, par article + variante),
   puis exportez en CSV ou JSON.

## Correspondances COSTWAVES ↔ IFC

| Concept COSTWAVES (add-on) | Équivalent IFC |
|---|---|
| Scan des éléments | entités `IfcWall`, `IfcSlab`, `IfcWindow`, `IfcDoor`, `IfcBuildingElementProxy`, etc. |
| Composite / Skins | `IfcMaterialLayerSet` + `IfcMaterialLayer` (nom + épaisseur) |
| Profils | `IfcMaterialProfileSet` / `IfcMaterialProfile` (nom lisible) |
| Matériau direct | `IfcMaterial` |
| Quantités (Brute / Nette / Conditionnelle) | `IfcElementQuantity` — `GrossArea` / `NetArea`, `NetVolume`, etc. |
| Paramètres GDL (formules) | propriétés de Psets (`IfcPropertySingleValue`) |
| Booléens (articles hérités) | propriété booléenne du Pset (`bool:<nom>`) |
| Élément ID | `Tag` |
| Étage / Calque | `IfcBuildingStorey` / `IfcPresentationLayerAssignment` |
| GUID | `IfcGloballyUniqueId` |
| Classification (repli) | `IfcClassificationReference` |
| Formule de quantité | port JS du `FormulaEvaluator` (`+ - * / × · ÷`, parenthèses, virgule décimale, libellés) |

Le **format de règles est identique** à celui du add-on
(`type/name/article/mode/quantity/key/keyName/valueKey/valueKeyName/unit/quantityFormula/calcMode/deductOpenings/deductHoles/ignored`,
clés `material` / `composite` / `profile` / `favorite` / `object` / `objectBool`).

## Limites connues (prototype)

- **Quantités par couche** : l'IFC ne fournit pas de quantités par couche d'un composite ;
  chaque couche facturée adopte les quantités de son **élément porteur** (documenté dans le
  métré). La variante par `skin.thickness` reste exacte.
- **Nécessite des quantités de base** à l'export Archicad ; sans elles : comptage + avertissement.
- **Psets** : la présence des paramètres GDL dépend des options d'export Archicad.
- Pas d'aller-retour vers Archicad — c'est une **vue de contrôle** du métré.
- Ensembles / groupes (`A / 0.625`), « volume conditionné » : partiellement couverts
  (le repli préfère les quantités `Net`/`Gross` selon le mode de la règle).
- Les retenues sont mémorisées par **navigateur** (localStorage), pas de fichier projet.

## Structure du dépôt

```
index.html                     l'application (1 fichier, sans build)
vendor/web-ifc-api.js          web-ifc 0.0.68 (build navigateur, ESM)
vendor/web-ifc.wasm            moteur WASM (mono-thread, aucun header spécial requis)
demo/costwaves-demo.ifc        IFC de démonstration (IFC2X3, écrit à la main)
demo/costwaves-demo-regles.json règles de démonstration (format add-on)
demo/test-parse.mjs            test Node : lecture du demo + conventions API web-ifc
demo/test-engine.mjs           test Node : moteur complet (extrait d'index.html) + formules
```

Publication web : GitHub Pages en **déploiement depuis la branche `main`** (racine du dépôt) —
à activer dans Settings → Pages si ce n'est pas déjà fait.

## Tests

```
npm install        # web-ifc 0.0.68 (pour les tests Node uniquement)
node demo/test-parse.mjs
node demo/test-engine.mjs
```

`test-engine.mjs` extrait le script d'`index.html`, rejoue le pipeline complet
(parse → règles → métré) et vérifie les 5 lignes attendues + 15 cas de formules
(port du `FormulaEvaluator`), dont les cas d'erreur.

## Relation avec le add-on

Ce prototype sert de **validation du modèle** : si le métré passe ici sur vos IFC d'essai,
le même JSON de règles pilote le add-on. Les évolutions de format de règles sont faites
côté COSTWAVES puis répercutées ici.
