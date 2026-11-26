#ifndef CUSTOM_TRIE_VECTOR_CPP
#define CUSTOM_TRIE_VECTOR_CPP

#include "vector.h"

namespace customTrie {

using std::size_t;

    template <typename T>
    void vector<T>::swap(vector& other) {
        T* tempData = data_;
        data_ = other.data_;
        other.data_ = tempData;
        size_t tempSize = size_;
        size_ = other.size_;
        other.size_ = tempSize;
        size_t tempCap = cap_;
        cap_ = other.cap_;
        other.cap_ = tempCap;
    }

    template <typename T>
    void vector<T>::reserve(size_t capacity) {
        if (capacity <= cap_) { // no expansion if there's space
            return;
        }
        vector tempVector;
        tempVector.data_ = new T[capacity];
        tempVector.cap_ = capacity;
        for (size_t i = 0; i < size_; ++i) {
            tempVector.data_[i] = data_[i];
        }
        T* tempData = data_;
        data_ = tempVector.data_;
        tempVector.data_ = tempData;
        cap_ = capacity;
    }

    template <typename T>
    vector<T>::vector() {
        data_ = nullptr;
        size_ = 0;
        cap_ = 0;
    }

    template <typename T>
    vector<T>::vector(const vector& other) {
        data_ = nullptr;
        size_ = 0;
        cap_ = 0;
        vector tempVector;
        tempVector.reserve(other.size_);
        for (size_t i = 0; i < other.size_; ++i) {
            tempVector.push_back(other.data_[i]);
        }
        data_ = tempVector.data_;
        size_ = tempVector.size_;
        cap_ = tempVector.cap_;
        tempVector.data_ = nullptr;
        tempVector.size_ = 0;
        tempVector.cap_ = 0;
    }

    template <typename T>
    vector<T>::~vector() {
        delete[] data_;
    }

    template <typename T>
    vector<T>& vector<T>::operator=(const vector& other) {
        if (this != &other) {
            vector tempVector(other);
            T* tempData = data_;
            data_ = tempVector.data_;
            tempVector.data_ = tempData;
            size_t n = size_;
            size_ = tempVector.size_;
            tempVector.size_ = n;
            n = cap_;
            cap_ = tempVector.cap_;
            tempVector.cap_ = n;
        }
        return *this;
    }

    template <typename T>
    void vector<T>::push_back(const T& value) {
        if (size_ == cap_) {
            T tempValue = value;
            reserve(cap_ ? cap_ * 2 : 4); // either double the storage space, or set to 4 if the vector had no space at all (size_ = cap_ = 0)
            data_[size_] = tempValue;
        }
        else {
            data_[size_] = value;
        }
        ++size_;
    }

    template <typename T>
    void vector<T>::pop_back() {
        if (size_) {
            --size_; // simply don't let the user access the last element
        }
    }

    template <typename T>
    void vector<T>::clear() {
        size_ = 0; // same logic
    }

    template <typename T>
    size_t vector<T>::size() const {
        return size_;
    }

    template <typename T>
    bool vector<T>::empty() const {
        return size_ == 0;
    }

    template <typename T>
    T& vector<T>::operator[](size_t index) {
        return data_[index];
    }

    template <typename T>
    const T& vector<T>::operator[](size_t index) const {
        return data_[index];
    }

}

#endif
