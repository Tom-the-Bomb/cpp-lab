#pragma once
#include <stdexcept>
#include <string>

class LruList {
public:
    struct Node {
        std::string key;
        Node* prev = nullptr;
        Node* next = nullptr;
    };

    LruList() {
        clear();
    }

    // block copy constructor, assignment
    LruList(const LruList&) = delete;
    LruList& operator=(const LruList&) = delete;

    // LruList doesn't own nodes, simply unlink real nodes and their owners will free them
    void clear() {
        head_.next = &tail_;
        tail_.prev = &head_;
    }

    void pushFront(Node* n) {
        n->prev = &head_;
        n->next = head_.next;
        head_.next->prev = n;
        head_.next = n;
    }

    void unlink(Node* n) {
        n->next->prev = n->prev;
        n->prev->next = n->next;
        n->prev = nullptr;
        n->next = nullptr;
    }

    // mark `n` as MRU
    void moveToFront(Node* n) {
        unlink(n);
        pushFront(n);
    }

    bool empty() const {
        return head_.next == &tail_;
    }

    // return LRU node
    Node* back() const {
        if (empty()) {
            throw std::out_of_range("List is empty");
        }
        return tail_.prev;
    }
private:
    // 2 dummy nodes
    Node head_, tail_;
};
