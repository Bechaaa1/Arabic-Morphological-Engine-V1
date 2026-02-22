# محرك البحث الصرفي العربي
## Arabic Morphological Search Engine & Derivation Generator

> **Mini-projet Algorithmique — ISI, Génie Logiciel et Systèmes d'Information**
> Année universitaire 2025-2026

---

## 📋 Aperçu

Ce projet implémente un **moteur morphologique complet pour la langue arabe**, basé sur le système racine–schème (Root–Pattern). Il combine un **arbre AVL** pour l'indexation des racines et une **table de hachage** pour la gestion des schèmes, offrant une efficacité algorithmique O(log n) et O(1) respectivement.

L'interface utilisateur est une **application web moderne** servie directement par l'exécutable C++, accessible via navigateur sur `http://localhost:8080`.

---

## 🏗️ Architecture

```
arabic-morpho-engine/
├── include/
│   ├── ArabicUtils.h        ← Utilitaires UTF-8 & algorithmes morphologiques
│   ├── AVLTree.h            ← Arbre AVL pour l'indexation des racines
│   ├── HashTable.h          ← Table de hachage pour les schèmes
│   ├── MorphologyEngine.h   ← Moteur central (orchestration)
│   └── HttpServer.h         ← Serveur HTTP minimal + API REST JSON
├── src/
│   ├── ArabicUtils.cpp      ← Codec UTF-8, applyPattern, extractRoot
│   ├── AVLTree.cpp          ← Rotations AVL, CRUD, traversée
│   ├── HashTable.cpp        ← Hachage polynomial, sondage quadratique
│   ├── MorphologyEngine.cpp ← Génération, validation, famille morphologique
│   ├── HttpServer.cpp       ← Routage REST + UI HTML embarquée
│   └── main.cpp             ← Point d'entrée (HTTP / CLI / Demo)
├── data/
│   ├── roots.txt            ← Corpus de racines arabes trilitères
│   └── patterns.txt         ← Dictionnaire des schèmes morphologiques
├── CMakeLists.txt
└── README.md
```

---

## 🔬 Structures de Données

### 1. Arbre AVL (Indexation des Racines)

```
Chaque nœud contient:
  ┌─────────────────────────────────────┐
  │  root:       std::string (UTF-8)    │
  │  derivatives: vector<DerivativeWord>│
  │  left / right: AVLNode*             │
  │  height:     int                    │
  └─────────────────────────────────────┘
```

- **Insertion / Recherche / Suppression** : O(log n)
- **Comparaison** : par valeur des codepoints Unicode (U+0600–U+06FF)
- **Rééquilibrage** : rotations droite/gauche, cas LL/RR/LR/RL
- **Hauteur réelle** : 8 niveaux pour 91 racines ≈ log₂(91) = 6.5

### 2. Table de Hachage (Schèmes Morphologiques)

```
Clé   : template du schème (ex: "فاعِل")
Valeur: Pattern { name, description, category }
```

- **Fonction de hachage** : rolling polynomial hash sur les octets UTF-8
  `h = Σ (byte[i] × 31^i) mod prime`
- **Résolution des collisions** : sondage quadratique `h(k,i) = (h(k) + i²) mod cap`
- **Facteur de charge** : rehashing automatique quand λ > 0.70
- **Complexité** : O(1) moyen, O(n) pire cas (degenerate clustering)

---

## ⚙️ Algorithmes Morphologiques

### Génération de Mots (applyPattern)

Le schème morphologique utilise ف, ع, ل comme marqueurs de position radicale.

```
Algorithme ApplyPattern(racine[r₀,r₁,r₂], schème):
  résultat ← []
  POUR chaque codepoint cp dans schème:
    SI cp == ف (U+0641): résultat ← résultat + r₀
    SI cp == ع (U+0639): résultat ← résultat + r₁
    SI cp == ل (U+0644): résultat ← résultat + r₂
    SINON:               résultat ← résultat + cp
  RETOURNER encode_UTF8(résultat)

Exemple: racine=[ك,ت,ب], schème="مَفْعُول"
  م → م, فَ → كَ, ْ → ْ, عُ → تُ, و → و, ل → ب
  Résultat: مَكْتُوب
```

### Validation Morphologique

```
Algorithme ValidateWord(mot, racine):
  POUR chaque schème dans table_de_hachage:
    mot_attendu ← ApplyPattern(racine, schème)
    SI StripDiacritics(mot) == StripDiacritics(mot_attendu):
      RETOURNER (OUI, schème)
  
  // Méthode inverse: extraction de racine
  POUR chaque schème:
    racine_extraite ← ExtractRoot(mot, schème)
    SI racine_extraite == racine: RETOURNER (OUI, schème)
  
  RETOURNER (NON, ∅)
```

### Analyse de Complexité

| Opération                     | Complexité   | Structure           |
|-------------------------------|--------------|---------------------|
| Insertion racine              | O(log n)     | AVL Tree            |
| Recherche racine              | O(log n)     | AVL Tree            |
| Suppression racine            | O(log n)     | AVL Tree            |
| Lookup schème                 | O(1) moyen   | Hash Table          |
| Génération d'un mot           | O(m + 1)     | Hash + UTF-8 scan   |
| Génération famille complète   | O(P × m)     | P = nb schèmes      |
| Validation morphologique      | O(P × m)     | P = nb schèmes      |
| Parcours inorder (tous roots) | O(n)         | AVL inorder         |

*n = nombre de racines, m = longueur du schème, P = nombre de schèmes*

---

## 🚀 Compilation & Exécution

### Prérequis
- GCC ≥ 9 ou Clang ≥ 10 (support C++17)
- CMake ≥ 3.14 (optionnel)

### Avec CMake
```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
./arabic-morpho
```

### Avec g++ directement
```bash
g++ -std=c++17 -O2 -Iinclude \
  src/ArabicUtils.cpp src/AVLTree.cpp src/HashTable.cpp \
  src/MorphologyEngine.cpp src/HttpServer.cpp src/main.cpp \
  -o arabic-morpho
```

### Modes d'exécution
```bash
./arabic-morpho              # Serveur HTTP → http://localhost:8080
./arabic-morpho --port 9000  # Port personnalisé
./arabic-morpho --cli        # Mode ligne de commande interactif
./arabic-morpho --demo       # Démonstration automatique
```

---

## 🌐 Interface Web

Ouvrir `http://localhost:8080` après démarrage du serveur.

### Onglets disponibles
| Onglet | Fonctionnalité |
|--------|----------------|
| ⚡ توليد | Génération d'un mot dérivé à partir d'un schème |
| ✓ تحقق | Validation morphologique (OUI/NON + schème identifié) |
| 🌿 الجذور | Gestion des racines (insertion, recherche, suppression) |
| 📐 الأوزان | Gestion des schèmes (tableau + ajout/suppression) |
| 🔍 العائلة الصرفية | Génération de tous les dérivés d'un jذر |

---

## 🔌 API REST

| Méthode | Endpoint | Corps | Description |
|---------|----------|-------|-------------|
| GET | `/api/roots` | — | Liste de toutes les racines |
| GET | `/api/patterns` | — | Liste de tous les schèmes |
| GET | `/api/stats` | — | Statistiques du moteur |
| POST | `/api/insert-root` | `{"root":"كتب"}` | Insérer une racine |
| POST | `/api/remove-root` | `{"root":"كتب"}` | Supprimer une racine |
| POST | `/api/search-root` | `{"root":"كتب"}` | Rechercher une racine |
| POST | `/api/generate` | `{"root":"كتب","pattern":"فاعِل"}` | Générer un dérivé |
| POST | `/api/generate-all` | `{"root":"كتب"}` | Famille complète |
| POST | `/api/validate` | `{"word":"مكتوب","root":"كتب"}` | Valider un mot |
| POST | `/api/derivatives` | `{"root":"كتب"}` | Dérivés stockés |
| POST | `/api/add-pattern` | `{"name":"…","desc":"…","cat":"…"}` | Ajouter un schème |
| POST | `/api/remove-pattern` | `{"name":"…"}` | Supprimer un schème |

### Exemple de réponse (`/api/generate`)
```json
{
  "success": true,
  "root": "كتب",
  "pattern": "فاعِل",
  "description": "اسم الفاعل",
  "category": "اسم",
  "derivedWord": "كاتِب",
  "error": ""
}
```

---

## 📊 Données intégrées

- **91 racines trilitères** arabes (corpus initial)
- **45 schèmes morphologiques** couvrant :
  - 14 formes verbales (أوزان الأفعال)
  - 7 types de noms (أسماء الفاعل، المفعول، الآلة، المكان…)
  - 9 mṣādīr (مصادر)
  - 5 patterns adjectivaux (صفات)
  - 4 pluriels (جموع)

---

## 🧪 Exemple de session CLI

```
morpho> generate كتب فاعِل
Derived word: كاتِب
  Pattern: فاعِل — اسم الفاعل

morpho> validate مكتوب كتب
✓ YES – نعم – الكلمة مشتقة من الجذر على وزن مَفْعُول
  Pattern: مَفْعُول — اسم المفعول

morpho> family علم
Family of 'علم': 45 derivatives
  [اسم]  عالِم (فاعِل) · مَعْلُوم (مَفْعُول) · مِعْلَم (مِفْعَل) …
  [فعل]  عَلِمَ · يَعْلَمُ · عَلَّمَ · أَعْلَمَ · اسْتَعْلَمَ …
  [مصدر] عِلَامَة · تَعْلِيم · إعْلَام · اسْتِعْلَام …

morpho> search كتب
Found: كتب  (O(log n) lookup in AVL tree)

morpho> stats
Roots in AVL tree : 91
Patterns in table : 45
Hash table load   : 67%
AVL tree height   : 8
```

---

## 📝 Encodage & Manipulation des Caractères Arabes

L'arabe est stocké en **UTF-8** (standard Unicode). Les caractères arabes occupent 2 octets chacun (bloc U+0600–U+06FF). Le système gère :

- **Décodage UTF-8** → vecteur de codepoints uint32_t
- **Réencodage** → std::string UTF-8
- **Suppression des harakat** (voyelles diacritiques : U+064B–U+065F, U+0652)
- **Comparaison lexicographique** par valeur de codepoint (ordre AVL)
- **Identification des marqueurs de radicaux** : ف (U+0641), ع (U+0639), ل (U+0644)

---

## 👥 Équipe & Contexte

Projet réalisé dans le cadre du module **Algorithmique et Structures de Données**  
**Département GLSI — Institut Supérieur d'Informatique (ISI)**  
Année universitaire 2025-2026
