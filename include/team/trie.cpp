#ifndef CUSTOM_TRIE_TRIE_CPP
#define CUSTOM_TRIE_TRIE_CPP

#include "trie.h"

namespace customTrie {

    using std::size_t;

    // Adds a node and returns its stable ID.
    template <typename T>
    size_t trie<T>::addNode() {
        if (size_ == cap_) {
            size_t cap = cap_ ? cap_ * 2 : 8; // double the pointer array, or start with 8 slots
            node** tempNodes = new node*[cap];
            for (size_t i = 0; i < size_; ++i) {
                tempNodes[i] = nodes_[i];
            }
            delete[] nodes_;
            nodes_ = tempNodes;
            cap_ = cap;
        }
        nodes_[size_] = new node; // nodes stay in place even when the pointer array grows
        return size_++;
    }

    // Checks that a path has no empty segments.
    template <typename T>
    bool trie<T>::valid(const char* path) {
        if (!path || !*path || *path == '/') { // reject null, empty, or leading-empty paths
            return false;
        }
        for (const char* p = path; *p; ++p) {
            if (*p == '/' && (p[1] == '/' || p[1] == '\0')) { // no empty segments or trailing slash
                return false;
            }
        }
        return true;
    }

    template <typename T>
    trie<T>::trie() {
        nodes_ = nullptr;
        size_ = 0;
        cap_ = 0;
        addNode(); // node 0 is the root
    }

    template <typename T>
    trie<T>::~trie() {
        for (size_t i = 0; i < size_; ++i) {
            delete nodes_[i];
        }
        delete[] nodes_;
    }

    // Finds an existing topic and writes its node ID if found.
    template <typename T>
    bool trie<T>::findTopic(const char* path, size_t& result) const {
        if (!valid(path)) {
            return false;
        }
        size_t id = 0;
        const char* part = path;
        for (const char* p = path; true; ++p) {
            if (*p == '/' || !*p) {
                if (!nodes_[id]->children.find(part, static_cast<size_t>(p - part), id)) { // follow each path segment
                    return false;
                }
                if (!*p) {
                    result = id;
                    return true;
                }
                part = p + 1;
            }
        }
    }

    // Finds or creates each segment of a valid path and returns the final node ID.
    template <typename T>
    size_t trie<T>::ensureTopic(const char* path) {
        size_t id = 0;
        const char* part = path;
        for (const char* p = path;; ++p) {
            if (*p == '/' || !*p) {
                size_t next;
                size_t len = static_cast<size_t>(p - part);
                if (!nodes_[id]->children.find(part, len, next)) { // create missing segments as we go
                    next = addNode();
                    nodes_[id]->children.insert(part, len, next);
                }
                id = next;
                if (!*p) {
                    return id;
                }
                part = p + 1;
            }
        }
    }

    // Adds a subscriber to a node unless the same object is already present.
    template <typename T>
    void trie<T>::addSubscription(size_t id, T& subscriber) {
        vector<T*>& subscribers = nodes_[id]->subscribers;
        for (size_t i = 0; i < subscribers.size(); ++i) {
            if (subscribers[i] == &subscriber) {
                return;
            }
        }
        subscribers.push_back(&subscriber);
    }

    // Removes a subscriber from a node and returns whether it was found.
    template <typename T>
    bool trie<T>::removeSubscription(size_t id, const T& subscriber) {
        if (id >= size_) {
            return false;
        }
        vector<T*>& subscribers = nodes_[id]->subscribers;
        for (size_t i = 0; i < subscribers.size(); ++i) {
            if (subscribers[i] == &subscriber) {
                subscribers[i] = subscribers[subscribers.size() - 1]; // fill the gap with the last subscriber
                subscribers.pop_back();
                return true;
            }
        }
        return false;
    }

    // Appends subscriber references from each existing prefix of a path.
    template <typename T>
    void trie<T>::visitMatches(const char* path, vector<T*>& subscribers) const {
        if (!valid(path)) {
            return;
        }
        size_t id = 0;
        const char* part = path;
        for (const char* p = path;; ++p) {
            if (*p == '/' || !*p) {
                if (!nodes_[id]->children.find(part, static_cast<size_t>(p - part), id)) {
                    return;
                }
                const vector<T*>& subs = nodes_[id]->subscribers; // collect subscribers at each existing prefix
                for (size_t i = 0; i < subs.size(); ++i) {
                    subscribers.push_back(subs[i]);
                }
                if (!*p) {
                    return;
                }
                part = p + 1;
            }
        }
    }

}

#endif
