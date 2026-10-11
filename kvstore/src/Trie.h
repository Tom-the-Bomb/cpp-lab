#pragma once
#include <cstddef>
#include <map>
#include <memory>
#include <string>
#include <vector>

class Trie {
    struct Node {
        using Children = std::map<char, std::unique_ptr<Node>>;
        Children children;
        bool isEnd = false;
    };
    Node root_;

    bool eraseRec(Node* node, const std::string& key, std::size_t i) {
        if (i == key.size()) {
            if (!node->isEnd) {
                return false;
            }
            node->isEnd = false;
        } else {
            Node::Children::iterator it = node->children.find(key[i]);
            if (it == node->children.end()) {
                return false;
            }
            if (eraseRec(it->second.get(), key, i + 1)) {
                node->children.erase(it);
            }
        }
        return !node->isEnd && node->children.empty();
    }

    void collectRec(
        const Node* node, std::string curr,
        std::vector<std::string>& out
    ) const {
        if (node->isEnd) {
            out.push_back(curr);
        }
        for (auto& child : node->children) {
            collectRec(child.second.get(), curr + child.first, out);
        }
    }
public:
    void erase(const std::string& key) {
        eraseRec(&root_, key, 0);
    }

    bool contains(const std::string& key) const {
        const Node* curr = &root_;
        for (char c : key) {
            Node::Children::const_iterator it = curr->children.find(c);
            if (it == curr->children.end()) {
                return false;
            }
            curr = it->second.get();
        }
        return curr->isEnd;
    }

    void insert(const std::string& key) {
        Node* curr = &root_;
        for (char c : key) {
            std::unique_ptr<Node>& child = curr->children[c];
            if (!child) {
                child = std::make_unique<Node>();
            }
            curr = child.get();
        }
        curr->isEnd = true;
    }

    std::vector<std::string> collect(const std::string& prefix) const {
        std::vector<std::string> out;
        const Node* curr = &root_;
        for (char c : prefix) {
            Node::Children::const_iterator it = curr->children.find(c);
            if (it == curr->children.end()) {
                return out;
            }
            curr = it->second.get();
        }
        collectRec(curr, prefix, out);
        return out;
    }
};