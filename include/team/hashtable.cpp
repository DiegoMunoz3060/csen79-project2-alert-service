#ifndef CUSTOM_TRIE_HASHTABLE_CPP
#define CUSTOM_TRIE_HASHTABLE_CPP

#include "hashtable.h"
#include <cstring>

namespace customTrie {

    using std::memcmp;
    using std::memcpy;
    using std::size_t;

    template <typename T>
    hashtable<T>::hashtable() {
        size_ = 0;
        cap_ = 0;
    }

    template <typename T>
    hashtable<T>::~hashtable() {
        for (size_t i = 0; i < cap_; ++i) {
            for (typename list<entry>::node* p = buckets_[i].front(); p; p = p->next) {
                delete[] p->value.key; // the list frees its nodes after the keys are freed
            }
        }
    }

    // Hashes a string segment using DJB2.
    template <typename T>
    size_t hashtable<T>::hash(const char* key, size_t len) {
        size_t h = 5381;
        for (size_t i = 0; i < len; ++i) {
            h = h * 33 + static_cast<unsigned char>(key[i]); // DJB2 string hashing
        }
        return h;
    }

    // Finds a key in its bucket and writes its value if found.
    template <typename T>
    bool hashtable<T>::find(const char* key, size_t len, T& value) const {
        if (!key || !cap_) { // no key or no buckets means nothing to find
            return false;
        }
        const list<entry>& bucket = buckets_[hash(key, len) % cap_];
        for (const typename list<entry>::node* p = bucket.front(); p; p = p->next) { // walk the chain for this bucket
            if (p->value.len == len && memcmp(p->value.key, key, len) == 0) { // compare the full key, not just the hash
                value = p->value.value;
                return true;
            }
        }
        return false;
    }

    // Expands the bucket array and redistributes the entries.
    template <typename T>
    void hashtable<T>::grow() {
        size_t cap = cap_ ? cap_ * 2 : 8; // double the bucket count, or start with 8
        vector<list<entry>> tempBuckets;
        tempBuckets.reserve(cap);
        for (size_t i = 0; i < cap; ++i) {
            tempBuckets.push_back(list<entry>());
        }
        for (size_t i = 0; i < cap_; ++i) {
            for (typename list<entry>::node* p = buckets_[i].front(); p; p = p->next) {
                size_t pos = hash(p->value.key, p->value.len) % cap; // find the new bucket
                tempBuckets[pos].push_front(p->value);
            }
        }
        buckets_.swap(tempBuckets); // the temporary vector releases the old list nodes
        cap_ = cap;
    }

    // Inserts a key and value, or updates the value if the key already exists.
    template <typename T>
    void hashtable<T>::insert(const char* key, size_t len, const T& value) {
        if (!key) {
            return;
        }
        if (cap_) { // update an existing key instead of adding a duplicate
            list<entry>& bucket = buckets_[hash(key, len) % cap_];
            for (typename list<entry>::node* p = bucket.front(); p; p = p->next) {
                if (p->value.len == len && memcmp(p->value.key, key, len) == 0) {
                    p->value.value = value;
                    return;
                }
            }
        }
        if (!cap_ || (size_ + 1) * 4 > cap_ * 3) { // grow before the load factor exceeds 0.75
            grow();
        }
        char* tempKey = new char[len + 1]; // keep our own copy of the key
        memcpy(tempKey, key, len);
        tempKey[len] = '\0';
        size_t pos = hash(key, len) % cap_;
        entry item{tempKey, len, value};
        buckets_[pos].push_front(item); // add the new entry to the front of the chain
        ++size_;
    }

    // Returns the number of entries in the table.
    template <typename T>
    size_t hashtable<T>::size() const {
        return size_;
    }

}

#endif
