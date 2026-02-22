#include "MorphologyEngine.h"
#include "ArabicUtils.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>

// ─────────────────────────────────────────────────────────────────────────────
//  Constructor – loads built-in data
// ─────────────────────────────────────────────────────────────────────────────

MorphologyEngine::MorphologyEngine() {
    loadDefaultPatterns();
    loadDefaultRoots();
}

// ─────────────────────────────────────────────────────────────────────────────
//  Built-in morphological patterns (schèmes)
//  Format: (template, description, grammatical category)
// ─────────────────────────────────────────────────────────────────────────────

void MorphologyEngine::loadDefaultPatterns() {
    struct PatternData { const char* name; const char* desc; const char* cat; };

    // ---- Verb forms (أفعال) ----
    const PatternData patterns[] = {
        // Form I – Base verb
        { "فَعَلَ",   "الفعل الماضي - الوزن الأول",          "فعل"   },
        { "فَعِلَ",   "الفعل الماضي (كسر العين)",              "فعل"   },
        { "فَعُلَ",   "الفعل الماضي (ضم العين)",               "فعل"   },
        { "يَفْعَلُ", "الفعل المضارع - الوزن الأول",           "فعل"   },
        { "يَفْعِلُ", "الفعل المضارع (كسر العين)",             "فعل"   },
        { "يَفْعُلُ", "الفعل المضارع (ضم العين)",              "فعل"   },

        // Form II – Intensive / causative (فَعَّلَ)
        { "فَعَّلَ",   "الفعل المضعَّف - الباب الثاني",        "فعل"   },
        { "يُفَعِّلُ", "المضارع المضعَّف",                      "فعل"   },

        // Form III – Reciprocal (فَاعَلَ)
        { "فَاعَلَ",   "المفاعلة - الباب الثالث",               "فعل"   },
        { "يُفَاعِلُ", "مضارع المفاعلة",                        "فعل"   },

        // Form IV – Causative (أَفْعَلَ)
        { "أَفْعَلَ",  "الإفعال - الباب الرابع",                "فعل"   },
        { "يُفْعِلُ",  "مضارع الإفعال",                         "فعل"   },

        // Form V – Reflexive of II (تَفَعَّلَ)
        { "تَفَعَّلَ",  "التفعُّل - الباب الخامس",              "فعل"   },

        // Form VI – Reciprocal of III (تَفَاعَلَ)
        { "تَفَاعَلَ",  "التفاعل - الباب السادس",               "فعل"   },

        // Form VII – Inchoative (اِنْفَعَلَ)
        { "انْفَعَلَ",  "الانفعال - الباب السابع",              "فعل"   },

        // Form VIII – Reflexive (اِفْتَعَلَ)
        { "افْتَعَلَ",  "الافتعال - الباب الثامن",              "فعل"   },

        // Form X – Request/Seek (اِسْتَفْعَلَ)
        { "اسْتَفْعَلَ","الاستفعال - الباب العاشر",             "فعل"   },

        // ---- Nominal patterns (أسماء) ----
        // Active participle
        { "فاعِل",    "اسم الفاعل",                             "اسم"   },
        { "فَاعِلَة", "اسم الفاعل المؤنث",                      "اسم"   },

        // Passive participle
        { "مَفْعُول", "اسم المفعول",                             "اسم"   },
        { "مَفْعُولَة","اسم المفعول المؤنث",                     "اسم"   },

        // Verbal noun (masdar) patterns
        { "فَعْل",    "المصدر - الوزن الأساسي",                  "مصدر"  },
        { "فِعَالَة", "مصدر الحرفة والمهنة",                     "مصدر"  },
        { "تَفْعِيل", "مصدر الباب الثاني",                       "مصدر"  },
        { "مَفْعَل",  "اسم المكان أو المصدر الميمي",              "مصدر"  },
        { "مَفْعَلَة","اسم المكان المؤنث",                        "مصدر"  },
        { "إفْعَال",  "مصدر الباب الرابع",                       "مصدر"  },
        { "افْتِعَال","مصدر الباب الثامن",                       "مصدر"  },
        { "انْفِعَال","مصدر الباب السابع",                       "مصدر"  },
        { "اسْتِفْعَال","مصدر الباب العاشر",                     "مصدر"  },

        // Instrument noun
        { "مِفْعَل",  "اسم الآلة",                               "اسم"   },
        { "مِفْعَال", "اسم الآلة - وزن ثانٍ",                    "اسم"   },
        { "مِفْعَلَة","اسم الآلة المؤنث",                        "اسم"   },

        // Diminutive
        { "فُعَيْل",  "التصغير",                                  "اسم"   },

        // Adjective patterns
        { "فَعِيل",   "الصفة المشبهة",                           "صفة"   },
        { "فَعَّال",  "صيغة المبالغة",                           "صفة"   },
        { "فَعُول",   "صيغة المبالغة - الثانية",                 "صفة"   },
        { "أَفْعَل",  "أفعل التفضيل",                            "صفة"   },
        { "فَعْلَان", "الصفة المشبهة - فعلان",                   "صفة"   },
        { "مِفْعَال", "صيغة المبالغة - مفعال",                   "صفة"   },

        // Elative / comparative
        { "فُعَال",   "العدد الجمعي",                             "اسم"   },
        { "فُعَّال",  "جمع فاعل",                                 "اسم"   },
        { "فِعَال",   "جمع فَعِيل",                               "اسم"   },
        { "أَفْعَال", "جمع قلة",                                  "اسم"   },
        { "فُعُول",   "جمع كثرة",                                  "اسم"   },

        // Place noun
        { "مَفْعِل",  "اسم المكان",                               "اسم"   },
    };

    for (const auto& p : patterns) {
        hashTable.insert(p.name, Pattern(p.name, p.desc, p.cat));
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Built-in Arabic root corpus (selection of common trilateral roots)
// ─────────────────────────────────────────────────────────────────────────────

void MorphologyEngine::loadDefaultRoots() {
    // Common trilateral Arabic roots with their semantic field
    const char* roots[] = {
        // Writing, science, knowledge
        "كتب", "علم", "درس", "قرأ", "فهم", "عرف", "بحث", "فكر",
        // Communication
        "قول", "كلم", "خبر", "سمع", "نطق", "حدث",
        // Movement
        "ذهب", "جاء", "دخل", "خرج", "سفر", "مشى", "ركب", "طار",
        // Work & production
        "عمل", "صنع", "بنى", "فتح", "حمل", "نقل", "خدم", "تجر",
        // Life & nature
        "حيا", "مات", "نمى", "زرع", "أكل", "شرب", "نوم", "صحو",
        // Emotion & perception
        "حبب", "كره", "خوف", "أمن", "رجا", "غضب", "فرح", "حزن",
        // Religion & morality
        "سلم", "دعا", "صلى", "شكر", "صبر", "عبد", "حمد", "قدس",
        // Time & measurement
        "وقت", "حسب", "عدد", "قدر", "وزن", "كيل",
        // Abstract
        "وجد", "رأى", "ظنن", "شعر", "قصد", "أرد", "نفع", "ضرر",
        // Nature & matter
        "ماء", "نار", "هوا", "أرض", "شجر", "حجر", "بحر",
        // Body & health
        "قلب", "عين", "يدى", "رجل", "رأس", "نفس", "جسم", "صحح",
        // Society
        "ملك", "حكم", "أمر", "نظم", "جمع", "فرق", "قسم", "ولد",
        nullptr
    };

    for (int i = 0; roots[i] != nullptr; i++) {
        tree.insert(roots[i]);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Root Management
// ─────────────────────────────────────────────────────────────────────────────

bool MorphologyEngine::insertRoot(const std::string& root) {
    return tree.insert(root);
}

bool MorphologyEngine::searchRoot(const std::string& root) const {
    return tree.search(root);
}

bool MorphologyEngine::removeRoot(const std::string& root) {
    return tree.remove(root);
}

std::vector<std::string> MorphologyEngine::getAllRoots() const {
    return tree.getAllRoots();
}

void MorphologyEngine::ensureRoot(const std::string& root) {
    tree.insert(root);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Pattern Management
// ─────────────────────────────────────────────────────────────────────────────

bool MorphologyEngine::addPattern(const std::string& name,
                                   const std::string& description,
                                   const std::string& category) {
    return hashTable.insert(name, Pattern(name, description, category));
}

bool MorphologyEngine::updatePattern(const std::string& name,
                                      const std::string& description,
                                      const std::string& category) {
    return hashTable.update(name, Pattern(name, description, category));
}

bool MorphologyEngine::removePattern(const std::string& name) {
    return hashTable.remove(name);
}

std::vector<Pattern> MorphologyEngine::getAllPatterns() const {
    return hashTable.getAllPatterns();
}

// ─────────────────────────────────────────────────────────────────────────────
//  Word Generation
// ─────────────────────────────────────────────────────────────────────────────

GenerationResult MorphologyEngine::generateWord(const std::string& root,
                                                 const std::string& patternName) {
    GenerationResult res;
    res.root    = root;
    res.pattern = patternName;
    res.success = false;

    // 1. Look up pattern in O(1)
    const Pattern* pat = hashTable.get(patternName);
    if (!pat) {
        res.errorMessage = "Pattern not found: " + patternName;
        return res;
    }
    res.patternDescription = pat->description;
    res.patternCategory    = pat->category;

    // 2. Decode root to codepoints
    auto rootCps = ArabicUtils::extractConsonants(root);
    if (rootCps.size() < 3) {
        res.errorMessage = "Root must have at least 3 consonants";
        return res;
    }

    // 3. Apply pattern template (ف→r0, ع→r1, ل→r2)
    std::string derived = ArabicUtils::applyPattern(rootCps, patternName);
    if (derived.empty()) {
        res.errorMessage = "Generation failed";
        return res;
    }

    res.derivedWord = derived;
    res.success     = true;

    // 4. Ensure root is in tree and store the derivative
    ensureRoot(root);
    tree.addDerivative(root, derived, patternName);

    return res;
}

std::vector<GenerationResult> MorphologyEngine::generateAllDerivatives(
    const std::string& root) {

    std::vector<GenerationResult> results;

    auto rootCps = ArabicUtils::extractConsonants(root);
    if (rootCps.size() < 3) return results;

    ensureRoot(root);

    hashTable.forEach([&](const Pattern& pat) {
        std::string derived = ArabicUtils::applyPattern(rootCps, pat.name);
        if (!derived.empty()) {
            GenerationResult res;
            res.root               = root;
            res.pattern            = pat.name;
            res.patternDescription = pat.description;
            res.patternCategory    = pat.category;
            res.derivedWord        = derived;
            res.success            = true;
            results.push_back(res);

            tree.addDerivative(root, derived, pat.name);
        }
    });

    // Sort by category then pattern name for consistent presentation
    std::sort(results.begin(), results.end(),
              [](const GenerationResult& a, const GenerationResult& b) {
                  if (a.patternCategory != b.patternCategory)
                      return a.patternCategory < b.patternCategory;
                  return a.pattern < b.pattern;
              });

    return results;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Word Validation
// ─────────────────────────────────────────────────────────────────────────────

ValidationResult MorphologyEngine::validateWord(const std::string& word,
                                                 const std::string& root) {
    ValidationResult res;
    res.isValid = false;

    auto rootCps = ArabicUtils::extractConsonants(root);
    if (rootCps.size() < 3) {
        res.message = "Root must have at least 3 consonants";
        return res;
    }

    // Search all patterns: check if the word matches generated word
    std::vector<Pattern> allPats = hashTable.getAllPatterns();
    for (const auto& pat : allPats) {
        if (ArabicUtils::matchesPattern(word, rootCps, pat.name)) {
            res.isValid            = true;
            res.matchedPattern     = pat.name;
            res.patternDescription = pat.description;
            res.extractedRoot      = root;
            res.message            = "نعم – الكلمة مشتقة من الجذر على وزن " + pat.name;

            // Store as validated derivative
            ensureRoot(root);
            tree.addDerivative(root, ArabicUtils::stripDiacritics(word), pat.name);
            return res;
        }
    }

    // Try reverse: extract root from word and compare to given root
    for (const auto& pat : allPats) {
        auto extracted = ArabicUtils::extractRoot(word, pat.name);
        if (extracted.size() == 3) {
            std::string extractedStr = ArabicUtils::fromCodepoints(extracted);
            std::string rootStripped = ArabicUtils::fromCodepoints(rootCps);
            if (extractedStr == rootStripped ||
                ArabicUtils::compare(extractedStr, rootStripped) == 0) {
                res.isValid            = true;
                res.matchedPattern     = pat.name;
                res.patternDescription = pat.description;
                res.extractedRoot      = extractedStr;
                res.message            = "نعم – الكلمة مشتقة من الجذر على وزن " + pat.name;
                ensureRoot(root);
                tree.addDerivative(root, ArabicUtils::stripDiacritics(word), pat.name);
                return res;
            }
        }
    }

    res.message = "لا – الكلمة لا تنتمي إلى هذا الجذر";
    return res;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Derivatives
// ─────────────────────────────────────────────────────────────────────────────

std::vector<DerivativeWord> MorphologyEngine::getDerivatives(
    const std::string& root) const {
    return tree.getDerivatives(root);
}

// ─────────────────────────────────────────────────────────────────────────────
//  File I/O
// ─────────────────────────────────────────────────────────────────────────────

void MorphologyEngine::loadRootsFromFile(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Cannot open roots file: " << filename << "\n";
        return;
    }
    std::string line;
    while (std::getline(file, line)) {
        // Trim whitespace
        while (!line.empty() && (line.back() == '\r' || line.back() == '\n' ||
                                  line.back() == ' '))
            line.pop_back();
        if (!line.empty() && line[0] != '#') {
            tree.insert(line);
        }
    }
}

void MorphologyEngine::loadPatternsFromFile(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Cannot open patterns file: " << filename << "\n";
        return;
    }
    std::string line;
    while (std::getline(file, line)) {
        while (!line.empty() && (line.back() == '\r' || line.back() == '\n' ||
                                  line.back() == ' '))
            line.pop_back();
        if (line.empty() || line[0] == '#') continue;

        // Parse CSV: name,description,category
        std::istringstream ss(line);
        std::string name, desc, cat;
        if (std::getline(ss, name, ',') &&
            std::getline(ss, desc, ',') &&
            std::getline(ss, cat)) {
            hashTable.insert(name, Pattern(name, desc, cat));
        }
    }
}

void MorphologyEngine::saveRootsToFile(const std::string& filename) const {
    std::ofstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Cannot write roots file: " << filename << "\n";
        return;
    }
    auto roots = tree.getAllRoots();
    for (const auto& r : roots) {
        file << r << "\n";
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Stats
// ─────────────────────────────────────────────────────────────────────────────

int MorphologyEngine::rootCount()    const { return tree.size(); }
int MorphologyEngine::patternCount() const { return hashTable.size(); }

void MorphologyEngine::printStats() const {
    std::cout << "=== Arabic Morphological Engine ===\n";
    std::cout << "Roots in AVL tree : " << rootCount()    << "\n";
    std::cout << "Patterns in table : " << patternCount() << "\n";
    std::cout << "Hash table load   : "
              << static_cast<int>(hashTable.loadFactor() * 100) << "%\n";
    std::cout << "AVL tree height   : " << tree.height() << "\n";
}
