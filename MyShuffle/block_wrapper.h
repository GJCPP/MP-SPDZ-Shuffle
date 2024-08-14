#pragma once

#include <emp-tool/utils/block.h>

class block_wrapper {
public:
    block_wrapper() = default;
    block_wrapper(const emp::block& b);
    
    operator emp::block() const;

    block_wrapper operator^(const block_wrapper& b) const;
    block_wrapper& operator^=(const block_wrapper& b);
    block_wrapper operator+(const block_wrapper& b) const;
    block_wrapper& operator+=(const block_wrapper& b);
    block_wrapper operator-(const block_wrapper& b) const;
    block_wrapper& operator-=(const block_wrapper& b);

    emp::block *operator&();
    const emp::block *operator&() const;

    emp::block data;
};

block_wrapper makeBlockWrapper(uint64_t high, uint64_t low);
