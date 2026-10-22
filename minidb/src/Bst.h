#pragma once
#include <algorithm>
#include <cstddef>
#include <memory>
#include <utility>
#include <vector>

template <typename K, bool Balanced = true> class Bst {
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
        return nodeHeight(root_.get());
    }
private:
    struct Node;
    using NodePtr = std::unique_ptr<Node>;

    struct Node {
        K val;
        NodePtr left;
        NodePtr right;
        std::size_t height = 1;
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
        curr = rebalance(std::move(curr));
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
        curr = rebalance(std::move(curr));
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

    std::size_t nodeHeight(const Node* node) const {
        if (node) {
            return node->height;
        }
        return 0;
    }

    void recomputeHeight(Node* node) {
        node->height = 1 + std::max(nodeHeight(node->left.get()), nodeHeight(node->right.get()));
    }

    int balanceFactor(const Node* node) const {
        return static_cast<int>(nodeHeight(node->left.get())) - static_cast<int>(nodeHeight(node->right.get()));
    }

    NodePtr rebalance(NodePtr node) {
        recomputeHeight(node.get());
        if constexpr (!Balanced) {
            return node;
        }
        int bf = balanceFactor(node.get());

        // TOO left heavy
        if (bf > 1) {
            int bfL = balanceFactor(node->left.get());
            // node.right is right heavy
            if (bfL < 0) {
                node = rotateLeftRight(std::move(node));
            } else {
                node = rotateRight(std::move(node));
            }
        }
        // TOO right heavy
        if (bf < -1) {
            int bfR = balanceFactor(node->right.get());
            if (bfR > 0) {
                node = rotateRightLeft(std::move(node));
            } else {
                node = rotateLeft(std::move(node));
            }
        }
        return node;
    }

    NodePtr rotateLeft(NodePtr node) {
        // `y` is the pivot AKA the new root after rotation
        NodePtr y = std::move(node->right);

        // first move the subtree that is in the spot we are trying to rotate to, out
        // it ends up going into `node`'s right as it is guaranteed to be bigger than `node`
        // since it is currently under `y` but `y` is on `node`'s right
        node->right = std::move(y->left);
        // subtree moved out, spot is free, rotate `y` into the new spot
        y->left = std::move(node);

        // `node` is no longer the root, is a child of `y`
        recomputeHeight(y->left.get());
        // `y` is now the new root
        recomputeHeight(y.get());
        return y;
    }

    NodePtr rotateRight(NodePtr node) {
        NodePtr y = std::move(node->left);

        node->left = std::move(y->right);
        y->right = std::move(node);

        recomputeHeight(y->right.get());
        recomputeHeight(y.get());
        return y;
    }

    NodePtr rotateLeftRight(NodePtr node) {
        //  3 (node)           3
        // 1  (node.left) -> 2 -> 2
        //  2               1    1 3
        node->left = rotateLeft(std::move(node->left));
        return rotateRight(std::move(node));
    }

    NodePtr rotateRightLeft(NodePtr node) {
        node->right = rotateRight(std::move(node->right));
        return rotateLeft(std::move(node));
    }
};

#ifdef INTELLISENSE_CHECK_TEMPLATES
template class Bst<int>;
#endif
