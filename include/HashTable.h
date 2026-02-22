#pragma once
#include <string>
#include <vector>
#include <functional>

// ─────────────────────────────────────────────
//  Pattern Data Type
// ─────────────────────────────────────────────

/**
 * Morphological pattern (schème morphologique).
 *
 * A pattern is a template string using ف, ع, ل as radical placeholders.
 * Example: "فاعل"  → agent noun (اسم الفاعل)
 *          "مفعول" → passive participle (اسم المفعول)
 *          "افتعل" → Form VIII verb
 */
struct Pattern {
    std::string name;        // Pattern name / template (e.g. "فاعل")
    std::string description; // Human-readable description
    std::string category;    // Grammatical category: "اسم" | "فعل" | "صفة" | "مصدر"

    Pattern() = default;
    Pattern(const std::string& n, const std::string& d, const std::string& c)
        : name(n), description(d), category(c) {}
};

// ─────────────────────────────────────────────
//  Hash Table Entry
// ─────────────────────────────────────────────

struct HashEntry {
    std::string key;
    Pattern     value;
    bool        occupied;
    bool        deleted;   // tombstone for open addressing

    HashEntry() : occupied(false), deleted(false) {}
};

// ─────────────────────────────────────────────
//  HashTable Class
// ─────────────────────────────────────────────

/**
 * Custom open-addressing hash table with quadratic probing.
 *
 * Stores morphological patterns keyed by their template string.
 * Automatically rehashes when load factor > 0.70.
 *
 * Complexity:
 *   - Average insert / lookup / delete: O(1)
 *   - Worst case: O(n) (degenerate clustering)
 *
 * Hash function: polynomial rolling hash over UTF-8 bytes, mod prime capacity.
 */
class HashTable {
public:
    explicit HashTable(int initialCapacity = 67); // prime
    ~HashTable() = default;

    // ── CRUD ─────────────────────────────────────
    /** Insert a new pattern. Returns false if key already exists. */
    bool insert(const std::string& key, const Pattern& pattern);

    /** Update an existing pattern. Returns false if key not found. */
    bool update(const std::string& key, const Pattern& pattern);

    /** Remove a pattern. Returns false if not found. */
    bool remove(const std::string& key);

    /** Retrieve a pattern pointer, or nullptr if not found. */
    Pattern*       get(const std::string& key);
    const Pattern* get(const std::string& key) const;

    /** Returns true if the key exists in the table. */
    bool contains(const std::string& key) const;

    // ── Iteration ────────────────────────────────
    /** Return all stored patterns. */
    std::vector<Pattern> getAllPatterns() const;

    /** Apply callback to every pattern. */
    void forEach(std::function<void(const Pattern&)> cb) const;

    // ── Stats ────────────────────────────────────
    int    size()       const { return count; }
    int    capacity()   const { return cap; }
    double loadFactor() const { return static_cast<double>(count) / cap; }

    /** Print table statistics (debug). */
    void printStats() const;

private:
    std::vector<HashEntry> table;
    int cap;
    int count;

    /** Polynomial rolling hash over UTF-8 bytes. */
    int hashFn(const std::string& key) const;

    /** Quadratic probe: h(k, i) = (h(k) + i*i) % cap */
    int probe(int h, int i) const;

    /** Find slot index for key; returns -1 if not found. */
    int findSlot(const std::string& key) const;

    /** Find insertion slot. */
    int findInsertSlot(const std::string& key) const;

    /** Double capacity and rehash all entries. */
    void rehash();

    /** Next prime >= n. Used for capacity management. */
    static int nextPrime(int n);
};
