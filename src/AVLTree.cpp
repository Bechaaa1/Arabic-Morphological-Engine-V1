#include "AVLTree.h"
#include "ArabicUtils.h"
#include <iostream>
#include <algorithm>

// ─────────────────────────────────────────────────────────────────────────────
//  Constructor / Destructor
// ─────────────────────────────────────────────────────────────────────────────

AVLTree::AVLTree() : root_(nullptr), nodeCount(0) {}

AVLTree::~AVLTree() {
    destroyTree(root_);
}

void AVLTree::destroyTree(AVLNode* node) {
    if (!node) return;
    destroyTree(node->left);
    destroyTree(node->right);
    delete node;
}

// ─────────────────────────────────────────────────────────────────────────────
//  AVL bookkeeping
// ─────────────────────────────────────────────────────────────────────────────

int AVLTree::nodeHeight(AVLNode* n) const {
    return n ? n->height : 0;
}

int AVLTree::balanceFactor(AVLNode* n) const {
    return n ? nodeHeight(n->left) - nodeHeight(n->right) : 0;
}

void AVLTree::updateHeight(AVLNode* n) {
    if (n)
        n->height = 1 + std::max(nodeHeight(n->left), nodeHeight(n->right));
}

int AVLTree::height() const {
    return nodeHeight(root_);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Rotations
// ─────────────────────────────────────────────────────────────────────────────

AVLNode* AVLTree::rotateRight(AVLNode* y) {
    AVLNode* x  = y->left;
    AVLNode* T2 = x->right;

    x->right = y;
    y->left  = T2;

    updateHeight(y);
    updateHeight(x);
    return x;
}

AVLNode* AVLTree::rotateLeft(AVLNode* x) {
    AVLNode* y  = x->right;
    AVLNode* T2 = y->left;

    y->left  = x;
    x->right = T2;

    updateHeight(x);
    updateHeight(y);
    return y;
}

AVLNode* AVLTree::rebalance(AVLNode* n) {
    updateHeight(n);
    int bf = balanceFactor(n);

    // Left-heavy
    if (bf > 1) {
        if (balanceFactor(n->left) < 0)       // Left-Right
            n->left = rotateLeft(n->left);
        return rotateRight(n);
    }
    // Right-heavy
    if (bf < -1) {
        if (balanceFactor(n->right) > 0)       // Right-Left
            n->right = rotateRight(n->right);
        return rotateLeft(n);
    }
    return n; // already balanced
}

// ─────────────────────────────────────────────────────────────────────────────
//  Insert
// ─────────────────────────────────────────────────────────────────────────────

AVLNode* AVLTree::insertNode(AVLNode* node, const std::string& key, bool& inserted) {
    if (!node) {
        inserted = true;
        nodeCount++;
        return new AVLNode(key);
    }

    int cmp = ArabicUtils::compare(key, node->root);
    if      (cmp < 0) node->left  = insertNode(node->left,  key, inserted);
    else if (cmp > 0) node->right = insertNode(node->right, key, inserted);
    else {
        inserted = false; // duplicate
        return node;
    }

    return rebalance(node);
}

bool AVLTree::insert(const std::string& root) {
    bool inserted = false;
    root_ = insertNode(root_, root, inserted);
    return inserted;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Search
// ─────────────────────────────────────────────────────────────────────────────

AVLNode* AVLTree::findNode(AVLNode* node, const std::string& key) const {
    if (!node) return nullptr;
    int cmp = ArabicUtils::compare(key, node->root);
    if      (cmp < 0) return findNode(node->left,  key);
    else if (cmp > 0) return findNode(node->right, key);
    else              return node;
}

bool AVLTree::search(const std::string& root) const {
    return findNode(root_, root) != nullptr;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Remove
// ─────────────────────────────────────────────────────────────────────────────

AVLNode* AVLTree::minNode(AVLNode* node) const {
    AVLNode* cur = node;
    while (cur->left) cur = cur->left;
    return cur;
}

AVLNode* AVLTree::removeNode(AVLNode* node, const std::string& key, bool& removed) {
    if (!node) return nullptr;

    int cmp = ArabicUtils::compare(key, node->root);
    if (cmp < 0) {
        node->left  = removeNode(node->left,  key, removed);
    } else if (cmp > 0) {
        node->right = removeNode(node->right, key, removed);
    } else {
        removed = true;
        nodeCount--;

        if (!node->left || !node->right) {
            AVLNode* child = node->left ? node->left : node->right;
            delete node;
            return child;
        }
        // Two children: replace with inorder successor
        AVLNode* successor = minNode(node->right);
        node->root        = successor->root;
        node->derivatives = successor->derivatives;
        // Remove successor from right subtree
        nodeCount++;  // compensate: removeNode will decrement again
        node->right = removeNode(node->right, successor->root, removed);
    }

    return rebalance(node);
}

bool AVLTree::remove(const std::string& root) {
    bool removed = false;
    root_ = removeNode(root_, root, removed);
    return removed;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Derivatives
// ─────────────────────────────────────────────────────────────────────────────

bool AVLTree::addDerivative(const std::string& root,
                             const std::string& word,
                             const std::string& pattern) {
    AVLNode* node = findNode(root_, root);
    if (!node) return false;

    // Check for existing entry (same word + pattern)
    for (auto& d : node->derivatives) {
        if (d.word == word && d.pattern == pattern) {
            d.frequency++;
            return true;
        }
    }
    node->derivatives.emplace_back(word, pattern, 1);
    return true;
}

std::vector<DerivativeWord> AVLTree::getDerivatives(const std::string& root) const {
    AVLNode* node = findNode(root_, root);
    if (!node) return {};
    return node->derivatives;
}

void AVLTree::incrementFrequency(const std::string& root, const std::string& word) {
    AVLNode* node = findNode(root_, root);
    if (!node) return;
    for (auto& d : node->derivatives) {
        if (d.word == word) { d.frequency++; return; }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Traversal
// ─────────────────────────────────────────────────────────────────────────────

void AVLTree::inorderTraversal(AVLNode* node,
                                std::function<void(const AVLNode*)> cb) const {
    if (!node) return;
    inorderTraversal(node->left, cb);
    cb(node);
    inorderTraversal(node->right, cb);
}

void AVLTree::inorder(std::function<void(const AVLNode*)> callback) const {
    inorderTraversal(root_, callback);
}

std::vector<std::string> AVLTree::getAllRoots() const {
    std::vector<std::string> result;
    result.reserve(nodeCount);
    inorder([&](const AVLNode* n) { result.push_back(n->root); });
    return result;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Debug print
// ─────────────────────────────────────────────────────────────────────────────

void AVLTree::printNode(AVLNode* node, const std::string& prefix, bool isLeft) const {
    if (!node) return;
    std::cout << prefix;
    std::cout << (isLeft ? "├── " : "└── ");
    std::cout << node->root
              << " [h=" << node->height
              << ", derivs=" << node->derivatives.size() << "]\n";
    printNode(node->left,  prefix + (isLeft ? "│   " : "    "), true);
    printNode(node->right, prefix + (isLeft ? "│   " : "    "), false);
}

void AVLTree::printTree() const {
    if (!root_) { std::cout << "(empty tree)\n"; return; }
    std::cout << root_->root
              << " [h=" << root_->height
              << ", derivs=" << root_->derivatives.size() << "]\n";
    printNode(root_->left,  "", true);
    printNode(root_->right, "", false);
}
