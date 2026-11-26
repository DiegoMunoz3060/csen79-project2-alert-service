#ifndef CUSTOM_TRIE_TRIE_H
#define CUSTOM_TRIE_TRIE_H

#include "hashtable.h"
#include "vector.h"

namespace customTrie {

using std::size_t;

    template <typename T>
    class trie {

        public:
            trie();
            ~trie();
            trie(const trie&) = delete;
            trie& operator=(const trie&) = delete;

            bool findTopic(const char* path, size_t& node) const;
            size_t ensureTopic(const char* path);

            void addSubscription(size_t node, T& subscriber);
            bool removeSubscription(size_t node, const T& subscriber);

            void visitMatches(const char* path, vector<T*>& subscribers) const;

        private:
            struct node {
                hashtable<size_t> children; 
                vector<T*> subscribers; // non-owning references to subscribers
            };
            node** nodes_;
            size_t size_;
            size_t cap_;
            size_t addNode();
            static bool valid(const char* path);

    };

}
#include "trie.cpp"

#endif
