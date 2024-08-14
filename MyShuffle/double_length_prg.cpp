#include "double_length_prg.h"

prg_seed random_seed() {
	prg_seed ret;
	static emp::PRG prg;
	prg.random_block(&ret);
	return ret;
}

std::array<prg_seed, 2> double_length_prg(const prg_seed& seed) {
	static emp::PRG prg;
	std::array<prg_seed, 2> ret;

	prg.reseed(&seed);
	prg.random_block(&ret[0], 2);
    return ret;
}

block_string arbitrary_prg(const prg_seed& seed, int length) {
	block_string ret;
	static emp::PRG prg;

	ret.resize(length);
	prg.reseed(&seed);
	prg.random_data(&ret[0], length);
	return ret;
}

block_string& operator^=(block_string& a, const block_string& b) {
	if (a.size() != b.size()) {
		std::cerr << ("XOR block_string with different length if forbiden") << std::endl;
		throw std::runtime_error("XOR block_string with different length if forbiden");
	}
	for (size_t i(0); i != a.size(); ++i) a[i] ^= b[i];
	return a;
}

block_string to_block_string(const prg_seed& s) {
	block_string ret;
	ret.resize(1);
	ret[0] = s;
	return ret;
}

prg_seed to_prg_seed(const block_string& s) {
	return s[0];
}

