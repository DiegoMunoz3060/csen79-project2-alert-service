#ifndef CUSTOM_TRIE_LIST_H
#define CUSTOM_TRIE_LIST_H

#include <cstddef>

namespace customTrie {

using std::size_t;

template <typename T>
class list {

    public:

        struct node {
            T value;
            node* prev;
            node* next;
        };

        list();
        list(const list& other);
        list& operator=(const list& other);
        ~list();

        node* push_front(const T& value);
        node* push_back(const T& value);
        void erase(node* pos);
        void pop_front();
        void pop_back();
        void clear();

        node* front();
        const node* front() const;
        node* back();
        const node* back() const;
        size_t size() const;
        bool empty() const;

    private:
        node* head_;
        node* tail_;
        size_t size_;
        void swap(list& other);
    };

}
#include "list.cpp"

#endif
