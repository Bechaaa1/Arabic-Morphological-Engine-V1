#include "HashTable.h"
#include <iostream>
#include <stdexcept>
#include <cstring>
#include <cstdint>

// ─────────────────────────────────────────────────────────────────────────────
//  Constructor
// ─────────────────────────────────────────────────────────────────────────────

HashTable::HashTable(int initialCapacity)
    : table(initialCapacity), cap(initialCapacity), count(0) {}

// ─────────────────────────────────────────────────────────────────────────────
//  Hash function: polynomial rolling hash over UTF-8 bytes
//  Uses prime base 31 and Horner's method
// ─────────────────────────────────────────────────────────────────────────────

int HashTable::hashFn(const std::string& key) const {
    const uint32_t BASE  = 31u;
    const uint32_t PRIME = 1000000007u;
    uint64_t hash = 0;
    for (unsigned char c : key) {
        hash = (hash * BASE + c) % PRIME;
    }
    return static_cast<int>(hash % static_cast<uint64_t>(cap));
}

// ─────────────────────────────────────────────────────────────────────────────
//  Quadratic probing: h(k,i) = (h(k) + i^2) % cap
// ─────────────────────────────────────────────────────────────────────────────

int HashTable::probe(int h, int i) const {
    return (h + i * i) % cap;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Find slot of an existing key (-1 if not found)
// ─────────────────────────────────────────────────────────────────────────────

int HashTable::findSlot(const std::string& key) const {
    int h = hashFn(key);
    for (int i = 0; i < cap; i++) {
        int idx = probe(h, i);
        const HashEntry& e = table[idx];
        if (!e.occupied && !e.deleted) break;   // empty slot → not present
        if (e.occupied && e.key == key) return idx;
    }
    return -1;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Find slot for insertion (first empty or tombstone)
// ─────────────────────────────────────────────────────────────────────────────

int HashTable::findInsertSlot(const std::string& key) const {
    int h = hashFn(key);
    int tombstone = -1;
    for (int i = 0; i < cap; i++) {
        int idx = probe(h, i);
        const HashEntry& e = table[idx];
        if (!e.occupied) {
            if (e.deleted && tombstone == -1) tombstone = idx;
            if (!e.deleted) return (tombstone != -1) ? tombstone : idx;
        }
        if (e.occupied && e.key == key) return idx; // already exists
    }
    return tombstone;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Next prime >= n (for rehash sizing)
// ─────────────────────────────────────────────────────────────────────────────

int HashTable::nextPrime(int n) {
    auto isPrime = [](int x) {
        if (x < 2) return false;
        if (x == 2) return true;
        if (x % 2 == 0) return false;
        for (int i = 3; i * i <= x; i += 2)
            if (x % i == 0) return false;
        return true;
    };
    while (!isPrime(n)) n++;
    return n;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Rehash
// ─────────────────────────────────────────────────────────────────────────────

void HashTable::rehash() {
    int newCap = nextPrime(cap * 2 + 1);
    std::vector<HashEntry> oldTable = std::move(table);
    table.assign(newCap, HashEntry());
    cap   = newCap;
    count = 0;

    for (const auto& e : oldTable) {
        if (e.occupied) {
            insert(e.key, e.value);
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  CRUD operations
// ─────────────────────────────────────────────────────────────────────────────

bool HashTable::insert(const std::string& key, const Pattern& pattern) {
    if (loadFactor() >= 0.70) rehash();

    int idx = findInsertSlot(key);
    if (idx == -1) return false;

    HashEntry& e = table[idx];
    if (e.occupied && e.key == key) return false; // duplicate

    e.key      = key;
    e.value    = pattern;
    e.occupied = true;
    e.deleted  = false;
    count++;
    return true;
}

bool HashTable::update(const std::string& key, const Pattern& pattern) {
    int idx = findSlot(key);
    if (idx == -1) return false;
    table[idx].value = pattern;
    return true;
}

bool HashTable::remove(const std::string& key) {
    int idx = findSlot(key);
    if (idx == -1) return false;
    table[idx].occupied = false;
    table[idx].deleted  = true;
    count--;
    return true;
}

Pattern* HashTable::get(const std::string& key) {
    int idx = findSlot(key);
    if (idx == -1) return nullptr;
    return &table[idx].value;
}

const Pattern* HashTable::get(const std::string& key) const {
    int idx = const_cast<HashTable*>(this)->findSlot(key);
    if (idx == -1) return nullptr;
    return &table[idx].value;
}

bool HashTable::contains(const std::string& key) const {
    return findSlot(key) != -1;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Iteration
// ─────────────────────────────────────────────────────────────────────────────

std::vector<Pattern> HashTable::getAllPatterns() const {
    std::vector<Pattern> result;
    result.reserve(count);
    for (const auto& e : table) {
        if (e.occupied) result.push_back(e.value);
    }
    return result;
}

void HashTable::forEach(std::function<void(const Pattern&)> cb) const {
    for (const auto& e : table) {
        if (e.occupied) cb(e.value);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Debug
// ─────────────────────────────────────────────────────────────────────────────

void HashTable::printStats() const {
    std::cout << "HashTable: capacity=" << cap
              << ", count=" << count
              << ", loadFactor=" << loadFactor() << "\n";
    for (int i = 0; i < cap; i++) {
        const auto& e = table[i];
        if (e.occupied) {
            std::cout << "  [" << i << "] " << e.key
                      << " → " << e.value.description << "\n";
        }
    }
}
