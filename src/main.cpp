/**
 * Arabic Morphological Search Engine
 * Main entry point
 *
 * Usage:
 *   ./arabic-morpho              → Start HTTP server on port 8080
 *   ./arabic-morpho --cli        → Interactive CLI mode
 *   ./arabic-morpho --port 9000  → HTTP server on custom port
 *   ./arabic-morpho --demo       → Run demonstration
 */
#include "MorphologyEngine.h"
#include "HttpServer.h"
#include "ArabicUtils.h"
#include <iostream>
#include <string>
#include <sstream>
#include <cstring>
#include <iomanip>
#include <locale>
#include <algorithm>

// Windows-specific: set console to UTF-8 so Arabic prints correctly
#ifdef _WIN32
  #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
  #endif
  #include <windows.h>
  static void setupConsole() {
      SetConsoleOutputCP(CP_UTF8);
      SetConsoleCP(CP_UTF8);
  }
#else
  static void setupConsole() {}
#endif

// ─────────────────────────────────────────────────────────────────────────────
//  ANSI color helpers
// ─────────────────────────────────────────────────────────────────────────────
#define RESET   "\033[0m"
#define BOLD    "\033[1m"
#define RED     "\033[31m"
#define GREEN   "\033[32m"
#define YELLOW  "\033[33m"
#define BLUE    "\033[34m"
#define MAGENTA "\033[35m"
#define CYAN    "\033[36m"
#define WHITE   "\033[37m"
#define GOLD    "\033[38;5;220m"

static void printHeader() {
    std::cout << "\n" << GOLD << BOLD;
    std::cout << "  ┌──────────────────────────────────────────────────────────────┐\n";
    std::cout << "  │        محرك البحث الصرفي العربي – Arabic Morpho Engine       │\n";
    std::cout << "  │                AVL Tree + Hash Table  ·  C++17               │\n";
    std::cout << "  └──────────────────────────────────────────────────────────────┘\n";
    std::cout << RESET << "\n";
}

// ─────────────────────────────────────────────────────────────────────────────
//  Demo: showcase the engine capabilities
// ─────────────────────────────────────────────────────────────────────────────
static void runDemo(MorphologyEngine& engine) {
    printHeader();
    std::cout << CYAN << BOLD << "[ DEMO ] " << RESET << "Showcasing Arabic Morphological Engine\n\n";

    // ── Stats ────────────────────────────────────────────────────────────────
    engine.printStats();
    std::cout << "\n";

    // ── Root operations ──────────────────────────────────────────────────────
    std::cout << YELLOW << BOLD << "── Root Management (AVL Tree) ──────────────────\n" << RESET;
    std::vector<std::string> testRoots = {"كتب", "علم", "قرأ", "حسب"};

    std::cout << "Inserting roots: ";
    for (const auto& r : testRoots) {
        bool ins = engine.insertRoot(r);
        std::cout << r << (ins ? "(new) " : "(dup) ");
    }
    std::cout << "\n";

    std::cout << "\nSearching for 'كتب': "
              << (engine.searchRoot("كتب") ? GREEN "FOUND" : RED "NOT FOUND")
              << RESET << "\n";
    std::cout << "Searching for 'xyz': "
              << (engine.searchRoot("xyz") ? GREEN "FOUND" : RED "NOT FOUND")
              << RESET << "\n";

    auto roots = engine.getAllRoots();
    std::cout << "\nFirst 10 roots (sorted by AVL inorder):\n  ";
    int shown = 0;
    for (const auto& r : roots) {
        if (shown >= 10) break;
        std::cout << GOLD << r << RESET << "  ";
        shown++;
    }
    std::cout << "\n\n";

    // ── Word generation ──────────────────────────────────────────────────────
    std::cout << YELLOW << BOLD << "── Word Generation ─────────────────────────────\n" << RESET;

    struct GenTest { const char* root; const char* pattern; };
    GenTest genTests[] = {
        {"كتب", "فاعِل"},    // كاتب
        {"كتب", "مَفْعُول"},  // مكتوب
        {"كتب", "افْتَعَلَ"}, // اكتتب
        {"علم", "فاعِل"},    // عالم
        {"علم", "مَفْعُول"},  // معلوم
        {"درس", "فاعِل"},    // دارس
        {"قرأ", "مَفْعُول"},  // مقروء
        {nullptr, nullptr}
    };

    std::cout << std::left;
    std::cout << BOLD << std::setw(12) << "Root"
              << std::setw(20) << "Pattern"
              << "Derived Word\n" << RESET;
    std::cout << std::string(50, '-') << "\n";

    for (int i = 0; genTests[i].root; i++) {
        auto res = engine.generateWord(genTests[i].root, genTests[i].pattern);
        std::cout << std::setw(12) << genTests[i].root
                  << std::setw(20) << genTests[i].pattern;
        if (res.success) {
            std::cout << GREEN << res.derivedWord << RESET;
        } else {
            std::cout << RED << "FAIL: " << res.errorMessage << RESET;
        }
        std::cout << "\n";
    }
    std::cout << "\n";

    // ── Validation ───────────────────────────────────────────────────────────
    std::cout << YELLOW << BOLD << "── Morphological Validation ────────────────────\n" << RESET;

    struct ValTest { const char* word; const char* root; bool expected; };
    ValTest valTests[] = {
        {"مكتوب",  "كتب", true},
        {"كاتب",   "كتب", true},
        {"عالم",   "علم", true},
        {"معلوم",  "علم", true},
        {"مكتوب",  "علم", false}, // wrong root
        {"كاتب",   "درس", false}, // wrong root
        {nullptr, nullptr, false}
    };

    for (int i = 0; valTests[i].word; i++) {
        auto res = engine.validateWord(valTests[i].word, valTests[i].root);
        bool correct = (res.isValid == valTests[i].expected);
        std::cout << "\"" << valTests[i].word << "\" from root \""
                  << valTests[i].root << "\": ";
        if (res.isValid) {
            std::cout << GREEN << "YES [" << res.matchedPattern << "]" << RESET;
        } else {
            std::cout << RED << "NO" << RESET;
        }
        std::cout << (correct ? "  ✓" : "  ✗ (unexpected)") << "\n";
    }
    std::cout << "\n";

    // ── Full family ──────────────────────────────────────────────────────────
    std::cout << YELLOW << BOLD << "── Family Generation for جذر 'كتب' ────────────\n" << RESET;
    auto family = engine.generateAllDerivatives("كتب");
    std::cout << "Generated " << family.size() << " derivatives:\n\n";

    std::string curCat;
    for (const auto& r : family) {
        if (r.patternCategory != curCat) {
            curCat = r.patternCategory;
            std::cout << CYAN << BOLD << "  [" << curCat << "]\n" << RESET;
        }
        std::cout << "    " << GOLD << std::setw(18) << r.derivedWord << RESET
                  << "  " << std::setw(18) << r.pattern
                  << "  " << r.patternDescription << "\n";
    }
    std::cout << "\n";

    // ── Hash table stats ─────────────────────────────────────────────────────
    std::cout << YELLOW << BOLD << "── Data Structures State ───────────────────────\n" << RESET;
    engine.printStats();
    std::cout << "\n";

    std::cout << GREEN << BOLD << "Demo complete. ✓\n" << RESET;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Interactive CLI
// ─────────────────────────────────────────────────────────────────────────────
static void runCLI(MorphologyEngine& engine) {
    // Set UTF-8 locale
    setlocale(LC_ALL, "en_US.UTF-8");

    printHeader();
    engine.printStats();
    std::cout << "\n";

    std::cout << CYAN << BOLD << "Commands:\n" << RESET;
    std::cout << "  " << YELLOW << "insert <root>" << RESET << "              — إدراج جذر في شجرة AVL\n";
    std::cout << "  " << YELLOW << "search <root>" << RESET << "              — البحث عن جذر\n";
    std::cout << "  " << YELLOW << "remove <root>" << RESET << "              — حذف جذر\n";
    std::cout << "  " << YELLOW << "roots" << RESET << "                      — عرض جميع الجذور\n";
    std::cout << "  " << YELLOW << "generate <root> <pattern>" << RESET << "  — توليد كلمة مشتقة\n";
    std::cout << "  " << YELLOW << "family <root>" << RESET << "              — العائلة الصرفية الكاملة\n";
    std::cout << "  " << YELLOW << "validate <word> <root>" << RESET << "     — التحقق الصرفي\n";
    std::cout << "  " << YELLOW << "patterns" << RESET << "                   — عرض الأوزان\n";
    std::cout << "  " << YELLOW << "derivs <root>" << RESET << "              — عرض المشتقات المخزنة\n";
    std::cout << "  " << YELLOW << "stats" << RESET << "                      — إحصاءات\n";
    std::cout << "  " << YELLOW << "demo" << RESET << "                       — عرض تجريبي\n";
    std::cout << "  " << YELLOW << "help" << RESET << "                       — المساعدة\n";
    std::cout << "  " << YELLOW << "quit" << RESET << "                       — خروج\n\n";

    std::string line;
    while (true) {
        std::cout << GOLD << BOLD << "morpho> " << RESET;
        if (!std::getline(std::cin, line)) break;

        // trim
        while (!line.empty() && (line.back() == ' ' || line.back() == '\r'))
            line.pop_back();
        if (line.empty()) continue;

        std::istringstream ss(line);
        std::string cmd; ss >> cmd;

        if (cmd == "quit" || cmd == "exit" || cmd == "q") {
            std::cout << "مع السلامة!\n";
            break;
        } else if (cmd == "demo") {
            runDemo(engine);
        } else if (cmd == "stats") {
            engine.printStats();
        } else if (cmd == "roots") {
            auto roots = engine.getAllRoots();
            std::cout << "Total roots: " << roots.size() << "\n";
            for (size_t i = 0; i < roots.size(); i++) {
                std::cout << GOLD << roots[i] << RESET << "  ";
                if ((i + 1) % 10 == 0) std::cout << "\n";
            }
            std::cout << "\n";
        } else if (cmd == "patterns") {
            auto pats = engine.getAllPatterns();
            std::cout << "Total patterns: " << pats.size() << "\n";
            std::cout << BOLD << std::setw(20) << "Pattern"
                      << std::setw(12) << "Category"
                      << "Description\n" << RESET;
            std::cout << std::string(60, '-') << "\n";
            for (const auto& p : pats) {
                std::cout << std::setw(20) << p.name
                          << std::setw(12) << p.category
                          << p.description << "\n";
            }
        } else if (cmd == "insert") {
            std::string root; ss >> root;
            if (root.empty()) { std::cout << RED << "Usage: insert <root>\n" << RESET; continue; }
            bool ok = engine.insertRoot(root);
            std::cout << (ok ? GREEN "Inserted: " : YELLOW "Already exists: ")
                      << root << RESET "\n";
        } else if (cmd == "search") {
            std::string root; ss >> root;
            if (root.empty()) { std::cout << RED << "Usage: search <root>\n" << RESET; continue; }
            bool found = engine.searchRoot(root);
            std::cout << (found ? GREEN "Found" : RED "Not found")
                      << ": " << root << RESET "\n";
        } else if (cmd == "remove") {
            std::string root; ss >> root;
            if (root.empty()) { std::cout << RED << "Usage: remove <root>\n" << RESET; continue; }
            bool ok = engine.removeRoot(root);
            std::cout << (ok ? GREEN "Removed" : RED "Not found")
                      << ": " << root << RESET "\n";
        } else if (cmd == "generate") {
            std::string root, pattern;
            ss >> root;
            std::getline(ss, pattern);
            while (!pattern.empty() && pattern[0] == ' ') pattern.erase(0, 1);
            if (root.empty() || pattern.empty()) {
                std::cout << RED << "Usage: generate <root> <pattern>\n" << RESET;
                continue;
            }
            auto res = engine.generateWord(root, pattern);
            if (res.success) {
                std::cout << GREEN << "Derived word: " << GOLD << BOLD
                          << res.derivedWord << RESET << "\n";
                std::cout << "  Pattern: " << res.pattern << " — " << res.patternDescription << "\n";
            } else {
                std::cout << RED << "Error: " << res.errorMessage << RESET "\n";
            }
        } else if (cmd == "family") {
            std::string root; ss >> root;
            if (root.empty()) { std::cout << RED << "Usage: family <root>\n" << RESET; continue; }
            auto results = engine.generateAllDerivatives(root);
            std::cout << "Family of '" << root << "': " << results.size() << " derivatives\n";
            std::string curCat;
            for (const auto& r : results) {
                if (r.patternCategory != curCat) {
                    curCat = r.patternCategory;
                    std::cout << "\n" << CYAN << "[" << curCat << "]" << RESET << "\n";
                }
                std::cout << "  " << GOLD << std::setw(18) << r.derivedWord << RESET
                          << "  " << r.pattern << "\n";
            }
            std::cout << "\n";
        } else if (cmd == "validate") {
            std::string word, root;
            ss >> word >> root;
            if (word.empty() || root.empty()) {
                std::cout << RED << "Usage: validate <word> <root>\n" << RESET;
                continue;
            }
            auto res = engine.validateWord(word, root);
            if (res.isValid) {
                std::cout << GREEN << "✓ YES – " << res.message << RESET "\n";
                std::cout << "  Pattern: " << res.matchedPattern
                          << " — " << res.patternDescription << "\n";
            } else {
                std::cout << RED << "✗ NO – " << res.message << RESET "\n";
            }
        } else if (cmd == "derivs") {
            std::string root; ss >> root;
            if (root.empty()) { std::cout << RED << "Usage: derivs <root>\n" << RESET; continue; }
            auto derivs = engine.getDerivatives(root);
            if (derivs.empty()) {
                std::cout << YELLOW << "No stored derivatives for " << root << RESET "\n";
            } else {
                std::cout << "Derivatives of '" << root << "':\n";
                for (const auto& d : derivs) {
                    std::cout << "  " << GOLD << d.word << RESET
                              << "  [" << d.pattern << "]"
                              << "  freq=" << d.frequency << "\n";
                }
            }
        } else if (cmd == "help") {
            std::cout << "Type 'demo' to see a full demonstration.\n";
        } else {
            std::cout << RED << "Unknown command: " << cmd << RESET "\n";
        }
        std::cout << "\n";
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Main
// ─────────────────────────────────────────────────────────────────────────────
int main(int argc, char* argv[]) {
    // Set up console for UTF-8 output (important on Windows)
    setupConsole();
    // Set locale for proper UTF-8 handling
    setlocale(LC_ALL, "en_US.UTF-8");

    bool cliMode  = false;
    bool demoMode = false;
    int  port     = 8080;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--cli")  == 0) cliMode  = true;
        if (strcmp(argv[i], "--demo") == 0) demoMode = true;
        if (strcmp(argv[i], "--port") == 0 && i + 1 < argc) {
            port = atoi(argv[i + 1]);
            i++;
        }
    }

    // Initialize engine (loads built-in roots & patterns)
    MorphologyEngine engine;

    if (demoMode) {
        runDemo(engine);
        return 0;
    }

    if (cliMode) {
        runCLI(engine);
        return 0;
    }

    // Default: HTTP server mode
    HttpServer server(engine, port);
    server.start();
    return 0;
}
