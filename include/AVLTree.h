#pragma once
#include <string>
#include <vector>
#include <functional>

// ─────────────────────────────────────────────
//  Data Types
// ─────────────────────────────────────────────

/**
 * A validated derivative word stored with its root.
 */
struct DerivativeWord {
    std::string word;     // The derived Arabic word (UTF-8)
    std::string pattern;  // Pattern name used (e.g. "فاعل")
    int frequency;        // Usage frequency counter

    DerivativeWord() : frequency(0) {}
    DerivativeWord(const std::string& w, const std::string& p, int f = 1)
        : word(w), pattern(p), frequency(f) {}
};

/**
 * AVL tree node.
 * Each node stores one Arabic root and its associated derivative words.
 */
struct AVLNode {
    std::string root;                    // Arabic root string (UTF-8)
    std::vector<DerivativeWord> derivatives; // Validated derived words
    AVLNode* left;
    AVLNode* right;
    int height;

    explicit AVLNode(const std::string& r)
        : root(r), left(nullptr), right(nullptr), height(1) {}
};

// ─────────────────────────────────────────────
//  AVLTree Class
// ─────────────────────────────────────────────

/**
 * Self-balancing AVL Binary Search Tree for Arabic roots.
 *
 * Ordering: lexicographic comparison of UTF-8 codepoints.
 * Complexity: O(log n) for insert, search, delete.
 */
class AVLTree {
public:
    AVLTree();
    ~AVLTree();

    // ── Root management ──────────────────────────
    /** Insert a new root. Returns true if newly inserted. */
    bool insert(const std::string& root);

    /** Search for a root. Returns true if found. */
    bool search(const std::string& root) const;

    /** Remove a root. Returns true if removed. */
    bool remove(const std::string& root);

    // ── Derivative management ────────────────────
    /** Add a derivative to an existing root node. */
    bool addDerivative(const std::string& root,
                       const std::string& word,
                       const std::string& pattern);

    /** Get all derivatives of a root. */
    std::vector<DerivativeWord> getDerivatives(const std::string& root) const;

    /** Increment frequency counter for a derivative. */
    void incrementFrequency(const std::string& root, const std::string& word);

    // ── Traversal ────────────────────────────────
    /** Collect all roots in sorted order. */
    std::vector<std::string> getAllRoots() const;

    /** Inorder traversal with callback. */
    void inorder(std::function<void(const AVLNode*)> callback) const;

    /** Print the tree structure (debug). */
    void printTree() const;

    // ── Stats ────────────────────────────────────
    int size() const { return nodeCount; }
    int height() const;

private:
    AVLNode* root_;
    int nodeCount;

    // AVL helpers
    int  nodeHeight(AVLNode* n) const;
    int  balanceFactor(AVLNode* n) const;
    void updateHeight(AVLNode* n);

    AVLNode* rotateRight(AVLNode* y);
    AVLNode* rotateLeft(AVLNode* x);
    AVLNode* rebalance(AVLNode* n);

    AVLNode* insertNode(AVLNode* node, const std::string& key, bool& inserted);
    AVLNode* removeNode(AVLNode* node, const std::string& key, bool& removed);
    AVLNode* minNode(AVLNode* node) const;
    AVLNode* findNode(AVLNode* node, const std::string& key) const;

    void inorderTraversal(AVLNode* node,
                          std::function<void(const AVLNode*)> cb) const;
    void printNode(AVLNode* node, const std::string& prefix, bool isLeft) const;
    void destroyTree(AVLNode* node);
};
