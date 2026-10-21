#pragma once
#include <algorithm>
#include <cstddef>
#include <memory>
#include <utility>
#include <vector>

template <typename K> class Bst {
public:
    // `false` if val already exists (nothing was inserted)
    bool insert(K val) {
        std::size_t oldSize = size_;
        root_ = insertHelper(std::move(val), std::move(root_));
        return size_ != oldSize;
    }

    // `false` if val does not exist (nothing was removed)
    bool erase(const K& val) {
        std::size_t oldSize = size_;
        root_ = eraseHelper(val, std::move(root_));
        return size_ != oldSize;
    }

    std::vector<K> range(const K& lo, const K& hi) const {
        visited_ = 0;
        std::vector<K> out;
        rangeHelper(root_.get(), lo, hi, out);
        return out;
    }

    bool contains(const K& val) const {
        const Node* node = root_.get();
        while (node) {
            if (val < node->val) {
                node = node->left.get();
            } else if (node->val < val) {
                node = node->right.get();
            } else {
                return true;
            }
        }
        return false;
    }

    std::size_t size() const {
        return size_;
    }

    std::size_t lastVisited() const {
        return visited_;
    }

    std::size_t height() const {
        return heightHelper(root_.get());
    }
private:
    struct Node;
    using NodePtr = std::unique_ptr<Node>;

    struct Node {
        K val;
        NodePtr left;
        NodePtr right;
    };

    mutable std::size_t visited_ = 0;
    std::size_t size_ = 0;
    NodePtr root_;

    NodePtr insertHelper(K val, NodePtr curr) {
        if (!curr) {
            curr = std::make_unique<Node>(std::move(val));
            size_++;
            return curr;
        }
        if (val < curr->val) {
            curr->left = insertHelper(std::move(val), std::move(curr->left));
        } else if (curr->val < val) {
            curr->right = insertHelper(std::move(val), std::move(curr->right));
        }
        return curr;
    }

    NodePtr eraseHelper(const K& val, NodePtr curr) {
        if (!curr) {
            return curr;
        }
        if (val < curr->val) {
            curr->left = eraseHelper(val, std::move(curr->left));
        } else if (curr->val < val) {
            curr->right = eraseHelper(val, std::move(curr->right));
        } else {
            // `curr` is the node we wish to remove
            // we seek to link `curr`'s parent to whatever is returned (skip `curr`)
            // our "find" algorithm thankfully already does that via move then
            //
            // curr no children => nullptr returned
            // curr only right => right returned
            // curr only left => left returned
            // curr has both => successor returned
            if (!curr->left) {
                size_--;
                return std::move(curr->right);
            } else if (!curr->right) {
                size_--;
                return std::move(curr->left);
            } else {
                // the successor is the smallest node that is greater than the one we seek to remove
                // 1. traverse right once (greater)
                // 2. continuously traverse left (smallest of the greater)
                const Node* suc = curr->right.get();
                while (suc->left) {
                    suc = suc->left.get();
                }
                // copy successor's value over (basically = to replacing curr with successor)
                curr->val = suc->val;
                // erase successor (no left node, so it will fall into the above if-branch)
                curr->right = eraseHelper(curr->val, std::move(curr->right));
            }
        }
        return curr;
    }

    void rangeHelper(const Node* node, const K& lo, const K& hi, std::vector<K>& out) const {
        visited_++;
        if (!node) {
            return;
        }
        if (lo < node->val) {
            rangeHelper(node->left.get(), lo, hi, out);
        }
        // lo <= node <= hi
        if (!(node->val < lo) && !(hi < node->val)) {
            out.push_back(node->val);
        }
        if (node->val < hi) {
            rangeHelper(node->right.get(), lo, hi, out);
        }
    }

    std::size_t heightHelper(const Node* curr) const {
        if (!curr) {
            return 0;
        }
        return 1 + std::max(heightHelper(curr->left.get()), heightHelper(curr->right.get()));
    }
};

#ifdef INTELLISENSE_CHECK_TEMPLATES
template class Bst<int>;
#endif
