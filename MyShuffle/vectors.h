#pragma once

#include <iostream>
#include <vector>

// #include "emp-tool/emp-tool.h"

/*
	This is the implementation of 2-dim array, i.e. matrix.
	The addition is defined entry-wise.
*/

template <typename mpc_int>
class vectors;

template <typename mpc_int>
std::ostream& operator<<(std::ostream& os, const vectors<mpc_int>& v);

template <typename mpc_int>
class vectors {
public:
	friend std::ostream& operator<<<>(std::ostream& os, const vectors<mpc_int>& v);
	vectors(size_t num, size_t len);
	vectors(const std::vector<mpc_int>& v) : num(v.size()), len(1), vec(v) {}
	vectors() = default;
	vectors(const vectors<mpc_int>& other) = default;

	typename std::vector<mpc_int>::iterator begin();
	typename std::vector<mpc_int>::const_iterator begin() const;
	typename std::vector<mpc_int>::iterator end();
	typename std::vector<mpc_int>::const_iterator end() const;

	operator mpc_int* ();
	operator std::vector<mpc_int>();
	operator std::vector<mpc_int>() const;

	vectors<mpc_int>& operator=(const vectors<mpc_int>& other);
	vectors<mpc_int> operator+(const vectors<mpc_int>& other) const;
	vectors<mpc_int> operator-(const vectors<mpc_int>& other) const;
	vectors<mpc_int>& operator+=(const vectors<mpc_int>& other);
	vectors<mpc_int>& operator-=(const vectors<mpc_int>& other);

	void init(size_t num, size_t len, mpc_int val=0);
	mpc_int* operator[](size_t pos); // pick one row vector
	const mpc_int* operator[](size_t pos) const; // pick one row vector
	mpc_int& at(size_t pos);
	const mpc_int& at(size_t pos) const;
	mpc_int& back(size_t line);

	void resize(int newnum, int newlen);
	void push_back(const mpc_int* val);

	void clear();

	bool nonempty() const;
	bool empty() const;


	// void rand_fill();


	bool operator!=(const vectors<mpc_int>& other) const;

	bool operator<(const vectors<mpc_int>& other) const {
		if (num != other.num) {
			return num < other.num;
		}
		if (len != other.len) {
			return len < other.len;
		}
		for (size_t i(0); i != num * len; ++i) {
			if (vec[i] != other.vec[i]) {
				return vec[i] < other.vec[i];
			}
		}
		return false;
	}

	bool operator==(const vectors<mpc_int>& other) const {
		if (num != other.num || len != other.len) {
			return false;
		}
		for (size_t i(0); i != num * len; ++i) {
			if (vec[i] != other.vec[i]) {
				return false;
			}
		}
		return true;
	}

	bool operator>(const vectors<mpc_int>& other) const {
		if (num != other.num) {
			return num > other.num;
		}
		if (len != other.len) {
			return len > other.len;
		}
		for (size_t i(0); i != num * len; ++i) {
			if (vec[i] != other.vec[i]) {
				return vec[i] > other.vec[i];
			}
		}
		return false;
	}

	static vectors<mpc_int> cat(const vectors<mpc_int>& a, const vectors<mpc_int>& b) {
		if (a.len != b.len) {
			std::cerr << "vectors::cat : shape mismatch, " << a.num << "x" << a.len << " vs " << b.num << "x" << b.len << std::endl;
			throw std::runtime_error("vectors::cat : shape mismatch.");
		}
		vectors<mpc_int> res(a.num + b.num, a.len);
		for (size_t i(0); i != a.num; ++i) {
			for (size_t j(0); j != a.len; ++j) {
				res[i][j] = a[i][j];
			}
		}
		for (size_t i(0); i != b.num; ++i) {
			for (size_t j(0); j != b.len; ++j) {
				res[i + a.num][j] = b[i][j];
			}
		}
		return res;
	}

	void split(size_t pos, vectors<mpc_int>& vec1, vectors<mpc_int>& vec2) {
		if (pos >= num) {
			std::cerr << "vectors::split : shape mismatch, " << pos << " >= " << num << std::endl;
			throw std::runtime_error("vectors::split : shape mismatch.");
		}
		vec1.resize(pos, len);
		vec2.resize(num - pos, len);
		for (size_t i(0); i != pos; ++i) {
			for (size_t j(0); j != len; ++j) {
				vec1[i][j] = vec[i * len + j];
			}
		}
		for (size_t i(0); i != num - pos; ++i) {
			for (size_t j(0); j != len; ++j) {
				vec2[i][j] = vec[(i + pos) * len + j];
			}
		}
	}

	const mpc_int * data() const;
	mpc_int* data();

	size_t size() const;

	size_t num, len; // num of rows, len of each row
protected:
	std::vector<mpc_int> vec;
};

template <typename mpc_int>
vectors<mpc_int>::vectors(size_t _num, size_t _len)
	: num(_num), len(_len)
{
	vec.resize(num * len);
}

template <typename mpc_int>
inline typename std::vector<mpc_int>::iterator vectors<mpc_int>::begin()
{
    return vec.begin();
}
template <typename mpc_int>
inline typename std::vector<mpc_int>::const_iterator vectors<mpc_int>::begin() const
{
    return vec.begin();
}

template <typename mpc_int>
inline typename std::vector<mpc_int>::iterator vectors<mpc_int>::end()
{
    return vec.end();
}
template <typename mpc_int>
inline typename std::vector<mpc_int>::const_iterator vectors<mpc_int>::end() const
{
    return vec.end();
}

template <typename mpc_int>
vectors<mpc_int>::operator mpc_int *()
{
    return &vec[0];
}

template<typename mpc_int>
inline vectors<mpc_int>::operator std::vector<mpc_int>() {
	return vec;
}

template<typename mpc_int>
inline vectors<mpc_int>::operator std::vector<mpc_int>() const
{
	return vec;
}

template<typename mpc_int>
inline vectors<mpc_int>& vectors<mpc_int>::operator=(const vectors<mpc_int>& other)
{
	num = other.num;
	len = other.len;
	vec = other.vec;
	return *this;
}

template <typename mpc_int>
inline vectors<mpc_int> vectors<mpc_int>::operator+(const vectors<mpc_int> &other) const
{
	if (num != other.num || len != other.len) {
		std::cerr << "vectors::operator+ : shape mismatch,"
			<< num << "x" << len << " vs " << other.num << "x" << other.len << std::endl;
		throw std::runtime_error("vectors::operator+ : shape mismatch.");
	}
	vectors<mpc_int> res(num, len);
	size_t sz = size();
	for (size_t i(0); i != sz; ++i) {
		res.vec[i] = vec[i] + other.vec[i];
	}
	return res;
}

template <typename mpc_int>
inline vectors<mpc_int> vectors<mpc_int>::operator-(const vectors<mpc_int> &other) const
{
	if (num != other.num || len != other.len) {
		std::cerr << "vectors::operator+ : shape mismatch,"
			<< num << "x" << len << " vs " << other.num << "x" << other.len << std::endl;
		throw std::runtime_error("vectors::operator+ : shape mismatch.");
	}
	vectors<mpc_int> res(num, len);
	size_t sz = size();
	for (size_t i(0); i != sz; ++i) {
		res.vec[i] = vec[i] - other.vec[i];
	}
	return res;
}

template<typename mpc_int>
inline vectors<mpc_int>& vectors<mpc_int>::operator+=(const vectors<mpc_int>& other)
{
	if (num != other.num || len != other.len) {
		std::cerr << "vectors::operator+= : shape inconsistent." << std::endl;
		std::cerr << "One is " << num << "x" << len << ", another is " << other.num << "x" << other.len << "." << std::endl;
		throw std::runtime_error("vectors::operator+= : shape inconsistent.");
	}
	size_t sz = size();
	for (size_t i(0); i != sz; ++i) {
		vec[i] += other.vec[i];
	}
	return *this;
}

template<typename mpc_int>
inline vectors<mpc_int>& vectors<mpc_int>::operator-=(const vectors<mpc_int>& other)
{
	if (num != other.num || len != other.len) {
		std::cerr << "vectors::operator+= : shape inconsistent." << std::endl;
		throw std::runtime_error("vectors::operator+= : shape inconsistent.");
	}
	size_t sz = size();
	for (size_t i(0); i != sz; ++i) {
		vec[i] -= other.vec[i];
	}
	return *this;
}

template <typename mpc_int>
void vectors<mpc_int>::init(size_t _num, size_t _len, mpc_int val) {
	num = _num, len = _len;
	vec = std::vector<mpc_int>(num * len, val);
}

template <typename mpc_int>
mpc_int* vectors<mpc_int>::operator[](size_t pos) {
	if (pos >= num) {
		std::cout << "vectors : Subscript of selection_vectors out of range, " << pos << " >= " << num << std::endl;
		throw std::runtime_error("Subscript of selection_vectors out of range.");
	}
	return  reinterpret_cast<mpc_int *>(&vec[pos * len]);
}

template <typename mpc_int>
const mpc_int* vectors<mpc_int>::operator[](size_t pos) const {
	if (pos >= num) {
		std::cout << "vectors : Subscript of selection_vectors out of range, " << pos << " >= " << num << std::endl;
		throw std::runtime_error("Subscript of selection_vectors out of range.");
	}
	return reinterpret_cast<const mpc_int *>(&vec[pos * len]);
}

template <typename mpc_int>
mpc_int& vectors<mpc_int>::at(size_t pos) {
	return vec.at(pos);
}

template <typename mpc_int>
const mpc_int& vectors<mpc_int>::at(size_t pos) const {
	return vec.at(pos);
}

template<typename mpc_int>
inline mpc_int& vectors<mpc_int>::back(size_t line)
{
	return vec[(line + 1) * len - 1];
}

template<typename mpc_int>
inline void vectors<mpc_int>::resize(int newnum, int newlen)
{
	num = newnum;
	len = newlen;
	vec.resize(len * num);
}

/*
* Be careful with this method! Be sure that you know what len is!
*/
template<typename mpc_int>
inline void vectors<mpc_int>::push_back(const mpc_int* val) {
	if (len == 0) {
		std::cerr << "vectors::push_back: len == 0." << std::endl;
		throw "vectors::push_back: len == 0.";
	}
	for (int i(0); i != len; ++i) {
		vec.push_back(val[i]);
	}
	++num;
}

template<typename mpc_int>
inline void vectors<mpc_int>::clear()
{
	num = 0;
	vec.clear();
}

template<typename mpc_int>
inline bool vectors<mpc_int>::nonempty() const
{
	return !vec.empty();
}

template<typename mpc_int>
inline bool vectors<mpc_int>::empty() const
{
	return vec.empty();
}

// template<typename mpc_int>
// inline void vectors<mpc_int>::rand_fill()
// {
// 	emp::PRG prg;
// 	size_t sz = size();
// 	for (size_t i(0); i != sz; ++i) {
// 		prg.random_data(vec[i], sizeof(mpc_int));
// 	}
// }

template<typename mpc_int>
inline bool vectors<mpc_int>::operator!=(const vectors<mpc_int>& other) const
{
	if (num != other.num) return true;
	if (len != other.len) return true;
	for (size_t i(0); i != num * len; ++i) {
		if (vec[i] != other.vec[i]) return true;
	}
	return false;
}

template <typename mpc_int>
inline const mpc_int *vectors<mpc_int>::data() const
{
    return vec.data();
}

template <typename mpc_int>
inline mpc_int *vectors<mpc_int>::data()
{
	return vec.data();
}

template<typename mpc_int>
inline size_t vectors<mpc_int>::size() const
{
	return num * len;
}
template <typename mpc_int>
std::ostream& operator<<(std::ostream& os, const vectors<mpc_int>& v) {
	os << "Vectors " << &v << " {" << std::endl;
	for (int i(0); i != v.num; ++i) {
		os << "\t";
		for (int j(0); j != v.len; ++j) {
			os << v[i][j] << " ";

        }
        os << std::endl;
    }
	os << "}" << std::endl;
	return os;
}
