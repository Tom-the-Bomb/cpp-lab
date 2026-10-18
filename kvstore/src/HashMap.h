#pragma once
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>

struct FnvHash {
    std::size_t operator()(const std::string& s) const {
        std::uint64_t h = 14695981039346656037ull;
        for (unsigned char c : s) {
            h ^= c;
            h *= 1099511628211ull;
        }
        return static_cast<std::size_t>(h);
    }
};

template <typename K, typename V, typename Hash = std::hash<K>> class HashMap {
    struct Node {
        K key;
        V value;
        Node* next;
    };
    Node** buckets_;
    std::size_t capacity_;
    std::size_t size_;
    Hash hash_;

    std::size_t indexFor(const K& key, std::size_t cap) const {
        // & (k - 1) is the same as % k if k is a power of 2
        return hash_(key) & (cap - 1);
    }

    void rehash(std::size_t newCap) {
        Node** newBuckets = new Node*[newCap]();

        for (std::size_t i = 0; i < capacity_; i++) {
            for (Node* curr = buckets_[i]; curr != nullptr;) {
                Node* next = curr->next;
                std::size_t newI = indexFor(curr->key, newCap);

                curr->next = newBuckets[newI];
                newBuckets[newI] = curr;
                curr = next;
            }
        }
        delete[] buckets_;
        buckets_ = newBuckets;
        capacity_ = newCap;
    }

    template <bool IsConst> class Iter {
        using MapPtr = std::conditional_t<IsConst, const HashMap*, HashMap*>;
        using NodePtr = std::conditional_t<IsConst, const Node*, Node*>;
        using ValRef = std::conditional_t<IsConst, const V&, V&>;

        friend class HashMap;

        Iter(MapPtr m, std::size_t b): map_(m), bucket_(b), node_(nullptr) {}

        // move cursor to the first non-empty bucket after and including `from`
        void seek(std::size_t from) {
            for (bucket_ = from; bucket_ < map_->capacity_; bucket_++) {
                if (map_->buckets_[bucket_]) {
                    node_ = map_->buckets_[bucket_];
                    return;
                }
            }
            // no non-empty buckets found
            node_ = nullptr;
        }

        MapPtr map_;
        std::size_t bucket_;
        NodePtr node_;
    public:
        // deref to get current entry
        std::pair<const K&, ValRef> operator*() const {
            return {node_->key, node_->value};
        }

        // increment to advance one entry
        Iter& operator++() {
            node_ = node_->next;
            if (!node_) {
                seek(bucket_ + 1);
            }
            return *this;
        }

        // iterators are equal if they are on the same entry
        bool operator==(const Iter& other) const {
            return node_ == other.node_;
        }
    };
public:
    HashMap(): buckets_(new Node*[16]()), capacity_(16), size_(0) {}
    ~HashMap() {
        for (std::size_t i = 0; i < capacity_; i++) {
            for (Node* curr = buckets_[i]; curr != nullptr;) {
                Node* next = curr->next;
                delete curr;
                curr = next;
            }
        }
        delete[] buckets_;
    }

    // copy constructor
    HashMap(const HashMap& other):
        buckets_(new Node*[other.capacity_]()), capacity_(other.capacity_), size_(other.size_), hash_(other.hash_) {
        for (std::size_t i = 0; i < capacity_; i++) {
            for (Node* curr = other.buckets_[i]; curr != nullptr; curr = curr->next) {
                Node* n = new Node {curr->key, curr->value, buckets_[i]};
                buckets_[i] = n;
            }
        }
    }

    // move constructor
    HashMap(HashMap&& other) noexcept:
        buckets_(other.buckets_), capacity_(other.capacity_), size_(other.size_), hash_(other.hash_) {
        other.buckets_ = nullptr;
        other.capacity_ = 0;
        other.size_ = 0;
    }

    // copy + move assignment
    // for copy assignments, due to passing by value, the copy constructor will be implicitly called
    // for move assignments, a copy is made via the move constructor (so no actual copies made)
    HashMap& operator=(HashMap other) noexcept {
        std::swap(buckets_, other.buckets_);
        std::swap(capacity_, other.capacity_);
        std::swap(size_, other.size_);
        std::swap(hash_, other.hash_);
        return *this;
    }

    void set(const K& key, V value) {
        if (capacity_ == 0) {
            rehash(16);
        }
        std::size_t i = indexFor(key, capacity_);

        V* n = find(key);
        if (!n) {
            Node* newNode = new Node {key, std::move(value), buckets_[i]};
            buckets_[i] = newNode;
            size_++;

            // size up once size gets to 75% of capacity
            if (size_ * 4 > capacity_ * 3) {
                rehash(capacity_ * 2);
            }
        } else {
            *n = std::move(value);
        }
    }

    // removes `key`: returns true if `key` existed, else false
    bool erase(const K& key) {
        if (capacity_ == 0) {
            return false;
        }
        std::size_t i = indexFor(key, capacity_);

        for (Node** curr = &buckets_[i]; *curr != nullptr; curr = &(*curr)->next) {
            if ((*curr)->key == key) {
                Node* node = *curr;
                *curr = node->next;

                delete node;
                size_--;
                return true;
            }
        }
        return false;
    }

    void clear() {
        *this = HashMap();
    }

    // const find, good for const HashMap declarations
    const V* find(const K& key) const {
        if (capacity_ == 0) {
            return nullptr;
        }
        std::size_t i = indexFor(key, capacity_);

        for (Node* curr = buckets_[i]; curr != nullptr; curr = curr->next) {
            if (curr->key == key) {
                return &curr->value;
            }
        }
        return nullptr;
    }

    // non-const find(), i.e. mutable return pointer
    V* find(const K& key) {
        return const_cast<V*>(std::as_const(*this).find(key));
    }

    // find() but instead returns a copy (value) instead of ptr
    std::optional<V> get(const K& key) const {
        const V* val = find(key);
        if (!val) {
            return std::nullopt;
        }
        return *val;
    }

    bool contains(const K& key) const {
        return find(key) != nullptr;
    }

    std::size_t size() const {
        return size_;
    }

    bool empty() const {
        return size_ == 0;
    }

    using iterator = Iter<false>;
    using const_iterator = Iter<true>;

    iterator begin() {
        iterator it(this, 0);
        // just in case bucket 0 is empty
        it.seek(0);
        return it;
    }

    iterator end() {
        // 1 position past the last element
        return iterator(this, capacity_);
    }

    const_iterator begin() const {
        const_iterator it(this, 0);
        it.seek(0);
        return it;
    }

    const_iterator end() const {
        return const_iterator(this, capacity_);
    }
};

#ifdef INTELLISENSE_CHECK_TEMPLATES
template class HashMap<std::string, std::string, FnvHash>;
#endif
