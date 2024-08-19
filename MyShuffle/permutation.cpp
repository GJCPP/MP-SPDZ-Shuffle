#include "permutation.h"

permutation::permutation(size_t _n) 
    : n(_n), perm(_n)
{
    for (int i(0); i != n; ++i) perm[i] = i;
}

permutation::permutation(size_t _n, bool random)
    : n(_n), perm(_n)
{
    for (int i(0); i != n; ++i) perm[i] = i;
    if (random) {
        std::shuffle(perm.begin(), perm.end(), std::mt19937(std::random_device()()));
    }
}

bool permutation::operator==(const permutation &beta) const
{
    if (n != beta.n) return false;
    for (int i(0); i != n; ++i) {
        if (perm[i] != beta.perm[i]) return false;
    }
    return true;
}

bool permutation::operator!=(const permutation& beta) const
{
    return !(*this == beta);
}

size_t& permutation::operator[](size_t pos)
{
#ifdef DEBUG
    if (pos < 0 || pos >= n) {
        std::cerr << "permutation::operator[] : out of range, " << pos << "/" << n << std::endl;
        throw std::runtime_error("permutation::operator[] : Out of range.");
    }
#endif
    return perm[pos];
}
const size_t& permutation::operator[](size_t pos) const
{
#ifdef DEBUG
    if (pos < 0 || pos >= n) {
        std::cerr << "permutation::operator[] : out of range, " << pos << "/" << n << std::endl;
        throw std::runtime_error("permutation::operator[] : Out of range.");
    }
#endif
    return perm[pos];
}

// The left permutation is perform BEFORE the right one.
permutation permutation::operator*(const permutation& beta) const {
    permutation ret(n);
    if (beta.n != n) {
        std::cerr << "Class permutation operator* : inconsistent size, " << n << "/" << beta.n << std::endl;
        throw std::runtime_error("Class permutation operator* : Inconsistent size.");
    }
    for (int i(0); i != n; ++i) {
        ret[i] = perm[beta[i]];
    }
    return ret;
}

permutation permutation::inverse() const {
    permutation ret(n);
    for (int i(0); i != n; ++i) {
        ret[perm[i]] = i;
    }
    return ret;
}
