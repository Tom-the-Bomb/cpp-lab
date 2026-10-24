#pragma once

#include <bit>
#include <cstddef>
#include <functional>
#include <limits>
#include <utility>
#include <vector>

template <typename K, typename V, typename Hash = std::hash<K>> class OpenHashMap {
public:
    // we could take const K& key, but we need to move when new key, only don't if duplicate
    // so const K& is only better if we have a lot of duplicates otherwise we are going to do a lot of copies
    //
    // `insert` is not "set", if `key` exists, nothing happens, return `false`
    bool insert(K key, V val) {
        probes_ = 1;
        if (needsRehash()) {
            std::size_t cap = capacity();
            // rehash to double curr capacity if # live keys would fill ~> half
            // else we can just keep the curr capacity (still better since tombstones are dropped)
            rehash((size_ + 1) * 2 > cap ? cap * 2 : cap);
        }
        std::size_t probe = home(key);
        std::size_t cap = capacity();
        // pos = cap is sentinel MISSING value
        std::size_t pos = cap;
        // find first empty/deleted slot to insert into
        // but also ensure key doesnt exist so we don't duplicate insert
        // won't happen if we reach empty first (since empty = key DNE)
        while (slots_[probe].state != SlotState::Empty) {
            // found key
            if (slots_[probe].state == SlotState::Full && slots_[probe].key == key) {
                return false;
            }
            // remember the first tombstone we see as that is the ideal insertion location
            // but we can't insert yet because the key could exist later on
            // tombstone's whole job is to indicate to us the key could still exist later
            if (pos == cap && slots_[probe].state == SlotState::Deleted) {
                pos = probe;
            }
            probe = (probe + 1) & (cap - 1);
            probes_++;
        }
        // insert @ empty if no earlier tombstone was found
        if (pos == cap) {
            pos = probe;
        }
        if (slots_[pos].state == SlotState::Empty) {
            // otherwise if `Deleted` it was already counted
            used_++;
        }
        slots_[pos].state = SlotState::Full;
        slots_[pos].key = std::move(key);
        slots_[pos].val = std::move(val);
        size_++;
        return true;
    }

    bool erase(const K& key) {
        std::size_t i = indexOf(key);
        if (i == capacity()) {
            return false;
        }
        slots_[i].state = SlotState::Deleted;
        size_--;
        return true;
    }

    const V* find(const K& key) const {
        std::size_t i = indexOf(key);
        if (i == capacity()) {
            return nullptr;
        }
        return &slots_[i].val;
    }

    bool contains(const K& key) const {
        return indexOf(key) != capacity();
    }

    std::size_t size() const {
        return size_;
    }

    std::size_t lastProbes() const {
        return probes_;
    }
private:
    enum struct SlotState {
        Empty,
        Full,
        Deleted,
    };

    struct Slot {
        SlotState state = SlotState::Empty;
        K key {};
        V val {};
    };

    static constexpr std::size_t initialCapacity = 16;

    // unlike kvstore's HashMap, we use a vector
    // we only used a heap allocated array before for learning purposes
    // either implementation can be used in both cases, vector means less work for us
    std::vector<Slot> slots_ = std::vector<Slot>(initialCapacity);
    std::size_t size_ = 0; // # of actually `Full`
    std::size_t used_ = 0; // # of non-`Empty` slots
    mutable std::size_t probes_ = 0;

    std::size_t capacity() const {
        return slots_.size();
    }

    // preferred slot (start of probe sequence)
    std::size_t home(const K& key) const {
        // implement mixing to relieve primary clustering
        // fibonacci hashing: 0x9E3779B97F4A7C15 = 2^64 / φ
        // and use top `b` bits (>> (64 - b)) instead of bottom `b` (& (cap - 1))
        // where capacity = 2^b (b = # 0-bit on right side) (note: cap - 1 => `b` 1-bit on right side)
        return (Hash {}(key) * 0x9E3779B97F4A7C15) >>
               (std::numeric_limits<std::size_t>::digits - std::countr_zero(capacity()));
    }

    // used for searching:
    // since the 70% load limit guarantees empty slots always exist
    // we don't need an m-probe guard, where m is the capacity
    // (this is the case when all slots are filled, but the element doesn't exist, leading to the probe becoming an
    // infinite loop) we either stop when we reach an empty slot or we find the key
    //
    // we use `!= Empty` instead of `== Full` to account for tombstones
    // ex: abc[d] (insert d, home=a) if b gets deleted, without tombstones when searching for d after, we will never
    // find it as we will always stop at the slot b was previously at
    //
    // returns `capacity()` if not found
    std::size_t indexOf(const K& key) const {
        probes_ = 1;
        std::size_t probe = home(key);
        std::size_t cap = capacity();
        while (slots_[probe].state != SlotState::Empty) {
            if (slots_[probe].state == SlotState::Full && slots_[probe].key == key) {
                return probe;
            }
            probe = (probe + 1) & (cap - 1);
            probes_++;
        }
        return cap;
    }

    bool needsRehash() const {
        // 70% load threshold
        return used_ * 10 > capacity() * 7;
    }

    void rehash(std::size_t newCapacity) {
        // allocate new vector
        // but we must swap into slots_ right away since home(k) uses capacity()
        std::vector<Slot> old = std::exchange(slots_, std::vector<Slot>(newCapacity));
        size_ = 0;
        used_ = 0;
        for (Slot& s : old) {
            if (s.state == SlotState::Full) {
                std::size_t probe = home(s.key);
                std::size_t cap = capacity();
                while (slots_[probe].state == SlotState::Full) {
                    probe = (probe + 1) & (cap - 1);
                }
                slots_[probe].key = std::move(s.key);
                slots_[probe].val = std::move(s.val);
                slots_[probe].state = SlotState::Full;
                size_++;
                used_++;
            }
        }
    }
};

#ifdef INTELLISENSE_CHECK_TEMPLATES
template class OpenHashMap<int, int>;
#endif
