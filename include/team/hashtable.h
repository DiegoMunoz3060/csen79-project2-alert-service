#ifndef CUSTOM_TRIE_HASHTABLE_H
#define CUSTOM_TRIE_HASHTABLE_H

#include <cstddef>
#include "list.h"
#include "vector.h"

namespace customTrie {

using std::size_t;

    // Maps a string segment to a node ID using separate chaining.
    template <typename T>
    class hashtable {

        public:
            hashtable();
            ~hashtable();
            hashtable(const hashtable&) = delete;
            hashtable& operator=(const hashtable&) = delete;
            
            bool find(const char* key, size_t len, T& value) const;
            void insert(const char* key, size_t len, const T& value);
            size_t size() const;

        private:
            struct entry { 
                char* key; 
                size_t len; 
                T value; 
            };
            vector<list<entry>> buckets_;
            size_t size_;
            size_t cap_;
            static size_t hash(const char* key, size_t len);
            void grow();
        };

}
#include "hashtable.cpp"

#endif
