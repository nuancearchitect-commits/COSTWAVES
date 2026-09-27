# CostWaves × Archicad — Add-On de quantification

Add-On C++ Archicad (**.apx**) qui lit le modèle Archicad et prépare les données
pour **CostWaves** :

> classification → éléments → composants → quantités → tableau → export JSON / CSV.

**Statut : Phase 2 codée** — lecture des données + articles + affectation.
Code livré, à compiler sur Windows.

---

## 1. Décisions actées (validation du 27/09/2026)

| Sujet | Décision |
|---|---|
| Version cible | **Archicad 29** (DevKit 29.3100) |
| Plateforme | **Windows** seul (Visual Studio, toolset v143) |
| CostWaves | Le serveur existe ; **l'export de phase 1 est fichier** (JSON ou CSV/Excel) — pas d'appel réseau |
| Article CostWaves | `id`, `name`, `unit` |
| Unités | On commence par les plus courantes : **m², m³, m, U** (ENS plus tard) |
| Règles de quantification | **Ignorées en phase 1** — on détecte et lit les quantités existantes d'Archicad |
| Interface | **Français** |
| Identité Add-On | Developer ID **944130148** · Local ID **396703700** (déclarés dans `RFIX/AddOnFix.grc`) |

### Idées retenues / différées (numérotation du cadrage initial)

- **Retenues (phase 1)** : `01` Add-On C++ natif · `03`/`04` architecture · `06` détection par classification · `08` quantités géométriques · `11` composants (AC 25+) · `25` tableau de quantification · export fichier JSON/CSV (variante locale de `30`)
- **Plus tard** : `07` propriété `CW_Article_ID` · `09` lecture par lot · `10` skins composites poussés · `13` sélection · `14`–`17` création classification/properties · `18`–`21` groupes & sets · `22`–`24` moteur de règles/exclusion/ENS · `26`–`29` UI avancée · `30`–`32` synchro serveur · `33` Add-On Commands
- **Écartées pour l'instant** : `02` couche Python/JSON secondaire

---

## 2. Périmètre codé (phases 1 + 2)

L'Add-On ajoute une commande **« CostWaves – Lecture des quantités… »** (menu Options).

La fenêtre :

1. **Sélecteur de système de classification** — liste tous les systèmes du projet
   (Archicad, CostWaves, Uniclass, etc.) ; changer de système relance la lecture.
2. **Tableau** (colonnes redimensionnables) — pour chaque élément portant une
   classe du système choisi, et pour chacun de ses composants :

   | Type | GUID | ID élément | Étage | Classe | Quantités disponibles |
   |---|---|---|---|---|---|
   | Élément | `{8C1F…}` | `W-012` | `0 - Rez-de-chaussée` | `CW-MUR - Mur extérieur` | `Volume 12,34 m³ · Surface 45,67 m² · …` |
   | Composant (skin) | | `W-012` | `0 - RDC` | `CW-MUR - …` | `Volume 1,23 m³ · Surface projetée 4,56 m²` |
   | Composant | `{A2B4…}` | `W-012` | `0 - RDC` | `CW-MUR - …` | *(propriétés dans le panneau détails)* |

   - **Type** : `Élément` / `Composant` (composants « properties » Archicad 25+) /
     `Composant (skin)` (couche d'une structure composite)
   - **GUID** : GUID stable de l'élément ou du composant (tronqué à l'affichage,
     complet dans les détails et les exports)
   - **ID élément** : propriété intégrée « Element ID » d'Archicad (résolue par son
     GUID intégré `B1B54D45-…`, avec repli par recherche de nom)
   - **Étage** : index + nom de l'étage
   - **Classe** : ID + nom de l'item de classification
   - **Quantités disponibles** : résumé ; **tout** est dans le panneau de détails

3. **Panneau de détails** — ligne sélectionnée : GUID complet, ID, étage, classe,
   toutes les quantités, et pour un composant : ses **propriétés** (lues à la demande).
4. **Boutons** : `Actualiser` · `Exporter JSON` · `Exporter CSV` · `Fermer`.
5. **Ligne d'état** : `N éléments classés · N composants · N skins · N éléments analysés`.
6. **Articles (phase 2)** : case `Sélection uniquement`, popup d'articles,
   `Affecter l'article` · `Importer des articles…` · `Créer la classification`.

### Ce que la phase 1 lit dans Archicad

| Donnée | API utilisée |
|---|---|
| Systèmes de classification | `ACAPI_Classification_GetClassificationSystems` |
| Classe d'un élément dans un système | `ACAPI_Element_GetClassificationInSystem` |
| Tous les éléments | `ACAPI_Element_GetElemList (API_ZombieElemID)` |
| En-tête (type, étage) | `ACAPI_Element_GetHeader` |
| Nom du type (localisé) | `ACAPI_Element_GetElemTypeName` |
| Noms des étages | `ACAPI_ProjectSetting_GetStorySettings` |
| Propriété « Element ID » | `ACAPI_Element_GetPropertyValue` |
| Quantités (par type d'élément) | `ACAPI_Element_GetQuantities` (+ masque complet) |
| Skins composites (matériau, volume, surface projetée) | `API_Quantities::composites` |
| Composants d'élément (AC 25+) | `ACAPI_Element_GetComponents` |
| Propriétés d'un composant | `ACAPI_Element_GetPropertyDefinitions/GetPropertyValues` |
| Noms de matériaux | `ACAPI_Attribute_Get` |

Types d'éléments dont les quantités sont extraites en phase 1 : mur, dalle,
colonne, poutre, fenêtre, porte, objet, lampe, lanterneau, terrain (mesh),
toit, coque, morph, zone, escalier, garde-corps, mur-rideau, remplissage (hatch).
Pour un autre type : ligne présente, quantités « — » (à étendre en phase 2).

### Exports

Fichiers écrits **à côté du .PLN** (ou dans *Documents* si projet non enregistré) :

- `<Projet>_CostWaves_<AAAAMMJJ_HHMMSS>.json` — structure complète :
  éléments, classe, **article** (id/name/unit reconnu d'après la classe),
  étage, quantités, skins (matériau + volume + surface projetée),
  composants (GUID + propriétés). Format d'échange pour CostWaves.
- `<Projet>_CostWaves_<AAAAMMJJ_HHMMSS>.csv` — **format long** (une ligne par
  quantité : `Type;GUID;ID élément;Étage;Classe;Article;Libellé;Valeur;Unité`),
  séparateur `;`, UTF-8 avec BOM → s'ouvre directement dans Excel.

### Phase 2 : articles CostWaves (codée)

| Fonction | Comment ça marche |
|---|---|
| **Source des articles** | Au choix : (a) **import d'un fichier JSON** `[{"id":"CW-MUR","name":"Mur extérieur","unit":"m2"}, …]` (ou `{"articles":[…]}`) via « Importer des articles… » ; (b) à défaut, **les items du système de classification choisi** servent d'articles (id de l'item = id d'article). L'import JSON a priorité. |
| **Créer la classification** | Bouton « Créer la classification » : crée (si absente) un système de classification **« CostWaves »** avec **un item par article** (item.id = article.id, item.name = article.name), puis complète les items manquants. Opération **annulable** (Ctrl+Z). |
| **Affecter un article** | Sélectionner une ligne **élément** dans le tableau, choisir l'article dans le popup, cliquer « Affecter l'article » : l'élément reçoit l'item de classification correspondant (sa classe précédente dans le système est remplacée). Annulable. |
| **Lecture de la sélection** | Case « Sélection uniquement » : la lecture (Actualiser / changement de système) n'analyse que les éléments sélectionnés dans Archicad — pratique sur les gros projets. |
| **Export enrichi** | Colonne `Article` (CSV) et objet `"article"` (JSON) : remplis quand la classe de l'élément correspond à un article connu. |

**Limite connue** : l'API Archicad ne permet d'écrire ni propriété ni
classification au niveau des **composants** — l'affectation se fait sur
l'**élément** (ses composants suivent). C'est une contrainte de l'API 29,
pas un choix.

---

## 3. Build (Windows)

### Prérequis

- **Visual Studio 2019/2022/2026** avec le toolset **v143** (C++ desktop)
- **CMake** ≥ 3.19
- **Python** ≥ 3.10 (pour la compilation des ressources)

### Méthode recommandée (une commande)

```bat
git clone <ce dépôt>
cd COSTWAVES
python Tools/BuildAddOn.py --configFile config.json -v 29
```

Le script **télécharge automatiquement le DevKit AC29** (et le LP_XMLConverter),
génère le projet Visual Studio et compile. Résultat :

```
Build/CostWaves-29/RelWithDebInfo/CostWaves.apx
```

Options utiles : `-b Debug` (configuration) · `-p` (package zip) ·
`-d <chemin DevKit local>` (DevKit déjà téléchargé).

### Méthode manuelle (CMake)

1. Télécharger le DevKit :
   https://github.com/GRAPHISOFT/archicad-api-devkit/releases
   (`API.Development.Kit.WIN.29.3100.zip`) et le dézipper.
2. Générer le projet :

   ```bat
   cmake -B Build -G "Visual Studio 17 2022" -A x64 -T v143 ^
         -DAC_API_DEVKIT_DIR="<chemin vers le dossier Support du DevKit>" ^
         -DAC_VERSION=29
   ```

3. Ouvrir `Build/CostWaves-29.sln` et compiler.

### Installation dans Archicad

1. Archicad 29 → **Options > Gestionnaire d'Add-Ons**.
2. « Ajouter » → sélectionner `CostWaves.apx`.
3. Ouvrir un projet → **Options > CostWaves – Lecture des quantités…**

---

## 4. Plan de test

### Phase 1 — lecture

- [ ] La fenêtre s'ouvre et liste les systèmes de classification
- [ ] Le choix du système filtre correctement (seuls les éléments classés apparaissent)
- [ ] Colonne ID élément remplie (propriété « Element ID »)
- [ ] Colonne Étage = index + nom corrects
- [ ] Quantités cohérentes avec les nomenclatures/schedules Archicad (volumes, surfaces)
- [ ] Un mur composite affiche bien ses skins (matériau, volume, surface projetée)
- [ ] Les composants apparaissent avec leur GUID, et leurs propriétés dans les détails
- [ ] Temps de lecture acceptable sur le projet (le scan est synchrone)

### Phase 2 — articles

- [ ] « Importer des articles… » : un JSON `[{"id","name","unit"},…]` se charge,
      le popup liste les articles, la ligne d'info indique la source
- [ ] Sans import : le popup se remplit avec les items du système choisi
- [ ] « Créer la classification » : le système « CostWaves » apparaît dans
      Archicad (Options > Classifications) avec un item par article ; relancer
      le bouton ne crée rien de plus
- [ ] « Affecter l'article » : la colonne Classe de l'élément change (système
      CostWaves sélectionné en haut), et **Ctrl+Z annule** l'affectation
- [ ] Affecter sur une ligne composant → message d'erreur explicite
- [ ] Case « Sélection uniquement » : ne lire que quelques éléments sélectionnés
- [ ] Export JSON : l'objet `"article"` est rempli pour les éléments classés
      avec un article connu ; export CSV : colonne Article remplie

### Exports (les deux phases)

- [ ] Export JSON valide (ouvrir dans un éditeur / validator)
- [ ] Export CSV s'ouvre proprement dans Excel (accents, colonnes)

**Retours attendus** : ce qui est faux, manquant, ou lent → ça pilote la suite.

---

## 5. Structure du dépôt

```
COSTWAVES/
├── config.json              # nom, version, langue de l'Add-On
├── CMakeLists.txt           # génération du projet (template officiel Graphisoft)
├── Tools/                   # outils de build Graphisoft (MIT, vendus dans le dépôt)
├── RFIX/
│   ├── AddOnFix.grc         # MDID : Developer ID 944130148 / Local ID 396703700
│   └── Images/              # icône du menu
├── RFIX.win/                # ressource Windows (icône .ico)
├── RINT/
│   └── AddOn.grc            # interface française : menu + fenêtre principale
└── Src/
    ├── AddOnMain.cpp        # points d'entrée de l'Add-On (menu)
    ├── CostWavesDialog.*    # fenêtre : système + tableau + détails + articles + exports
    ├── ModelReader.*        # lecture Archicad (classification, sélection, quantités, composants)
    ├── ArticleManager.*     # articles CostWaves : import JSON, classification, affectation
    ├── Exporter.*           # export JSON / CSV (UTF-8), enrichi de l'article
    ├── DataTypes.hpp        # modèle de données interne
    ├── ResourceIds.hpp
    └── CostWavesPrecompiledHeader.hpp
```

---

## 6. Roadmap (rappel du cadrage)

| Phase | Contenu | Numéros |
|---|---|---|
| **1 (codée)** | Lecture + tableau + export fichiers | 01, 03, 04, 06, 08, 11, 25, export local |
| **2 (codée)** | Articles : import JSON/classification, création de la classification CostWaves, affectation, lecture de la sélection, export enrichi | 13, 14, 16, 17, 30 (variante locale) |
| 3 | Finesse : composites avancés, lecture par lot, tous les types | 09, 10 |
| 4 | Groupes CostWaves, exclusion « consumed », facturation ENS, propriété `CW_Article_ID` | 07, 12, 18–21, 23, 24, 29 |
| 5 | Synchro CostWaves (API `id/name/unit`), détection de modifications | 30, 31, 32, 33 |

Les choix définitifs de contenu des phases suivantes seront revalidés avant codage.

---

## 7. Notes techniques

- **Toutes les signatures API utilisées ont été vérifiées** dans les headers du
  DevKit Archicad 29 (29.3100) — documentation Doxygen + headers réels.
- L'UI utilise `DG::MultiSelListBox` avec en-têtes et colonnes redimensionnables
  (le `ListBrowser` historique n'existe plus dans le DG d'AC29). Les indices
  d'items DG sont **1-based**.
- Le scan est **synchrone** : sur un très gros projet, la fenêtre peut se figer
  quelques secondes pendant la lecture (à optimiser en phase 3 avec
  `ACAPI_Element_GetMoreQuantities`).
- Le GUID de la propriété intégrée « Element ID » est stable
  (`B1B54D45-C951-42C9-9AF8-898F0BF212AB`) ; un repli par recherche de nom est prévu.
- Les composants « properties » (AC 25+) et les « skins » composites sont
  affichés séparément : c'est un point à valider sur un vrai projet (phase 1 =
  détection de l'existant) avant de choisir comment les mapper aux articles.
- **Écritures annulables** : création de classification et affectation
  d'article sont enveloppées dans `ACAPI_CallUndoableCommand` (un seul niveau
  d'undo par action).
- **API 29, écriture** : `ACAPI_Property_CreatePropertyDefinition`,
  `ACAPI_Classification_CreateClassificationSystem/CreateClassificationItem`,
  `ACAPI_Element_AddClassificationItem/RemoveClassificationItem` n'existent
  qu'au niveau **élément** — pas d'écriture sur les composants.
- La sélection courante se lit via `ACAPI_Selection_Get` (`API_Neig.guid`,
  handle du marquee à libérer).
- MDID fournis par Graphisoft (https://archicadapi.graphisoft.com/profile/add-ons) —
  le build « distribution » doit être fait avec `AC_ADDON_FOR_DISTRIBUTION=ON`.
