#ifndef CUSTOM_TRIE_LIST_CPP
#define CUSTOM_TRIE_LIST_CPP

#include "list.h"

namespace customTrie {

using std::size_t;

    template <typename T>
    list<T>::list() {
        head_ = nullptr;
        tail_ = nullptr;
        size_ = 0;
    }

    // Swaps *this list with the given other (no deletion involved, private function)
    // Because this is a linked list we simply need to relink all the pointers & not copy the data
    template <typename T>
    void list<T>::swap(list& other) {
        node* p = head_;
        head_ = other.head_;
        other.head_ = p;
        p = tail_;
        tail_ = other.tail_;
        other.tail_ = p;
        size_t n = size_;
        size_ = other.size_;
        other.size_ = n;
    }

    template <typename T>
    typename list<T>::node* list<T>::push_front(const T& value) {
        node* p = new node{value, nullptr, head_};
        if (head_) { // nullptr can be inherently converted to 0/false
            head_->prev = p; // if we have a head element (i.e. the list isn't empty) we simply relink
        }
        else {
            tail_ = p; // otherwise we gotta also set tail_ to make sure it's not nullptr after we insert a first element
        }
        head_ = p;
        ++size_;
        return p;
    }

    template <typename T>
    typename list<T>::node* list<T>::push_back(const T& value) { // almost the same logic as before
        node* p = new node{value, tail_, nullptr};
        if (tail_) {
            tail_->next = p;
        }
        else {
            head_ = p;
        }
        tail_ = p;
        ++size_;
        return p;
    }

    template <typename T>
    void list<T>::erase(node* p) {
        if (!p) {
            return; // given node is nullptr
        }
        if (p->prev) {
            p->prev->next = p->next; // not start of list, relink the prev element
        }
        else {
            head_ = p->next; // start of list, reset head
        }
        if (p->next) {
            p->next->prev = p->prev; // same logic for end of list
        }
        else {
            tail_ = p->prev;
        }
        delete p;
        --size_;
    }

    template <typename T>
    void list<T>::pop_front() {
        erase(head_);
    }

    template <typename T>
    void list<T>::pop_back() {
        erase(tail_);
    }

    template <typename T>
    void list<T>::clear() {
        while (head_) {
            pop_front(); // keep popping front until list is empty
        }
    }

    template <typename T>
    list<T>::~list() {
        clear();
    }

    template <typename T>
    list<T>::list(const list& other) {
        head_ = nullptr;
        tail_ = nullptr;
        size_ = 0;
        list tempList;
        for (const node* p = other.head_; p != nullptr; p = p->next) {
            tempList.push_back(p->value);
        }
        swap(tempList);
    }

    template <typename T>
    list<T>& list<T>::operator=(const list& other) {
        if (this != &other) {
            list tempList(other);
            swap(tempList);
        }
        return *this;
    }

    template <typename T>
    typename list<T>::node* list<T>::front() {
        return head_;
    }

    template <typename T>
    const typename list<T>::node* list<T>::front() const {
        return head_;
    }

    template <typename T>
    typename list<T>::node* list<T>::back() {
        return tail_;
    }

    template <typename T>
    const typename list<T>::node* list<T>::back() const {
        return tail_;
    }

    template <typename T>
    size_t list<T>::size() const {
        return size_;
    }

    template <typename T>
    bool list<T>::empty() const {
        return (size_ == 0);
    }

}

#endif
