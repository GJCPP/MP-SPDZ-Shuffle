#include "block_wrapper.h"

block_wrapper::block_wrapper(const emp::block &b)
	: data(b)
{
}

block_wrapper::operator emp::block() const
{
	return data;
}

block_wrapper block_wrapper::operator^(const block_wrapper & b) const
{
	block_wrapper ret;
	ret.data = data ^ b.data;
	return ret;
}

block_wrapper& block_wrapper::operator^=(const block_wrapper & b)
{
	data ^= b.data;
	return *this;
}

block_wrapper block_wrapper::operator+(const block_wrapper &b) const
{
    return data + b.data;
}

block_wrapper& block_wrapper::operator+=(const block_wrapper &b)
{
	data += b.data;
    return *this;
}

block_wrapper block_wrapper::operator-(const block_wrapper &b) const
{
    return data - b.data;
}

block_wrapper& block_wrapper::operator-=(const block_wrapper &b)
{
	data -= b.data;
    return *this;
}

emp::block * block_wrapper::operator&()
{
	return &data;
}

const emp::block * block_wrapper::operator&() const
{
	return &data;
}

block_wrapper makeBlockWrapper(uint64_t high, uint64_t low)
{
    return emp::makeBlock(high, low);
}
