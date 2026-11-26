#ifndef CUSTOM_TRIE_VECTOR_H
#define CUSTOM_TRIE_VECTOR_H

#include <cstddef>

namespace customTrie {

using std::size_t;

template <typename T>

    class vector {
        public:
            vector();
            vector(const vector& other);
            vector& operator=(const vector& other);
            ~vector();

            void swap(vector& other);
            void reserve(size_t capacity);
            void push_back(const T& value);
            void pop_back();
            void clear();
            size_t size() const;
            bool empty() const;

            T& operator[](size_t index);
            const T& operator[](size_t index) const;
            
        private:
            T* data_;
            size_t size_;
            size_t cap_;
    };

}
#include "vector.cpp"

#endif
