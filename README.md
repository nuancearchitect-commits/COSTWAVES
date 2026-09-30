# CostWaves × Archicad — Add-On de quantification

Add-On C++ Archicad (**.apx**) qui lit le modèle Archicad et prépare les données
pour **CostWaves** :

> classification → éléments → composants → quantités → tableau → export JSON / CSV.

**Statut : Phase 4 codée** — lecture + articles + composites + **ensembles
facturables (ENS), exclusion « consommé », récapitulatif par article**.
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

## 2. Périmètre codé (phases 1 → 6)

L'Add-On ajoute une commande **« CostWaves – Lecture des quantités… »** (menu Options) qui
ouvre/ferme la **palette CostWaves** (fenêtre flottante *modeless* : elle ne bloque ni la
navigation, ni la sélection dans Archicad, se range dans l'environnement de travail et revient
à sa position d'une session à l'autre).

La palette :

0. **Sources de quantification** (spec §1–§10) — une rangée dédiée :
   - **« Source BIM : Élément / Composants (skins) »** : mode de quantification
     global. **Jamais l'élément ET ses composants comptés simultanément** —
     en mode Élément seules les lignes d'éléments sont affichées/facturées,
     en mode Composants seuls les skins classés le sont. L'Add-On ne présuppose
     jamais la quantification : c'est l'utilisateur qui choisit le mode
     (mur → élément ou skins ; garde-corps → parent ou sous-composants).
   - **Case « Dessins 2D »** : inclut les lignes, polylignes, splines, arcs,
     cercles et hachures dans le métré — **aucune obligation de modéliser en
     3D**. Le popup adjacent filtre par type (Tous / Lignes / Polylignes /
     Splines / Arcs / Cercles / Hachures).
   - Les dessins 2D sont des **objets de métré à part entière** dans le même
     tableau, avec une colonne **Source** (`2D` / `BIM` / `Composant`) et le
     **calque** affiché à la place de l'étage (calques dédiés type
     `CW-METRE-PLINTHE`). Classe d'un dessin 2D = classification **ou**
     propriété `CW_Article_ID` (repli, posée par « Affecter l'article » même
     si la classification échoue).
   - Quantités 2D calculées géométriquement : ligne → longueur ; arc →
     longueur d'arc + rayon ; **cercle → circonférence, surface, rayon,
     diamètre (toutes les mesures conservées)** ; polyligne → arêtes droites
     + arcs (sous-contours respectés) ; spline → longueur de la polyligne
     des points ; **hachure → surface + périmètre** (zones non modélisées :
     peinture, revêtement, terrasse, voirie, étanchéité…).

0b. **Quantités extraites des ouvertures et des objets GDL** —
   - **Fenêtres et portes** (portes-fenêtres comprises) : Surface, Volume,
     Largeur, Hauteur, Surface brute, Hauteur appui, plus les quantités
     dérivées `Contour ouverture` (2×(L+H)), `Épaisseur mur hôte` (mur
     porteur, via le champ owner) et **`Surface tableau` = contour ×
     épaisseur (enduit latéral)** ;
   - **Objets GDL** : Surface, Volume (moteur Archicad) + dimensions
     `Largeur A`, `Profondeur B` (élément) et `Hauteur ZZYZX` (paramètres de
     la bibliothèque, via `APIMemoMask_AddPars`) — toutes exploitables dans
     les règles et formules par article.

1. **Sélecteur de système de classification** — liste tous les systèmes du projet
   (Archicad, CostWaves, Uniclass, etc.) ; changer de système relance la lecture.
2. **Tableau** (en-têtes, **largeurs automatiques**, scroll horizontal,
   colonnes redimensionnables) — **une colonne par type de quantité**
   (Surface, Volume, Longueur 3D, Épaisseur, Surface projetée… selon les
   lignes lues) plus une colonne **Facturé** (quantité facturée au sens du
   métré). **Règle d'affichage : seules les lignes portant une classe
   apparaissent** — un élément sans classe n'est pas affiché (seuls ses
   skins classés le sont, avec les surfaces projetées des skins), et les
   composants « properties » sans quantités ne sont pas affichés :

   | Source | Type | ID élément | Étage / Calque | Classe | Facturé | Surface | Volume | … |
   |---|---|---|---|---|---|---|---|---|
   | BIM | Élément | `W-012` | `0 - RDC` | `CW-MUR - Mur extérieur` | `12,34 m³` | `45,67` | `12,34` | |
   | 2D | Polyline | `PL-01` | `CW-METRE-PLINTHE` | `CW-PLIN - Plinthe` | `42,00 ml` | | | |
   | Groupe n° 1 | | `0 - RDC` | `CW-PORTE - Porte…` | `1 ENS (par groupe)` | | | |
   | Membre (consommé) | `D-007` | `0 - RDC` | `CW-PORTE - …` | `—` | `1,8` | | |
   | Skin — Brique (cœur) | `W-012` | `0 - RDC` | `CW-BRIQ - Brique…` *(classe du matériau)* | `1,23 m³` | `4,56` | `1,23` | |

   - **Type** : `Élément` / `Ensemble` / `Groupe n° n` (lignes facturables
     groupant des membres) / `Membre (consommé)` (élément d'un ensemble ou
     groupe, non facturé seul) / `Skin — <matériau>` (couche d'une structure
     composite, avec marqueur `(cœur)` si la couche fait partie du noyau —
     la colonne Classe porte alors **la classe du matériau**, et la colonne
     Surface projetée la surface du skin)
   - **Détails d'un skin (phase 3)** : nom du **composite**, numéro de couche
     (`couche 2/5`), épaisseur de la couche, marqueurs **cœur / finition**
   - **GUID** : GUID stable de l'élément ou du composant — affiché dans le
     panneau de détails et les exports
   - **ID élément** : propriété intégrée « Element ID » d'Archicad (résolue par son
     GUID intégré `B1B54D45-…`, avec repli par recherche de nom)
   - **Étage** : index + nom de l'étage
   - **Classe** : ID + nom de l'item de classification
   - **Quantités disponibles** : résumé ; **tout** est dans le panneau de détails

3. **Panneau de détails** — ligne sélectionnée : GUID complet, ID, étage, classe,
   toutes les quantités, et pour un composant : ses **propriétés** (lues à la demande).
4. **Boutons** : `Actualiser` · `Exporter JSON` · `Exporter CSV` · `Récapitulatif par article` · `Fermer`.
   Phases 4-5 : `Créer un ensemble` · `Créer un groupe` · `Dissoudre ensemble / groupe` · `Créer le matériau…`.
5. **Ligne d'état** : `N éléments classés · N composants · N skins · N éléments analysés`.
6. **Articles (phase 2)** : case `Sélection uniquement` (cochée par défaut : la palette
 **suit la sélection du plan en direct** — on sélectionne dans Archicad, le tableau s'actualise),
   popup d'articles **hiérarchique** (indenté comme l'arbre des classifications),
   `Affecter l'article` · `Importer des articles…` · `Créer la classification`.
7. **Règles de calcul** (bouton `Règles de calcul…`) — **pour chaque article, définir
   quelle quantité adopter** pour la facturation : une fenêtre dédiée liste les
   articles (ID, libellé, unité, quantité à facturer) et, **selon le type de
   l'article (son unité)**, ne propose que les quantités compatibles lues dans
   le projet :
   - article au **m²** → Surface nette, Surface brute, Surface projetée, Surface
     côté ligne de réf., surfaces des skins… ;
   - article au **m³** → Volume, Volume conditionné, Volume brut… ;
   - article au **ml** → Longueur, Longueur 3D, Périmètre, Circonférence… ;
   - article **ENS/U** → comptage (1 par ligne/groupe), pas de règle ;
   - `Automatique (selon l'unité)` = première quantité de l'unité (défaut) ;
   si la quantité choisie n'existe pas sur une ligne (autre type d'élément),
   repli automatique sur l'unité.
   **Formule dérivée** (champ « Formule dérivée », prioritaire sur le libellé) :
   expression arithmétique sur les libellés des quantités de la ligne —
   `+ - * / ( )`, nombres et variantes `× ÷`. Exemple : un article « enduit
   latéral » au m² se calcule par `Contour ouverture * Épaisseur mur hôte`
   (contour de la fenêtre × épaisseur du mur hôte). La formule est validée à
   l'OK contre les quantités lues dans le projet ; si une ligne ne possède pas
   une quantité référencée (autre type d'élément), repli sur le libellé puis
   sur l'unité.
   La règle s'applique à la colonne `Facturé`, au récapitulatif par article,
   aux exports JSON/CSV et au payload API (`calcQuantity` + `calcFormula` par
   article) ; elle est mémorisée dans `CostWaves-calcul.json` à côté du PLN
   (sections `rules` et `formulas`) et peut aussi venir du catalogue importé
   (champs `calcQuantity`/`quantity` et `calcFormula`/`formula` du JSON).

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

### Ce que la phase 3 ajoute

| Donnée | API utilisée |
|---|---|
| **Lecture par lot** : toutes les quantités d'un même type en un appel (repli unitaire automatique si le lot échoue) | `ACAPI_Element_GetMoreQuantities` |
| Attribut composite d'un élément (mur, dalle, toit, coquille) | `ACAPI_Element_Get` (`.composite` / `.shellBase.composite`) |
| Couches du composite : matériau, épaisseur, cœur/finition | `ACAPI_Attribute_Get` + `ACAPI_Attribute_GetDef` (`cwall_compItems`) + `ACAPI_DisposeAttrDefsHdls` |
| Noms de matériaux / composites / types mis en cache (1 appel par attribut distinct) | caches internes purgés à chaque lecture |
| Quantités des sous-éléments de mur-rideau | montants (`cwFrame`), panneaux (`cwPanel`), accessoires (`cwAccessory`) |
| Quantités des sous-éléments d'escalier | contremarches (`stairRiser`), marches (`stairTread`), structure (`stairStructure`) |
| Quantités des sous-éléments de garde-corps | main courante, lisses, poteaux, balustres, panneaux, segments… (`railing*`) |
| Segments de colonne / poutre | `columnSegment`, `beamSegment` |

Tous les membres de l'union `API_ElementQuantity` sont désormais couverts
(40 types d'éléments au total, contre 18 en phase 1). Les quantités par
*partie* d'élément
(`elemPartQuantities` / `elemPartCompositeQuantity`, ex. segments de mur
multiniveaux) sont lues dans des buffers mais pas encore affichées — piste
pour une phase ultérieure.

### Ce que la phase 4 ajoute — ensembles facturables

| Fonction | Détail |
|---|---|
| **Grouper en ensemble** | Sélection multi-lignes + article du popup → un « ensemble CostWaves » : chaque membre reçoit la classe, `CW_Article_ID` et `CW_Group_ID` (identifiant `CW-G-…` unique). **Une seule commande annulable** (Ctrl+Z dissout tout). |
| **Exclusion « consommé »** | Les membres d'un ensemble ne sont **pas facturés individuellement** : ils apparaissent en sous-lignes `Membre (consommé)` sous leur ligne `Ensemble`, et sortent du récapitulatif et des quantités facturées. |
| **Facturation ENS (forfait)** | Article à l'unité `ENS` (ou vide) : quantité facturée = **1 par ligne** (élément seul ou ensemble). Article à l'unité m²/m³/m/U : quantité = première quantité de l'élément portant cette unité ; pour un ensemble = **somme des quantités de ses membres**. |
| **Dissoudre l'ensemble** | Retire `CW_Group_ID` (valeur vidée) sur les membres — une commande annulable. La classe et l'article sont conservés : les éléments redeviennent facturables individuellement. |
| **Récapitulatif par article** | Bouton dédié → fenêtre (créée en code, sans ressource) listant par article : identifiant, libellé, unité, nombre d'éléments et d'ensembles, **quantité totale facturée**. Les articles inconnus du catalogue apparaissent avec l'unité `?` (pas de total). |
| **Affectation sur un ensemble** | `Affecter l'article` sur une ligne `Ensemble` = changer l'article de **tous ses membres** en une commande ; sur une ligne membre consommé → refus explicite (dissoudre d'abord). |

Propriétés utilisées (groupe « CostWaves », créées à la demande) :
`CW_Article_ID` (déjà phase 2) et **`CW_Group_ID`** (texte, lu au scan —
résolu sans création pour ne rien écrire lors des lectures).

### Ce que la phase 5 ajoute — palette, matériaux, groupes numérotés

| Fonction | Détail |
|---|---|
| **Palette modeless** | La fenêtre principale devient une **palette flottante** : on navigue et on sélectionne dans Archicad pendant qu'elle reste ouverte (elle se masque automatiquement pendant les opérations qui le demandent, et mémorise sa position). La case `Sélection uniquement` est cochée par défaut et la palette **s'actualise à chaque changement de sélection**. |
| **Tableau 6 colonnes** | Correctif : les colonnes `GUID`, `ID élément`, `Étage`, `Classe` et `Quantités` s'affichent désormais (les champs de tabulation n'étaient pas créés). Même correctif sur le récapitulatif. |
| **Créer le matériau…** | Nouvelle fenêtre : nom du matériau, **nouvelle classe** (système + classe parente + **ID pré-rempli au premier disponible parmi les enfants** + nom) ou **classe existante** (sélecteur), puis **hachure** (remplissage en coupe), **surface de coupe**, **stylos avant/arrière-plan**, **puissance** (priorité de connexion 1–1000). Le matériau est créé — ou mis à jour s'il existe déjà — et **lié à la classe** (qui sert d'article dans le métré). |
| **Créer un ensemble** | Nouveau flux : on sélectionne des éléments **dans le plan**, on clique, une fenêtre propose de choisir **l'article (classe)** ; chaque membre reçoit la classe, `CW_Article_ID` et `CW_Group_ID` (identifiant `CW-E-…`). Une seule commande annulable. |
| **Créer un groupe** | Même flux que l'ensemble, mais chaque groupe reçoit un **numéro** (`CW-N-1`, `CW-N-2`, …). **La quantité réelle du métré est le nombre de groupes** de l'article : 3 groupes créés = quantité 3 (les membres sont consommés, comme pour les ensembles). |
| **Dissoudre ensemble / groupe** | Fonctionne sur les lignes `Ensemble` et `Groupe n° …` (ou leurs membres) : retire `CW_Group_ID`, une commande annulable. |
| **Récapitulatif enrichi** | Colonnes `Éléments` · `Ensembles` · `Groupes` · `Skins` · `Quantité totale` ; les exports JSON/CSV portent `groupType` (`ensemble`/`numbered`), `groupNumber`, `numberedGroups`, `numberedGroupCount` et `skinCount`. |
| **Skins classés (révision)** | Un élément sans classe dont au moins une couche a un **matériau classé** est appelé ; chaque skin porte la classe de son matériau (colonne Classe), est facturé sur l'article de ce matériau (colonne Facturé, récapitulatif `Skins`) et l'export JSON/CSV porte sa classification. Seules les lignes classées sont affichées. |
| **Fenêtres en ressources GRC** | Les fenêtres « Créer le matériau… » (réorganisée : une ligne par contrôle, plus de cases masquées), choix d'article et récapitulatif sont définies en GRC (elles s'ouvraient vides en création programmatique). |
| **Tableau (révision)** | Largeurs de colonnes **automatiques** (contenus + en-têtes) et **scroll horizontal** (HVScroll) ; surfaces projetées des skins dans leur colonne ; composants « properties » retirés de l'affichage. |
| **Ensemble/groupe (correctif)** | Création corrigée : valeur par défaut explicite lors de la création des propriétés `CW_Article_ID`/`CW_Group_ID` (le variant laissé « indéfini » faisait échouer la création), résolution de l'article dans **tous** les systèmes de classification, et création automatique du système/item « CostWaves » si l'article importé (JSON) n'existe dans aucune classification. |


Fichiers écrits **à côté du .PLN** (ou dans *Documents* si projet non enregistré) :

- `<Projet>_CostWaves_<AAAAMMJJ_HHMMSS>.json` — structure complète :
  éléments, classe, **article** (id/name/unit reconnu d'après la classe),
  étage, quantités, skins (matériau + volume + surface projetée +, si la
  structure composite est reconnue : `composite`, `skinIndex`/`skinCount`,
  `core`, `finish`), composants (GUID + propriétés) ;
  **(phase 4)** bloc `summary` (totaux facturés par article), blocs
  `{ "kind": "group", "groupId", "billedQuantity", "billedUnit", "members": […] }`
  pour les ensembles et `"groupId"`/`"consumed": true` sur les membres.
  Format d'échange pour CostWaves.
- `<Projet>_CostWaves_<AAAAMMJJ_HHMMSS>.csv` — **format long** (une ligne par
  quantité : `Type;GUID;ID élément;Étage;Classe;Article;Libellé;Valeur;Unité`),
  séparateur `;`, UTF-8 avec BOM → s'ouvre directement dans Excel. Les lignes
  skins portent le matériau et le composite ; **(phase 4)** lignes `Ensemble`
  (avec `Quantité facturée`), lignes `Membre (consommé)` et bloc final
  `Récapitulatif` (une ligne par article).

### Phase 2 : articles CostWaves (codée)

| Fonction | Comment ça marche |
|---|---|
| **Source des articles** | Au choix : (a) **import d'un fichier JSON** `[{"id":"CW-MUR","name":"Mur extérieur","unit":"m2"}, …]` (ou `{"articles":[…]}`) via « Importer des articles… » ; (b) à défaut, **les items du système de classification choisi** servent d'articles (id de l'item = id d'article). L'import JSON a priorité. |
| **Créer la classification** | Bouton « Créer la classification » : crée (si absente) un système de classification **« CostWaves »** avec **un item par article** (item.id = article.id, item.name = article.name), puis complète les items manquants. Opération **annulable** (Ctrl+Z). |
| **Affecter un article** | Sélectionnez une ou plusieurs lignes **élément** (Ctrl+clic) dans le tableau, choisissez l'article, cliquez « Affecter l'article » : chaque élément reçoit l'item de classification correspondant (sa classe précédente dans le système est remplacée). **Une seule commande annulable** (un seul Ctrl+Z) pour toute la sélection ; les échecs individuels sont comptés sans tout arrêter. |
| **Lecture de la sélection** | Case « Sélection uniquement » : la lecture (Actualiser / changement de système) n'analyse que les éléments sélectionnés dans Archicad — pratique sur les gros projets. |
| **Créer les matériaux** | Pour **chaque article** : crée le matériau de construction `id — nom` (idempotent : un matériau de ce nom n'est pas recréé), crée son item dans le système « CostWaves » s'il manque, et **affecte l'item au matériau** (remplace sa classe précédente dans ce système). ⚠️ La création de matériaux n'est **pas annulable** (limite API Archicad). |
| **Propriété CW_Article_ID** | « Affecter l'article » écrit aussi l'identifiant d'article dans la propriété texte `CW_Article_ID` (groupe « CostWaves », créés automatiquement si absents) — visible dans les nomenclatures et les exports Archicad. |
| **Recherche** | Champ de recherche au-dessus du tableau : filtre en direct (non sensible à la casse) sur type, GUID, ID élément, étage et classe. |
| **Tri du tableau** | Clic sur un en-tête de colonne pour trier (croissant/décroissant, flèche dans l'en-tête) ; les composants restent rattachés à leur élément. |
| **Export enrichi** | Colonne `Article` (CSV) et objet `"article"` (JSON) : remplis quand la classe de l'élément correspond à un article connu. |

**Limite connue** : l'API Archicad ne permet d'écrire ni propriété ni
classification au niveau des **composants** — l'affectation se fait sur
l'**élément** (ses composants suivent). C'est une contrainte de l'API 29,
pas un choix.

---

### Phase 6 — Communication Archicad → CostWaves

1. Bouton « Envoyer vers CostWaves… » (bas de palette) et commande de menu
   « CostWaves → Envoyer vers CostWaves… » : la fenêtre de réglages s'ouvre.
2. URL vide → validation refusée avec message ; URL invalide → erreur claire
   après clic sur Envoyer (hôte injoignable / URL invalide).
3. Renseigner l'URL de l'API d'import + la clé API, envoyer : vérifier côté
   serveur le payload (projectId, articles, summary, elements avec quantités
   normalisées et components skins classés, membres consommés absents).
4. Articles inconnus : sélectionner chaque mode dans la fenêtre (projet
   uniquement par défaut) et vérifier `unknownArticleMode` dans le payload ;
   la réponse du serveur listant `unknownArticles` s'affiche dans le bilan.
5. Fermer/réouvrir : `CostWaves-settings.json` à côté du PLN retient URL,
   clé et mode.
6. Projet non enregistré : l'envoi utilise « SansTitre » (repli Documents).

### Phase 7 — Sources de quantification (mode + dessins 2D)

1. Mode **Élément** : un mur classé à skins classés → une seule ligne élément
   facturée ; passer en **Composants (skins)** → la ligne élément disparaît du
   tableau, seuls les skins classés restent (jamais les deux à la fois).
2. Vérifier le récapitulatif par article, l'export JSON/CSV et le payload API
   (`quantificationMode`) dans les deux modes.
3. Cocher **« Dessins 2D »** : tracer une ligne, une polyligne (avec arcs),
   une spline, un arc, un cercle, une hachure sur un calque `CW-METRE-…`,
   affecter un article → lignes `2D` dans le tableau avec calque, quantités
   (longueur / circonférence / surface / rayon / diamètre / périmètre).
4. Cercle : les quatre mesures (circonférence, surface, rayon, diamètre)
   apparaissent ; l'article choisit celle à facturer via son unité.
5. Popup de filtre 2D : restreindre à un type → seules les lignes de ce type
   restent ; décocher la case → retour au BIM seul.
6. Exports : colonnes `Source` et `Calque` dans le CSV, `classified2D` dans le
   JSON, champs `source`/`layer` par élément dans le payload API.

### Phase 7 bis — Règles de calcul (quantité à adopter par article)

1. Ouvrir « Règles de calcul… » : le tableau liste les articles ; sélectionner
   un article au m² → le popup ne propose que les surfaces lues dans le projet
   (Surface nette, Surface brute, Surface projetée…) ; un article au m³ → les
   volumes ; un article ENS → « (comptage) ».
2. Choisir « Surface nette » pour un article de mur : la colonne `Facturé` du
   tableau, le récapitulatif par article et l'export CSV/JSON basculent sur la
   surface nette ; choisir « Volume conditionné » pour un article au m³ et
   vérifier de même.
3. Un même article appliqué à des éléments sans cette quantité (ex. hachure
   pour un article « Surface nette ») : repli automatique sur l'unité.
4. OK → fermer/réouvrir Archicad : les règles sont conservées
   (`CostWaves-calcul.json` à côté du PLN) ; Annuler → aucune modification.
5. Catalogue JSON avec `"calcQuantity": "Surface brute"` : la règle est
   appliquée dès l'import ; le champ `calcQuantity` part dans le payload API.

### Phase 7 ter — Quantités dérivées et extraction ouvertures/objets GDL

1. Fenêtre posée dans un mur : la ligne porte `Contour ouverture`,
   `Épaisseur mur hôte` et `Surface tableau` (contour × épaisseur) dans ses
   colonnes de quantités ; même chose pour une porte/porte-fenêtre.
2. Article « Enduit latéral » (m²) : dans « Règles de calcul… », saisir la
   formule `Contour ouverture * Épaisseur mur hôte` → OK : la colonne
   `Facturé` des fenêtres affiche la surface du tableau ; une formule
   invalide (libellé inconnu, parenthèse manquante) bloque la fermeture avec
   un message.
3. Objet GDL classé (sanitaire, mobilier…) : la ligne affiche `Largeur A`,
   `Profondeur B`, `Hauteur ZZYZX` en plus de Surface/Volume.
4. Fermer/réouvrir : les formules sont conservées
   (`CostWaves-calcul.json`, section `formulas`) et partent dans le payload
   API (`calcFormula`).

### Étape courante — NUANCE BIM > COSTWAVES > palette des correspondances

Menu **NUANCE BIM** (barre de menus) → **COSTWAVES…** ouvre la **palette**
(inspirée de la maquette CostWaves V4 : titre, sous-titre, section
CORRESPONDANCES, boutons avec description, ligne d'état) :

- **Matériaux, composites et profils…** : fenêtre des correspondances
  attributs → articles ;
- **Objets GDL…** : correspondances objet de bibliothèque → article
  + valeur clé ;
- **Articles hérités…** : booléen activé (global) → article + valeur clé.

Bouton **« Matériaux, composites et profils… »** :

- **Filtre Type** : Matériau / Composite / Profil complexe ;
- **Filtre Classification** : système dont les classes sont les articles ;
- **Matériaux** : clic sur la ligne → choix de la classe (fenêtre avec
  recherche) ; sans classe = ignoré du métré ;
- **Composites et profils** : case à cocher en bout de ligne — le clic ouvre
  le choix de la classe ; cochée = la colonne suivante affiche l'article ;
  décochée = « quantifié par matériau décomposé » (italique) : la structure
  n'est pas comptée comme un tout, le métré passe par les matériaux de ses
  couches ;
- **Valeur clé** (dernière colonne de chaque ligne) : le clic ouvre le choix
  d'un paramètre différenciant (recherche + filtre de groupe) — deux groupes
  de clés calculées par COSTWAVES : **Composant** (épaisseur de l'élément,
  épaisseur totale du composite) et **Couche** (épaisseur de la couche du
  matériau, position de la couche, nombre de couches) ; « — (aucune) »
  retire la clé. La valeur de la clé sera exploitée au métré
  (différenciation des articles — étape suivante) ;
- **Objets GDL** (bouton « Objets GDL… » sur la palette) : fenêtre autonome avec son système de
  classification ; « Ajouter… » enchaîne : choix de l'objet (.gsm posables
  chargés, recherche), sa classe (article), puis sa valeur clé parmi les
  **variables GDL de type longueur** de l'objet (épaisseur, hauteur,
  dimensions… — libellé + nom GDL) ; la classe « (aucune) » retire la
  correspondance. Règles enregistrées dans la même bibliothèque
  (type « object ») ;
- **colonne Type** dans les sélecteurs de paramètres GDL (double
  vérification) : TOUS les paramètres sont listés avec leur type GDL
  (longueur, bool, entier, texte…) ; ceux du type attendu apparaissent en
  tête avec le type en gras ;
- **filtre par type** dans le sélecteur de paramètres : une rangée de
  boutons radio sous la recherche (« Tous » + un bouton par type
  réellement présent) filtre la liste par type — **rien n'est filtré à
  l'ouverture** (« Tous » sélectionné), le filtre ne s'applique que sur
  un clic et se combine avec la recherche (l'entrée « (aucune) » reste
  toujours visible) ;
- **style CostWaves** (couleurs de la maquette V4, sans copier ses
  écrans) : palette avec **bande de titre sombre** (#101826, titre blanc
  en gras) qui suit la largeur, libellé de section et lignes d'état en
  **accent bleu** #155eef (état en pastille fond #e8f0fe), textes d'aide
  gris #6a7b90 ; dans les listes : articles en bleu accent, valeurs clés
  en vert (#027a48), cellules vides « — » et « (aucune) » en gris,
  colonne Type du sélecteur en pastille accent pour le type attendu ;
- **Articles hérités** (bouton « Articles hérités » sur la palette) : un
  article hérité naît d'un **paramètre booléen activé** (tablette, seuil,
  volet…), **quel que soit l'objet qui le porte** — le même nom de
  paramètre se répète entre objets, la règle est donc globale
  (booléen → article, sans lien d'objet ; l'objet du flux « Ajouter… »
  sert uniquement à lister les paramètres) ; une **valeur clé**
  facultative (variable GDL de type longueur de l'objet, comme dans les
  correspondances objets GDL) différencie les variantes de l'article
  hérité (Ø125/Ø160, H8/H12…) ; « (aucune) » retire la
  règle ;
- **Tableau des matériaux** du projet (Matériau | Classe) ;
- **clic sur un matériau** → ouvre la fenêtre de choix de la classe :
  liste **« ID — Nom »** avec **barre de recherche en haut** (style
  sélecteur d'attributs Archicad, liste indentée comme la classification) —
  double-clic ou « Choisir » valide ; **« — (aucune) »** retire la
  correspondance (un matériau sans classe est déjà ignoré du métré — pas
  d'option Ignorer) ;
- **Enregistrer** : confirmation avec le chemin
  (`<Documents>/CostWaves-regles.json`, réutilisable entre projets) ;
  Fermer enregistre aussi.

Format : `{"rules": [{"type": "material", "name": "BETON_25",
"article": "CW-030", "mode": "element"}]}`.

*Correctif persistance* : les valeurs du JSON sont maintenant écrites entre
guillemets (le fichier précédemment écrit était invalide et illisible au
rechargement — changements perdus) ; un échec de lecture de la
bibliothèque est désormais signalé à l'ouverture de la fenêtre.

## 2bis. Fenêtre « Quantitatif » — contrôle des quantités avant export

Bouton **« Quantitatif… »** sur la palette : fenêtre séparée dédiée au
contrôle et au calcul des quantités **avant export vers CostWaves**.
Chaque ligne du tableau = **un article CostWaves effectivement quantifié**
dans la maquette (règles matériaux/composites/profils/objets GDL +
classification) :

| Colonne | Contenu |
|---|---|
| **Article** | `id — libellé` de la base CostWaves |
| **Source** | origine Archicad (cliquable — voir traçabilité) : « Matériau — Béton 25 », « Composite — MUR_EXT_30 », « Objet GDL — Fenêtre PVC »… ; « (+N) » si l'article a plusieurs origines |
| **Unité** | unité de l'article, **modifiable** (m², ml, m³, u, kg) — le changement adapte la dimension et les paramètres de calcul |
| **Mode calcul** | **Brute / Conditionnelle / Nette** pour les unités géométriques (m², ml, m³) ; les unités non géométriques (u, kg…) sont comptées (pas de boutons radio affichés) |
| **Quantité** | quantité retenue ; « * » = corrigée manuellement |

- **Moteur aligné sur la correspondance** : pour chaque élément, le moteur
  cherche la règle dans la bibliothèque (fenêtre « Matériaux, composites et
  profils »). Un **composite/profil avec article** est calculé **pour
  lui-même** (mode Élément de la règle) ou **par ses couches** (mode
  Composants — chaque couche est facturée sur l'article de la règle de son
  matériau) ; un **composite/profil sans article** est « quantifié par
  matériau décomposé » → ses couches uniquement ; un **objet GDL** est
  calculé pour lui-même. **Jamais l'élément ET ses couches à la fois.** La
  traçabilité (clic Source) affiche le mode de la règle (« calculé pour
  lui-même ») et, pour une couche, le composite d'où elle vient (« via
  MUR_EXT_30 ») ;
- **Mode de calcul** (panneau du bas, article sélectionné) :
  - **Brute** — géométrie principale de l'élément (surface de référence,
    volume, longueur) ;
  - **Conditionnelle** — conditions de l'article : le **volume conditionné**
    des connexions pour le m³ (note « aucune condition disponible » sinon) ;
  - **Nette** — après déductions : **surface brute − ouvertures − exclusions** ;
    les déductions sont des **paramètres visibles et contrôlables**
    (cases « Déduire les ouvertures (fenêtres, portes) », « Déduire les
    trous ») ;
  - la quantité est **recalculée automatiquement** à chaque changement de
    mode, d'unité ou de paramètre.
- **Quantité retenue vs calculée** : correction manuelle possible (« Retenir »)
  ; « Réinitialiser » revient à la valeur calculée ; **la quantité calculée
  d'origine n'est jamais perdue** (affichée en permanence dans le panneau).
- **Traçabilité** (clic sur la colonne **Source**) : chaîne complète
  *Article CostWaves → Mapping → Sources Archicad (avec sous-totaux par
  source et valeur clé) → Mode de calcul → Paramètres → Quantité calculée /
  retenue* — comprendre **pourquoi** une quantité a été obtenue avant de
  l'envoyer.
- **Recalculer** relit la maquette ; les réglages (unité, mode, déductions)
  et les corrections manuelles sont conservés par article pendant la
  session (persistance sur fichier : prochaine étape).

*Limites actuelles (incrément 1)* : les **articles hérités** (booléens) ne
sont pas encore comptés dans la lecture (ils le seront dès que le lecteur
appliquera ces règles) ; ensembles/groupes non consommés (lecture sans
propriété de groupe) ; « Conditionnelle » n'exploite que le volume
conditionné ; les réglages ne survivent pas à la fermeture de la fenêtre.

## 3. Build (Windows)

### Télécharger le .apx déjà compilé (recommandé)

**Chaque tâche livrée est compilée automatiquement par GitHub Actions**
(runner Windows + DevKit officiel GRAPHISOFT 29.3100) et le fichier
`CostWaves-AC29.apx` est publié dans la release **`apx-latest`** :

> **https://github.com/nuancearchitect-commits/COSTWAVES/releases**

- Fichier à télécharger : `CostWaves-AC29.apx` (release « CostWaves — Add-On
  Archicad 29 (.apx) ») — toujours à jour avec le dernier commit poussé.
- Historique : chaque build est aussi archivé dans les **artifacts** des runs
  Actions (90 jours).
- Installation : voir « Installation dans Archicad » ci-dessous.
- Le build est déclenché à chaque push sur la branche de travail (fin de
  tâche = nouveau .apx), ou manuellement (*Actions > Build APX > Run workflow*).

### Prérequis (build local)

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
2. « Ajouter » → sélectionner `CostWaves-AC29.apx` (téléchargé depuis la
   release `apx-latest`, cf. ci-dessus) ou `CostWaves.apx` (build local).
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

### Phase 2 — matériaux & propriété

- [ ] « Créer le matériau… » (voir plan de test phase 5) : le matériau
      apparaît dans Options > Gestionnaire de matériaux de construction et
      porte la classe choisie (Options > Classifications)
- [ ] « Affecter l'article » : la propriété `CW_Article_ID` (groupe CostWaves)
      apparaît sur l'élément avec l'identifiant d'article (sélectionner
      l'élément > Paramètres / nomenclature)
- [ ] Recherche : taper un fragment (ex. `mur`, un étage, un ID) filtre le
      tableau en direct ; vider le champ restaure tout
- [ ] Tri : cliquer sur « Étage » ou « Classe » trie le tableau (flèche dans
      l'en-tête, second clic = sens inverse) ; les composants restent sous
      leur élément
- [ ] Affectation multiple : Ctrl+clic sur plusieurs lignes d'éléments puis
      « Affecter l'article » → toutes les lignes sont classées, message avec
      le décompte, **un seul Ctrl+Z** annule tout

### Phase 3 — composites avancés, tous types, performance

- [ ] Un mur **composite** classé : chaque skin affiche `Skin — <matériau>`
      et, dans les détails, le nom du composite, `couche i/n`, l'épaisseur
      (mm) et le marqueur **cœur** sur les couches du noyau (vérifier contre
      Options > Composites : ordre, épaisseurs, matériau)
- [ ] Idem sur une **dalle**, un **toit** et une **coquille** composites
- [ ] Une couche de finition (enduit/isolant côté finition) porte le
      marqueur **finition** dans les détails
- [ ] Épaisseurs de skins cohérentes : `Épaisseur 200 mm` pour une couche
      réglée à 20 cm dans le composite
- [ ] Export JSON : les skins d'un mur composite portent `"composite"`,
      `"skinIndex"`, `"skinCount"`, `"core": true` (couches du noyau) ;
      l'épaisseur apparaît dans `"quantities"` (unité `mm`)
- [ ] Export CSV : lignes skins avec `Composant (skin) — <matériau> · <composite> (cœur)`
      et une ligne `Épaisseur;200;mm`
- [ ] **Nouveaux types couverts** : classer un montant de mur-rideau, un
      panneau de mur-rideau, une contremarche/marche d'escalier, un segment
      de garde-corps (via la classification par élément) → les quantités
      (volume, longueur 3D…) apparaissent au lieu de « — »
- [ ] **Porte / fenêtre / lampe** : les quantités apparaissent (les portes et
      lampes lisaient le mauvais membre de l'union avant la phase 3)
- [ ] **Performance** : sur un gros projet, la lecture est plus rapide qu'en
      phase 2 (quantités lues par lot, un appel par type ; noms de matériaux
      et composites mis en cache) ; pas de gel anormal de la fenêtre

### Phase 4 — ensembles, consommés, ENS, récapitulatif

- [ ] Sélectionner 2-3 éléments **dans le plan**, cliquer « Créer un
      ensemble », choisir un article **à l'unité ENS** (ou vide) → message avec
      l'identifiant `CW-E-…`, le tableau montre une ligne `Ensemble` suivi de ses membres
      `Membre (consommé)` ; **Ctrl+Z annule tout** (les éléments redeviennent
      des lignes `Élément` normales)
- [ ] Les membres portent `CW_Group_ID` (sélectionner un élément dans Archicad >
      Paramètres > propriétés CostWaves) avec le même identifiant
- [ ] « Récapitulatif par article » : l'ensemble compte pour **1 ENS** dans le
      total de son article ; les membres consommés n'apparaissent pas
      individuellement
- [ ] Grouper avec un article **en m²/m³/m** : la quantité facturée de
      l'ensemble = somme des quantités des membres dans cette unité (ex. deux
      portes → somme des surfaces)
- [ ] « Affecter l'article » sur une ligne `Ensemble` → tous les membres
      changent d'article (un seul Ctrl+Z) ; sur une ligne membre → message
      d'erreur explicite
- [ ] « Dissoudre l'ensemble » (depuis la ligne Ensemble ou un membre) → les
      éléments redeviennent facturables individuellement, classe/article
      conservés ; Ctrl+Z restaure l'ensemble
- [ ] Ligne d'état : `N ensemble(s) · N consommé(s)` apparaît après regroupement
- [ ] Export JSON : bloc `summary` + bloc `"kind": "group"` avec `members` ;
      export CSV : lignes `Ensemble`/`Membre (consommé)` et bloc final
      `Récapitulatif`
- [ ] Recréer un ensemble dans la même seconde → identifiants distincts
      (suffixe `-2`)

### Phase 5 — palette, matériaux, groupes numérotés

- [ ] La fenêtre s'ouvre depuis le menu et **ne bloque pas** : on peut zoomer,
      sélectionner, éditer pendant qu'elle est ouverte ; le menu la bascule
      (afficher/masquer), la croix la masque aussi
- [ ] **Suivi de sélection** : cocher `Sélection uniquement` (défaut), cliquer des
      éléments dans le plan → le tableau se met à jour à chaque clic ; décocher →
      le tableau redevient statique (bouton `Actualiser`)
- [ ] **Tableau complet** : les colonnes GUID, ID élément, Étage, Classe et
      Quantités sont remplies (plus seulement « Type »)
- [ ] **Créer le matériau…** : saisir un nom, cocher « nouvelle classe »,
      choisir le système et la classe parente → l'ID proposé est le premier
      disponible parmi les enfants ; choisir hachure, surface, stylos, puissance →
      le matériau apparaît avec ces réglages et porte la classe ; recliquer avec
      le même nom → le matériau est **mis à jour** (attributs modifiés)
- [ ] **Créer le matériau…** avec « classe existante » : le matériau est lié à la
      classe choisie sans créer d'item
- [ ] **Créer un ensemble** : sélection dans le plan → bouton → choix de
      l'article → ligne `Ensemble` + membres consommés (comme phase 4) ;
      Ctrl+Z annule tout
- [ ] **Créer un groupe** : sélectionner des éléments dans le plan, bouton,
      article → message `Groupe n° 1` ; recommencer avec d'autres éléments →
      `Groupe n° 2`, etc. ; le récapitulatif montre **quantité = nombre de
      groupes** (3 groupes → 3) ; Ctrl+Z annule
- [ ] **Dissoudre** fonctionne sur une ligne `Groupe n° …` comme sur `Ensemble`
- [ ] Récapitulatif : colonnes Éléments / Ensembles / Groupes / Quantité totale
      toutes remplies ; exports JSON/CSV avec `groupType`, `groupNumber`)

### Exports (toutes phases)

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
│   └── AddOn.grc            # interface française : menu + palette principale (GDLG Palette)
└── Src/
    ├── AddOnMain.cpp        # points d'entrée (menu, palette, suivi de sélection)
    ├── CostWavesDialog.*    # palette : système + tableau + détails + articles + exports
    ├── ArticlePickerDialog.*# choix de l'article pour « Créer un ensemble / un groupe »
    ├── MaterialDialog.*     # fenêtre « Créer le matériau… » (classe + attributs)
    ├── ModelReader.*        # lecture Archicad (classification, sélection, quantités, composants)
    ├── ArticleManager.*     # articles, classification, matériaux, ensembles/groupes
    ├── SummaryDialog.*      # récapitulatif par article
    ├── SendDialog.*         # fenêtre « Envoyer vers CostWaves » (URL, clé, articles inconnus)
    ├── CostWavesApi.*       # communication CostWaves (payload §13, WinHTTP, réglages)
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
| **3 (codée)** | Finesse : composites avancés (skins enrichis), lecture par lot, tous les types d'éléments | 09, 10 |
| **4 (codée)** | Ensembles CostWaves facturables (exclusion « consommé » automatique), facturation ENS, récapitulatif par article | 07, 12, 18–21, 23, 24, 29 |
| **5 (codée)** | Palette modeless (navigation/sélection libres, suivi de sélection), tableau 6 colonnes, fenêtre « Créer le matériau… », « Créer un ensemble » / « Créer un groupe » (groupes numérotés, quantité = nombre de groupes) | 02, 05, 15, 22, 26–28 |
| **6 (codée)** | Communication Archicad → CostWaves (spéc. §12/§13) : bouton « Envoyer vers CostWaves… » + commande de menu, fenêtre de réglages (URL, clé API, traitement des articles inconnus §6), payload JSON complet, envoi HTTP(S) via WinHTTP | 30, 31, 32, 33 |
| **7 (codée)** | Sources de quantification (spéc. repostée §1–§10) : mode **Élément / Composants** exclusif, **dessins 2D** (ligne, polyligne, spline, arc, cercle, hachure) comme objets de métré à part entière dans le même tableau (colonne Source), filtre par type 2D, classe 2D = classification ou propriété CW_Article_ID  ; **règles de calcul** : quantité à adopter par article (nette, brute, conditionnée, projetée…) et **formules dérivées** (`Contour ouverture * Épaisseur mur hôte` → enduit latéral) dans la fenêtre « Règles de calcul… » ; extraction enrichie des **portes/fenêtres** (contour, épaisseur mur hôte, surface tableau) et des **objets GDL** (A/B/ZZYZX) | — |
| 8 | Synchro bidirectionnelle CostWaves ↔ Archicad, détection de modifications de quantités (le module `CostWavesApi` isole déjà le transport) | |

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
- **Ensembles (phase 4)** : l'appartenance est portée par la propriété texte
  `CW_Group_ID` sur chaque membre (pas d'élément parent dans Archicad — la
  ligne « Ensemble » est virtuelle, reconstruite au scan). La dissolution vide
  la valeur : il n'existe pas d'API pour « détacher » une valeur de propriété
  d'un élément (`ACAPI_Element_SetProperty` avec texte vide). Le guid de la
  définition est résolu **sans création** au scan (aucune écriture à la lecture).
- **Communication CostWaves (spéc. §12/§13, phase 6)** : le module `CostWavesApi`
  (`Src/CostWavesApi.hpp/.cpp`) isole tout le transport — payload, envoi,
  analyse de réponse — pour préparer la synchro bidirectionnelle sans toucher
  au reste de l'Add-On. Détails :
  - **Payload** (§13) : `projectId`/`projectName` (nom du PLN), catalogue
    `articles` (id/name/unit), `summary` facturé par article (compteurs
    éléments/ensembles/groupes/skins + quantité totale), puis `elements` :
    éléments classés, ensembles et groupes numérotés (membres consommés exclus,
    §8/§9) avec classe, article, quantité facturée, unité, **toutes les quantités**
    (clés normalisées : `surface`, `volume`, `length3d`, `thickness`,
    `projectedSurface`…) et `components` = skins classés (§4) avec la classe de
    leur matériau. Le format définitif sera figé avec l'API CostWaves.
  - **Envoi** : `POST` HTTP(S) via **WinHTTP** (Windows seul, lié par
    `#pragma comment (lib, "winhttp.lib")`), en-têtes `Content-Type:
    application/json` + `Authorization: Bearer <clé API>`, délais 10/30 s.
    Appel **bloquant** pendant l'envoi (MVP ; une alerte prévient l'utilisateur).
  - **Réponse** : champs lus s'ils sont présents — `createdLines`,
    `updatedLines`, `unknownArticles` (§6 : le mode — projet uniquement par
    défaut, base + projet, ignorer — est choisi dans la fenêtre d'envoi et
    transmis dans `unknownArticleMode`), `message`.
  - **Réglages** : `CostWaves-settings.json` écrit à côté du PLN (URL, clé API,
    mode articles inconnus) — rechargés à chaque ouverture de la fenêtre.
- **Palette (phase 5)** : la fenêtre principale est un `DG::Palette` (GDLG `Palette`),
  singleton enregistré par `ACAPI_RegisterModelessWindow` (messages `APIPalMsg_*`,
  mémorisation de la position). Le suivi de la sélection utilise
  `ACAPI_Notification_CatchSelectionChange` : la palette ne se réactualise que si elle
  est visible et si `Sélection uniquement` est cochée (garde anti-réentrance).
- **Groupes numérotés (phase 5)** : la valeur `CW_Group_ID` encode le type —
  `CW-N-<n>` = groupe numéroté n° n (quantité facturée = 1 par groupe, le nombre de
  groupes de l'article est la quantité réelle du métré) ; toute autre valeur non vide
  (`CW-E-…` nouveaux, `CW-G-…` historiques) = ensemble (facturation phase 4 inchangée).
  Le prochain numéro = max des numéros existants + 1 (lecture de `CW_Group_ID` sur
  tous les éléments du projet au moment de la création).
- **Matériaux (phase 5)** : `ACAPI_Attribute_Create` / `Modify` sur
  `API_BuildingMaterialID` avec `connPriority` (puissance), `cutFill` (hachure),
  `cutFillPen`/`cutFillBackgroundPen` (stylos), `cutMaterial` (surface de coupe) ;
  la classe est affectée au matériau par `ACAPI_Attribute_AddClassificationItem`.
  La création d'attributs n'est pas annulable (limite API).
- **Fenêtre récapitulative** : créée entièrement en code
  (`DG::ModalDialog` programmatique + `DG::MultiSelListBox` avec en-têtes
  posés par `SetHeaderItemCount`/`SetHeaderItemText`/`SetTabFieldProperties`),
  sans ressource GRC.
- **Skins ↔ couches du composite (phase 3)** : `API_CompositeQuantity`
  n'identifie pas sa couche (le `compositeId` ne porte que le GUID du
  sous-élément). La correspondance se fait donc par **position dans le
  composite**, avec repli sur le **matériau** (`buildMatIndices`) si l'ordre
  ne correspond pas ; sans correspondance fiable, le skin garde ses quantités
  (volume, surface projetée) sans enrichissement. L'épaisseur de couche
  (`fillThick`, en mètres dans l'API) est affichée/exportée en **mm**.
- **Écritures annulables** : création de classification, affectation d'article
  (classe + propriété `CW_Article_ID`) sont enveloppées dans
  `ACAPI_CallUndoableCommand` (un seul niveau d'undo par action).
  ⚠️ La **création de matériaux de construction** n'est pas annulable
  (limite documentée de `ACAPI_Attribute_Create`) — mais elle est idempotente
  par nom.
- **API 29, écriture** : `ACAPI_Property_CreatePropertyGroup/CreatePropertyDefinition`,
  `ACAPI_Element_SetProperty`, `ACAPI_Classification_CreateClassificationSystem/CreateClassificationItem`,
  `ACAPI_Element_AddClassificationItem/RemoveClassificationItem` n'existent
  qu'au niveau **élément** — pas d'écriture sur les composants. La
  classification des **attributs** (matériaux) passe par
  `ACAPI_Attribute_AddClassificationItem/RemoveClassificationItem`.
- La sélection courante se lit via `ACAPI_Selection_Get` (`API_Neig.guid`,
  handle du marquee à libérer).
- MDID fournis par Graphisoft (https://archicadapi.graphisoft.com/profile/add-ons) —
  le build « distribution » doit être fait avec `AC_ADDON_FOR_DISTRIBUTION=ON`.
