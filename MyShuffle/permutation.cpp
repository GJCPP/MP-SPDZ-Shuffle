#include "permutation.h"

#include "Tools/random.h"

namespace {
    size_t random_below(PRNG& prg, size_t upper)
    {
        const word bound = static_cast<word>(upper);
        const word threshold = -bound % bound;
        word sample;
        do {
            sample = prg.get_word();
        } while (sample < threshold);
        return static_cast<size_t>(sample % bound);
    }
}

permutation::permutation(size_t _n) 
    : n(_n), perm(_n)
{
    for (size_t i(0); i != n; ++i) perm[i] = i;
}

permutation::permutation(size_t _n, bool random)
    : n(_n), perm(_n)
{
    for (size_t i(0); i != n; ++i) perm[i] = i;
    if (random) {
        SeededPRNG prg;
        for (size_t i = n; i > 1; --i) {
            std::swap(perm[i - 1], perm[random_below(prg, i)]);
        }
    }
}

bool permutation::operator==(const permutation &beta) const
{
    if (n != beta.n) return false;
    for (size_t i(0); i != n; ++i) {
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
    if (pos >= n) {
        std::cerr << "permutation::operator[] : out of range, " << pos << "/" << n << std::endl;
        throw std::runtime_error("permutation::operator[] : Out of range.");
    }
#endif
    return perm[pos];
}
const size_t& permutation::operator[](size_t pos) const
{
#ifdef DEBUG
    if (pos >= n) {
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
    for (size_t i(0); i != n; ++i) {
        ret[i] = perm[beta[i]];
    }
    return ret;
}

permutation permutation::inverse() const {
    permutation ret(n);
    for (size_t i(0); i != n; ++i) {
        ret[perm[i]] = i;
    }
    return ret;
}
