#pragma once
#include <boost/multiprecision/cpp_int.hpp> 
#include <boost/serialization/access.hpp>
using boost::multiprecision::uint128_t, boost::multiprecision::uint256_t;
typedef uint128_t fint;
const uint128_t mod((uint128_t(1) << 127) - 697);
//const uint128_t mod((uint128_t(1) << 31) - 1);
const unsigned int mod2(((unsigned int)1<<31)-1);
//const unsigned int mod2(19);
//const unsigned int mod2(100003);
template <class Archive>
void serialize(uint128_t& val, Archive& ar, const unsigned int version) {
	val.serialize(ar, version);
}

template <class Archive>
void serialize(unsigned int& val, Archive& ar, const unsigned int) {
// #pragma unused(version)
	ar & val;
}

template <class Archive>
void serialize(unsigned short& val, Archive& ar, const unsigned int) {
// #pragma unused(version)
	ar & val;
}

template <typename fint, typename mul_fint, const fint& mod>
class field_int 
{
public:
	typedef fint basic_int;
	typedef mul_fint multi_int;
	friend std::ostream& operator<<(std::ostream& os, const field_int<fint, mul_fint, mod>& f) {
		os << f.val;
		return os;
	}
	friend class boost::serialization::access;
	field_int();
	field_int(int v);
	field_int(unsigned int v);
	field_int(unsigned long long v);
	field_int(uint128_t v);
	field_int(uint256_t v);

	template<class Archive>
	void serialize(Archive& ar, const unsigned int version)
	{
		::serialize(val, ar, version);
	}

	field_int<fint, mul_fint, mod> operator+(const field_int<fint, mul_fint, mod>& i) const;
	field_int<fint, mul_fint, mod> operator-(const field_int<fint, mul_fint, mod>& i) const;
	field_int<fint, mul_fint, mod> operator*(const field_int<fint, mul_fint, mod>& i) const;
	field_int<fint, mul_fint, mod> operator/(const field_int<fint, mul_fint, mod>& i) const;
	field_int<fint, mul_fint, mod> operator&(const field_int<fint, mul_fint, mod>& i) const;
	field_int<fint, mul_fint, mod> operator&(int i) const;
	field_int<fint, mul_fint, mod>& operator+=(const field_int<fint, mul_fint, mod>& i);
	field_int<fint, mul_fint, mod>& operator-=(const field_int<fint, mul_fint, mod>& i);
	field_int<fint, mul_fint, mod>& operator*=(const field_int<fint, mul_fint, mod>& i);
	field_int<fint, mul_fint, mod>& operator/=(const field_int<fint, mul_fint, mod>& i);
	field_int<fint, mul_fint, mod>& operator%=(int i);
	field_int<fint, mul_fint, mod>& operator<<=(int i);
	field_int<fint, mul_fint, mod>& operator>>=(int i);

	field_int<fint, mul_fint, mod> operator+(const fint i) const;
	field_int<fint, mul_fint, mod> operator-(const fint i) const;
	field_int<fint, mul_fint, mod> operator-() const;
	field_int<fint, mul_fint, mod> operator*(const fint i) const;
	field_int<fint, mul_fint, mod> operator/(const fint i) const;
	field_int<fint, mul_fint, mod> operator<<(int i) const;
	field_int<fint, mul_fint, mod> operator>>(int i) const;

	bool operator!=(const field_int<fint, mul_fint, mod>& i) const {
		return val != i.val;
	}
	bool operator!=(int i) const {
		return val != fint(i);
	}
	

	bool operator<(const field_int<fint, mul_fint, mod>& i) const {
		return val < i.val;
	}
	bool operator<=(const field_int<fint, mul_fint, mod>& i) const {
		return val <= i.val;
	}
	bool operator==(const field_int<fint, mul_fint, mod>& i) const {
		return val == i.val;
	}
	bool operator>(const field_int<fint, mul_fint, mod>& i) const {
		return val > i.val;
	}
	bool operator>=(const field_int<fint, mul_fint, mod>& i) const {
		return val >= i.val;
	}

	bool operator<(int i) const {
		return val < static_cast<fint>(i);
	}
	bool operator<=(int i) const {
		return val <= static_cast<fint>(i);
	}
	bool operator==(int i) const {
		return val == static_cast<fint>(i);
	}
	bool operator>(int i) const {
		return val > static_cast<fint>(i);
	}
	bool operator>=(int i) const {
		return val >= static_cast<fint>(i);
	}


	operator fint() const;
	operator int() const;

	bool nonzero() const;
	bool is_zero() const;

	field_int<fint, mul_fint, mod> inverse() const;
	field_int<fint, mul_fint, mod> sqrt() const;

	static int bit_length() { 
		fint tem = mod;
		int cnt = 0;
		while (tem) {
			tem >>= 1;
			++cnt;
		}
		return cnt;
	}
	static fint modulo() {
		return mod;
	}

	fint val;
};

template <typename fint, typename mul_fint, const fint& mod>
inline void exgcd(fint a, fint b, fint& x, fint& y) {
	if (a % b == 0) {
		x = 0, y = 1;
		return;
	}
	exgcd<fint, mul_fint, mod>(b, a % b, y, x);

	fint t = static_cast<fint>(((a / b * mul_fint(x)) % mod));
	y += mod - t;
	if (y >= mod) y -= mod;
	//y -= (a / b * x) % mod;
	//y -= static_cast<long long>(a / b) * x % mod;
	//y %= mod;
}


template <typename fint, typename mul_fint, const fint& mod>
inline field_int<fint, mul_fint, mod>::field_int()
	: val(0)
{
}

template<typename fint, typename mul_fint, const fint& mod>
inline field_int<fint, mul_fint, mod>::field_int(int v)
{
	if (v >= 0) val = v;
	else {
		val = (mod + v) % mod;
	}
}


template <typename fint, typename mul_fint, const fint& mod>
inline field_int<fint, mul_fint, mod>::field_int(unsigned int v)
{
	val = v % mod;
}

template <typename fint, typename mul_fint, const fint& mod>
inline field_int<fint, mul_fint, mod>::field_int(unsigned long long v)
	: val(v% mod)
{
}

template <typename fint, typename mul_fint, const fint& mod>
inline field_int<fint, mul_fint, mod>::field_int(uint128_t v)
	: val(v% mod)
{
	if (val < 0) val += mod;
}

template <typename fint, typename mul_fint, const fint& mod>
inline field_int<fint, mul_fint, mod>::field_int(uint256_t v)
	: val(v% mod)
{
	if (val < 0) val += mod;
}

template <typename fint, typename mul_fint, const fint& mod>
inline field_int<fint, mul_fint, mod> field_int<fint, mul_fint, mod>::operator+(const field_int<fint, mul_fint, mod>& i) const {
	return (val + i.val) % mod;
}


template <typename fint, typename mul_fint, const fint& mod>
inline field_int<fint, mul_fint, mod> field_int<fint, mul_fint, mod>::operator-(const field_int<fint, mul_fint, mod>& i) const {
	if (val > i.val) return val - i.val;
	return val + (mod - i.val);
}

template <typename fint, typename mul_fint, const fint& mod>
inline field_int<fint, mul_fint, mod> field_int<fint, mul_fint, mod>::operator*(const field_int<fint, mul_fint, mod>& i) const {
	return field_int((static_cast<mul_fint>(val) * i.val) % mod);
}


template <typename fint, typename mul_fint, const fint& mod>
inline field_int<fint, mul_fint, mod> field_int<fint, mul_fint, mod>::operator/(const field_int<fint, mul_fint, mod>& i) const {
	return field_int((static_cast<mul_fint>(val) * i.inverse().val) % mod);
}

template<typename fint, typename mul_fint, const fint& mod>
inline field_int<fint, mul_fint, mod> field_int<fint, mul_fint, mod>::operator&(const field_int<fint, mul_fint, mod>& i) const
{
	return field_int<fint, mul_fint, mod>(val & i.val);
}

template<typename fint, typename mul_fint, const fint& mod>
inline field_int<fint, mul_fint, mod> field_int<fint, mul_fint, mod>::operator&(int i) const
{
	return field_int<fint, mul_fint, mod>(val & i);
}


template <typename fint, typename mul_fint, const fint& mod>
inline field_int<fint, mul_fint, mod>& field_int<fint, mul_fint, mod>::operator+=(const field_int<fint, mul_fint, mod>& i) {
	val = (val + i.val) % mod;
	return *this;
}


template <typename fint, typename mul_fint, const fint& mod>
inline field_int<fint, mul_fint, mod>& field_int<fint, mul_fint, mod>::operator-=(const field_int<fint, mul_fint, mod>& i) {
	if (val < i.val) val = (val + (mod - i.val)) % mod;
	else val -= i.val;
	return *this;
}


template <typename fint, typename mul_fint, const fint& mod>
inline field_int<fint, mul_fint, mod>& field_int<fint, mul_fint, mod>::operator*=(const field_int<fint, mul_fint, mod>& i) {
	val = static_cast<fint>((static_cast<mul_fint>(val) * i.val) % mod);
	return *this;
}


template <typename fint, typename mul_fint, const fint& mod>
inline field_int<fint, mul_fint, mod>& field_int<fint, mul_fint, mod>::operator/=(const field_int<fint, mul_fint, mod>& i) {
	return *this *= i.inverse();
}

template <typename fint, typename mul_fint, const fint& mod>
inline field_int<fint, mul_fint, mod>& field_int<fint, mul_fint, mod>::operator%=(int i) {
	val %= i;
	return *this;
}

template<typename fint, typename mul_fint, const fint& mod>
inline field_int<fint, mul_fint, mod>& field_int<fint, mul_fint, mod>::operator<<=(int i)
{
	val = fint(mul_fint(val) << i);
	return *this;
}

template<typename fint, typename mul_fint, const fint& mod>
inline field_int<fint, mul_fint, mod>& field_int<fint, mul_fint, mod>::operator>>=(int i)
{
	val >>= i;
	return *this;
}


template <typename fint, typename mul_fint, const fint& mod>
inline field_int<fint, mul_fint, mod> field_int<fint, mul_fint, mod>::operator+(const fint i) const {
	return (val + i) % mod;
}


template <typename fint, typename mul_fint, const fint& mod>
inline field_int<fint, mul_fint, mod> field_int<fint, mul_fint, mod>::operator-(const fint i) const
{
	if (val < i) return (val + (mod - i)) % mod;
	return val - i;
}


template <typename fint, typename mul_fint, const fint& mod>
inline field_int<fint, mul_fint, mod> field_int<fint, mul_fint, mod>::operator-() const {
	return field_int(mod - val);
}


template <typename fint, typename mul_fint, const fint& mod>
inline field_int<fint, mul_fint, mod> field_int<fint, mul_fint, mod>::operator*(const fint i) const
{
	return field_int((static_cast<mul_fint>(val) * i) % mod);
}




template <typename fint, typename mul_fint, const fint& mod>
inline field_int<fint, mul_fint, mod> field_int<fint, mul_fint, mod>::operator/(const fint i) const {
	fint x, y;
	exgcd<fint, mul_fint, mod>(i, mod, x, y);
	return field_int((static_cast<mul_fint>(val) * x) % mod);
}

template<typename fint, typename mul_fint, const fint& mod>
inline field_int<fint, mul_fint, mod> field_int<fint, mul_fint, mod>::operator<<(int i) const
{
	return field_int<fint, mul_fint, mod>(mul_fint(val) << i);
}

template<typename fint, typename mul_fint, const fint& mod>
inline field_int<fint, mul_fint, mod> field_int<fint, mul_fint, mod>::operator>>(int i) const
{
	return field_int<fint, mul_fint, mod>(val >> i);
}


template<typename fint, typename mul_fint, const fint& mod>
inline field_int<fint, mul_fint, mod>::operator fint() const
{
	return val;
}

template <typename fint, typename mul_fint, const fint& mod>
inline field_int<fint, mul_fint, mod>::operator int() const {
	return static_cast<int>(val);
}

template <typename fint, typename mul_fint, const fint& mod>
inline bool field_int<fint, mul_fint, mod>::nonzero() const {
	return val != 0;
}

template<typename fint, typename mul_fint, const fint& mod>
inline bool field_int<fint, mul_fint, mod>::is_zero() const
{
	return val == fint(0);
}


template <typename fint, typename mul_fint, const fint& mod>
inline field_int<fint, mul_fint, mod> field_int<fint, mul_fint, mod>::inverse() const {
	assert(val != 0);
	fint x, y;
	exgcd<fint, mul_fint, mod>(val, mod, x, y);
	return x;
}

template <typename fint, typename mul_fint, const fint& mod>
inline field_int<fint, mul_fint, mod> field_int<fint, mul_fint, mod>::sqrt() const {
	assert(mod % 4 == 3);
	fint cnt((mod + 1) >> 2);
	field_int<fint, mul_fint, mod> base = val;
	field_int<fint, mul_fint, mod> ret = 1;
	while (cnt) {
		if (cnt % 2 == 1) ret *= base;
		base *= base;
		cnt >>= 1;
	}
	return ret;
}

template <typename fint, typename mul_fint, const fint& mod>
inline std::ostream& operator<<(std::ostream& os, const field_int<fint, mul_fint, mod>& f) {
	os << f.val;
	return os;
}

template class field_int<unsigned int, unsigned long long, mod2>;
template class field_int<uint128_t, uint256_t, mod>;

typedef field_int<unsigned int, unsigned long long, mod2> field_int_32;
typedef field_int<uint128_t, uint256_t, mod> field_int_128;


