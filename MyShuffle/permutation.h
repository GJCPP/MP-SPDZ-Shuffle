#pragma once
#include <iostream>
#include <vector>
#include <random>

#include "global.h"

#include "vectors.h"

class permutation {
public:
    permutation() = default;
    permutation(size_t n);
    permutation(size_t n, bool random);

    bool operator==(const permutation& beta) const;
    bool operator!=(const permutation& beta) const;

    size_t& operator[](size_t pos);
    const size_t& operator[](size_t pos) const;

    permutation operator*(const permutation& beta) const;
    permutation inverse() const;

    template <typename T>
    void perform(std::vector<T>& vec) const {
        if (n != vec.size()) {
            std::cerr << "permutation::perform: size mismatch, " << n << " != " << vec.size() << std::endl;
            throw std::runtime_error("permutation::perform: size mismatch.");
        }
        std::vector<T> tmp(vec.size());
        for (size_t i(0); i != n; ++i) {
            tmp[i] = vec[perm[i]];
        }
        vec = tmp;
    }

    template <typename T>
    void perform(vectors<T>& vec) const {
        if (n != vec.num) {
            std::cerr << "permutation::perform: size mismatch, " << n << " != " << vec.num << std::endl;
            throw std::runtime_error("permutation::perform: size mismatch.");
        }
        vectors<T> tmp(vec.num, vec.len);
        for (size_t i(0); i != n; ++i) {
            for (size_t j(0); j != vec.len; ++j) {
                tmp[i][j] = vec[perm[i]][j];
            }
        }
        vec = tmp;
    }

    size_t n;
    std::vector<size_t> perm;
};
