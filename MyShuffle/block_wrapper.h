#pragma once

#include "Tools/random.h"
#include "Tools/Hash.h"

#define BLOCKS_FOR_HASH  (Hash::hash_length / SEED_SIZE)

class block_wrapper {
public:
    block_wrapper() = default;

    block_wrapper operator^(const block_wrapper& b) const;
    block_wrapper& operator^=(const block_wrapper& b);
    block_wrapper operator+(const block_wrapper& b) const;
    block_wrapper& operator+=(const block_wrapper& b);
    block_wrapper operator-(const block_wrapper& b) const;
    block_wrapper& operator-=(const block_wrapper& b);

    bool is_zero() const;
    bool is_nonzero() const;

    octet data[SEED_SIZE];
};

block_wrapper makeBlockWrapper(uint64_t high, uint64_t low);
