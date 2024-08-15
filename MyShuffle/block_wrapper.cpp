#include "block_wrapper.h"

block_wrapper block_wrapper::operator^(const block_wrapper & b) const
{
	block_wrapper ret;
	for (int i(0); i != SEED_SIZE; ++i)
		ret.data[i] = data[i] ^ b.data[i];
	return ret;
}

block_wrapper& block_wrapper::operator^=(const block_wrapper & b)
{
	for (int i(0); i != SEED_SIZE; ++i)
		data[i] ^= b.data[i];
	return *this;
}

block_wrapper block_wrapper::operator+(const block_wrapper &b) const
{
	block_wrapper ret;
	for (int i(0); i != SEED_SIZE; ++i)
		ret.data[i] = data[i] + b.data[i];
    return ret;
}

block_wrapper& block_wrapper::operator+=(const block_wrapper &b)
{
	for (int i(0); i != SEED_SIZE; ++i)
		data[i] += b.data[i];
    return *this;
}

block_wrapper block_wrapper::operator-(const block_wrapper &b) const
{
	block_wrapper ret;
	for (int i(0); i != SEED_SIZE; ++i)
		ret.data[i] = data[i] - b.data[i];
    return ret;
}

block_wrapper& block_wrapper::operator-=(const block_wrapper &b)
{
	for (int i(0); i != SEED_SIZE; ++i)
		data[i] -= b.data[i];
    return *this;
}

bool block_wrapper::is_zero() const
{
	for (int i(0); i != SEED_SIZE; ++i)
		if (data[i] != 0) return false;
    return true;
}

bool block_wrapper::is_nonzero() const
{
	for (int i(0); i != SEED_SIZE; ++i)
		if (data[i] != 0) return true;
    return false;
}

block_wrapper makeBlockWrapper(uint64_t high, uint64_t low)
{
	block_wrapper ret;
	for (int i(0); i != 8; ++i)
		ret.data[i] = (high >> (8 * (7 - i))) & 0xff;
	for (int i(0); i != 8; ++i)
		ret.data[i + 8] = (low >> (8 * (7 - i))) & 0xff;
	return ret;
}
