#pragma once
#include "AVLTree.h"
#include "HashTable.h"
#include "ArabicUtils.h"
#include <string>
#include <vector>
 
// ─────────────────────────────────────────────
//  Result Types
// ─────────────────────────────────────────────
 
struct GenerationResult {
    std::string root;
    std::string pattern;
    std::string patternDescription;
    std::string patternCategory;
    std::string derivedWord;
    bool        success;
    std::string errorMessage;
};
 
struct ValidationResult {
    bool        isValid;
    std::string extractedRoot;    // What root was found in the word
    std::string matchedPattern;   // Which pattern matched
    std::string patternDescription;
    std::string message;
};
 
// ─────────────────────────────────────────────
//  MorphologyEngine
// ─────────────────────────────────────────────
 
/**
 * Arabic Morphological Search Engine
 *
 * Combines an AVL tree (root indexing) with a hash table (pattern lookup)
 * to provide:
 *   1. Root management (insert / search / delete)
 *   2. Pattern management (add / update / remove)
 *   3. Word generation: root + pattern → derived word
 *   4. Word validation: word + root → pattern identification
 *   5. Family exploration: root → all derived words
 *
 * On construction the engine loads built-in Arabic roots and patterns.
 * Additional data can be loaded from text files.
 */
class MorphologyEngine {
public:
    MorphologyEngine();
 
    // ── Root Management ───────────────────────────────────────────────────────
    /** Insert a root into the AVL tree. Returns true if newly added. */
    bool insertRoot(const std::string& root);
 
    /** Search for a root. Returns true if present. */
    bool searchRoot(const std::string& root) const;
 
    /** Remove a root and all its derivatives. Returns true if removed. */
    bool removeRoot(const std::string& root);
 
    /** Get sorted list of all stored roots. */
    std::vector<std::string> getAllRoots() const;
 
    // ── Pattern Management ────────────────────────────────────────────────────
    /** Add a new morphological pattern. Returns false if already exists. */
    bool addPattern(const std::string& name,
                    const std::string& description,
                    const std::string& category);
 
    /** Update an existing pattern. Returns false if not found. */
    bool updatePattern(const std::string& name,
                       const std::string& description,
                       const std::string& category);
 
    /** Remove a pattern. Returns false if not found. */
    bool removePattern(const std::string& name);
 
    /** Get all patterns stored in the hash table. */
    std::vector<Pattern> getAllPatterns() const;
 
    // ── Core Morphological Operations ─────────────────────────────────────────
 
    /**
     * Generate a derived word from a root and a pattern name.
     *
     * Algorithm:
     *   1. Look up pattern template in hash table → O(1)
     *   2. Decode root UTF-8 to codepoints
     *   3. Iterate through pattern codepoints, replacing
     *      ف→root[0], ع→root[1], ل→root[2]
     *   4. Re-encode to UTF-8
     *   5. Store result in AVL node for the root
     *
     * @param root        Arabic root (UTF-8), e.g. "كتب"
     * @param patternName Pattern template name, e.g. "فاعل"
     * @return GenerationResult with the derived word
     */
    GenerationResult generateWord(const std::string& root,
                                   const std::string& patternName);
 
    /**
     * Generate all possible derivatives for a root across all known patterns.
     *
     * @param root Arabic root (UTF-8)
     * @return Vector of GenerationResult (one per pattern)
     */
    std::vector<GenerationResult> generateAllDerivatives(const std::string& root);
 
    /**
     * Validate whether a word morphologically belongs to a given root.
     *
     * Algorithm:
     *   For each known pattern:
     *     1. Apply pattern to root → expected word
     *     2. Compare with input word (after diacritic stripping)
     *   OR: extract consonants from word and see if they match the root
     *       consonants in the positions dictated by each pattern.
     *
     * @param word Candidate Arabic word (UTF-8)
     * @param root Arabic root to test against (UTF-8)
     * @return ValidationResult
     */
    ValidationResult validateWord(const std::string& word,
                                   const std::string& root);
 
    /**
     * Get all stored (validated or generated) derivatives for a root.
     */
    std::vector<DerivativeWord> getDerivatives(const std::string& root) const;
 
    // ── File I/O ──────────────────────────────────────────────────────────────
    /** Load roots from a text file (one root per line, UTF-8). */
    void loadRootsFromFile(const std::string& filename);
 
    /** Load patterns from a CSV file: name,description,category */
    void loadPatternsFromFile(const std::string& filename);
 
    /** Save all roots to a text file. */
    void saveRootsToFile(const std::string& filename) const;
 
    // ── Stats ─────────────────────────────────────────────────────────────────
    int rootCount()    const;
    int patternCount() const;
 
    /** Print a summary of the engine state. */
    void printStats() const;
 
private:
    AVLTree   tree;
    HashTable hashTable;
 
    /** Load the built-in Arabic root corpus. */
    void loadDefaultRoots();
 
    /** Load the built-in morphological pattern dictionary. */
    void loadDefaultPatterns();
 
    /** Ensure root exists in tree; insert if not. */
    void ensureRoot(const std::string& root);
};