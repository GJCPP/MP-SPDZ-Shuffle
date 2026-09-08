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

bool block_wrapper::operator==(const block_wrapper &b) const
{
	for (int i(0); i != SEED_SIZE; ++i)
		if (data[i] != b.data[i]) return false;
	return true;
}

bool block_wrapper::operator!=(const block_wrapper &b) const
{
	for (int i(0); i != SEED_SIZE; ++i)
		if (data[i] != b.data[i]) return true;
	return false;
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

void block_wrapper::pack(octetStream &o, int n) const
{
    (void) n;
	o.append((octet*) &data,sizeof(data));
}

void block_wrapper::unpack(octetStream &o, int n)
{
    (void) n;
	o.consume((octet*) &data,sizeof(data));
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

std::ostream &operator<<(std::ostream &os, const block_wrapper &b)
{
	// Same as operator<<(..., osuCrypto::block b) in deps/libOTe/cryptoTools/cryptoTools/Common/block.cpp
    os << std::hex;
    uint64_t* data = (uint64_t*)b.data;

    os << std::setw(16) << std::setfill('0') << data[1]
        << std::setw(16) << std::setfill('0') << data[0];

    os << std::dec << std::setw(0);
    return os;
}
