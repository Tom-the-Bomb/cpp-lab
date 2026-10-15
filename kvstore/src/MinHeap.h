#pragma once
#include <cstddef>
#include <functional>
#include <stdexcept>
#include <utility>
#include <vector>

template <typename T, typename Less = std::less<T>>
class MinHeap {
public:
    void pop() {
        if (empty()) {
            throw std::out_of_range("Heap is empty");
        }

        // we can only remove the last item efficiently
        // so move root there via swapping
        std::swap(data_.front(), data_.back());
        data_.pop_back();

        // sift down to preserve ordering rule
        if (!data_.empty()) {
            siftDown(0);
        }
    }

    void push(T x) {
        data_.push_back(std::move(x));
        siftUp(data_.size() - 1);
    }

    const T& top() const {
        if (empty()) {
            throw std::out_of_range("Heap is empty");
        }
        return data_.front();
    }

    bool empty() const {
        return data_.empty();
    }

    std::size_t size() const {
        return data_.size();
    }
private:
    // binary tree representation where each parent <= its children
    std::vector<T> data_;
    Less less_;

    void siftUp(std::size_t i) {
        while (i > 0) {
            std::size_t par = (i - 1) / 2;

            if (less_(data_[i], data_[par])) {
                std::swap(data_[i], data_[par]);
                i = par;
            } else {
                return;
            }
        }
    }

    void siftDown(std::size_t i) {
        std::size_t n = data_.size();

        while (true) {
            std::size_t smallest = i, l = 2 * i + 1, r = l + 1;

            if (l < n && less_(data_[l], data_[smallest])) {
                smallest = l;
            }
            if (r < n && less_(data_[r], data_[smallest])) {
                smallest = r;
            }
            if (smallest == i) {
                return;
            }
            std::swap(data_[i], data_[smallest]);
            // the element we need to keep sifting down is now in the spot the originally min element was at
            i = smallest;
        }
    }
};

#ifdef INTELLISENSE_CHECK_TEMPLATES
template class MinHeap<int>;
template class MinHeap<int, std::greater<int>>;
#endif
