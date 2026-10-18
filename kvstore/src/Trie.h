#pragma once
#include <cstddef>
#include <map>
#include <memory>
#include <string>
#include <vector>

class Trie {
public:
    void insert(const std::string& key) {
        Node* curr = &root_;
        for (char c : key) {
            auto& child = curr->children[c]; // creates an empty slot if `c` is missing
            if (!child) {
                child = std::make_unique<Node>();
            }
            curr = child.get();
        }
        curr->isEnd = true;
    }

    void erase(const std::string& key) {
        eraseRec(&root_, key, 0);
    }

    void clear() {
        // children's values are unique ptrs that will free themselves
        root_.children.clear();
        root_.isEnd = false;
        // root_ is "" which remains valid (no need to delete)
    }

    bool contains(const std::string& key) const {
        const Node* node = findNode(key);
        return node && node->isEnd;
    }

    // every key starting with `prefix`, in alphabetical order
    std::vector<std::string> collect(const std::string& prefix) const {
        std::vector<std::string> out;
        if (const Node* node = findNode(prefix)) {
            collectRec(node, prefix, out);
        }
        return out;
    }

private:
    struct Node {
        std::map<char, std::unique_ptr<Node>> children;
        bool isEnd = false;
    };

    Node root_;

    // the node reached by following `path` from the root, or nullptr if the path breaks
    const Node* findNode(const std::string& path) const {
        const Node* curr = &root_;
        for (char c : path) {
            auto it = curr->children.find(c);
            if (it == curr->children.end()) {
                return nullptr;
            }
            curr = it->second.get();
        }
        return curr;
    }

    // returns true if the caller should remove `node`
    bool eraseRec(Node* node, const std::string& key, std::size_t i) {
        if (i == key.size()) {
            if (!node->isEnd) {
                return false;
            }
            node->isEnd = false;
        } else {
            auto it = node->children.find(key[i]);
            if (it == node->children.end()) {
                return false;
            }
            if (eraseRec(it->second.get(), key, i + 1)) {
                node->children.erase(it);
            }
        }
        return !node->isEnd && node->children.empty();
    }

    // pre-order DFS, so `out` is in alphabetical order
    void collectRec(const Node* node, std::string path, std::vector<std::string>& out) const {
        if (node->isEnd) {
            out.push_back(path);
        }
        for (const auto& [ch, child] : node->children) {
            collectRec(child.get(), path + ch, out);
        }
    }
};
