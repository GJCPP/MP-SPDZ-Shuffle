#pragma once
#include <iostream>
#include <vector>
#include <random>

#include "global.h"

class permutation {
public:
    permutation() = default;
    permutation(int n);
    permutation(int n, bool random);

    bool operator==(const permutation& beta) const;
    bool operator!=(const permutation& beta) const;

    int& operator[](int pos);
    const int& operator[](int pos) const;

    permutation operator*(const permutation& beta) const;
    permutation inverse() const;

    int n;
    std::vector<int> perm;
};
