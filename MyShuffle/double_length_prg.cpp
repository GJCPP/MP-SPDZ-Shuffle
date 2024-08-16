#include "double_length_prg.h"

prg_seed random_seed() {
	prg_seed ret;
	PRNG prg;
	prg.InitSeed();
	prg.get_octets(ret.data, SEED_SIZE);
	return ret;
}

PRNG prg_with_seed(const prg_seed& seed) {
	PRNG prg;
	prg.SetSeed(reinterpret_cast<const octet *>(&seed));
	return prg;
}

std::array<prg_seed, 2> double_length_prg(const prg_seed& seed) {
	PRNG prg = prg_with_seed(seed);
	std::array<prg_seed, 2> ret;

	prg.get_octets(ret[0].data, SEED_SIZE);
	prg.get_octets(ret[1].data, SEED_SIZE);
    return ret;
}

block_string arbitrary_prg(const prg_seed& seed, int length) {
	block_string ret;
	PRNG prg = prg_with_seed(seed);

	ret.resize(length);
	prg.get_octets(reinterpret_cast<octet *>(&ret[0]), length * SEED_SIZE);
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

