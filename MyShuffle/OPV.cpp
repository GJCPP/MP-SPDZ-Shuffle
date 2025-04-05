#include "OPV.h"


namespace myShuffle {
	opv_2n::opv_2n(int n, int _pos, const prg_seed ot_msg[], const block_wrapper hash_val[])
		: pos(_pos)
	{
		int tree_pos(pos << 1), tree_n(n + 1);
		std::vector<std::vector<prg_seed>> layers(tree_n + 1);
		for (int i(1); i <= tree_n; ++i) layers[i].resize(1 << i);
		for (int i(1); i <= tree_n; ++i) {
			int ob_pos = (tree_pos >> (tree_n - i));
			int even((ob_pos & 1) ? 0 : 1); // Choose to not know the even one.
			prg_seed newseed = ot_msg[i - 1];
			for (int j(0); j != (1 << i); j += 2) {
				newseed ^= layers[i][j + even];
			}
			layers[i][ob_pos ^ 1] = newseed;
			if (i != tree_n) {
				for (int j(0); j != (1 << i); ++j) {
					if (j == ob_pos) continue;
					auto expand = double_length_prg(layers[i][j]);
					layers[i + 1][j << 1] = expand[0];
					layers[i + 1][j << 1 | 1] = expand[1];
				}
			}
		}
		block_wrapper comp_hash_val[BLOCKS_FOR_HASH];
		Hash hash;
		for (int i(0); i != (1 << n); ++i) {
			hash.update(&layers[tree_n][i << 1 | 1], sizeof(block_wrapper));
		}
		hash.final(reinterpret_cast<octet *>(comp_hash_val));
		for (int i(0); i != BLOCKS_FOR_HASH; ++i) {
			if ((comp_hash_val[i] - hash_val[i]).is_nonzero()) {
				std::cerr << "opv_2n: wrong hash value." << std::endl;
				throw std::runtime_error("opv_2n: wrong hash value.");
			}
		}
		data.resize(1 << n);
		for (int i(0); i != (1 << n); ++i) {
			data[i] = layers[tree_n][i << 1];
		}
		if (data[pos].is_nonzero()) {
			std::cerr << "opv_2n::opv_2n Implementation error." << std::endl;
			throw "opv_2n::opv_2n Implementation error.";
		}
	}

	opv_2n::opv_2n(int n, int _pos, prg_seed*& next_ot_msg, block_wrapper*& next_hash_val)
		: pos(_pos)
	{
		int tree_pos(pos << 1), tree_n(n + 1);
		std::vector<std::vector<prg_seed>> layers(tree_n + 1);
		for (int i(1); i <= tree_n; ++i) layers[i].resize(1 << i);
		for (int i(1); i <= tree_n; ++i) {
			int ob_pos = (tree_pos >> (tree_n - i));
			int even((ob_pos & 1) ? 0 : 1); // Choose to not know the even one.
			prg_seed newseed = *next_ot_msg++;
			for (int j(0); j != (1 << i); j += 2) {
				newseed ^= layers[i][j + even];
			}
			layers[i][ob_pos ^ 1] = newseed;
			if (i != tree_n) {
				for (int j(0); j != (1 << i); ++j) {
					if (j == ob_pos) continue;
					auto expand = double_length_prg(layers[i][j]);
					layers[i + 1][j << 1] = expand[0];
					layers[i + 1][j << 1 | 1] = expand[1];
				}
			}
		}
		block_wrapper comp_hash_val[BLOCKS_FOR_HASH];
		Hash hash;
		for (int i(0); i != (1 << n); ++i) {
			hash.update(&layers[tree_n][i << 1 | 1], sizeof(block_wrapper));
		}
		hash.final(reinterpret_cast<octet *>(comp_hash_val));
		for (int i(0); i != BLOCKS_FOR_HASH; ++i) {
			if ((comp_hash_val[i] - *next_hash_val++).is_nonzero()) {
				std::cerr << "opv_2n: wrong hash value." << std::endl;
				throw std::runtime_error("opv_2n: wrong hash value.");
			}
		}
		data.resize(1 << n);
		for (int i(0); i != (1 << n); ++i) {
			data[i] = layers[tree_n][i << 1];
		}
		if (data[pos].is_nonzero()) {
			std::cerr << "opv_2n::opv_2n Implementation error." << std::endl;
			throw "opv_2n::opv_2n Implementation error.";
		}
	}

	const prg_seed& opv_2n::operator[](size_t pos) const
	{
	#ifdef DEBUG
		if (pos > data.size()) {
			std::cerr << "opv_2n::operator[] : index out of range, " << pos << "/" << data.size() << std::endl;
			throw std::runtime_error("opv_2n : index out of range.");
		}
	#endif
		return data[pos];
	}

	std::array<std::vector<prg_seed>, 3> opv_2n::construct(int n)
	{
		int tree_n(n + 1);
		std::vector<std::vector<prg_seed>> layers(tree_n + 1);
		for (int i(1); i <= tree_n; ++i) layers[i].resize(1 << i);
		layers[0].push_back(random_seed());
		std::array<std::vector<prg_seed>, 3> ret;
		for (int i(1); i <= tree_n; ++i) {
			prg_seed s0 = {}, s1 = {};
			for (size_t j(0); j < layers[i].size(); j += 2) {
				auto expand = double_length_prg(layers[i - 1][j >> 1]);
				layers[i][j] = expand[0];
				layers[i][j + 1] = expand[1];
				s0 ^= layers[i][j];
				s1 ^= layers[i][j + 1];
			}
			ret[0].push_back(s0);
			ret[1].push_back(s1);
		}
		data.resize(1 << n);
		for (int i(0); i != (1 << n); ++i) {
			data[i] = layers[tree_n][i << 1];
		}

		
		// Compute the hash of right leaves.
		Hash hash;
		ret[2].resize(BLOCKS_FOR_HASH);
		for (int i(0); i != (1 << n); ++i) {
			hash.update(&layers[tree_n][i << 1 | 1], sizeof(block_wrapper));
		}
		hash.final(reinterpret_cast<octet *>(ret[2].data()));

		return ret;
	}

	void sender_append_OPV(std::vector<prg_seed>& msg0, std::vector<prg_seed>& msg1, std::vector<block_wrapper>& check_val,
		int logsz, std::vector<opv_2n>& opvs)
	{
		opv_2n opv;
		auto msg = opv.construct(logsz);
		for (size_t i(0); i != msg[0].size(); ++i) {
			msg0.push_back(msg[0][i]);
			msg1.push_back(msg[1][i]);
		}
		for (const block_wrapper block : msg[2]) {
			check_val.push_back(block);
		}

		opvs.push_back(opv);
	}

	void receiver_append_OPV(osuCrypto::BitVector& choose, std::vector<block_wrapper>& hash_val, int logsz, int pos)
	{
		int tree_pos = pos << 1;
		for (int i(logsz); i >= 0; --i) {
			choose.pushBack(1 ^ ((tree_pos >> i) & 1));
		}
		for (int i(0); i != BLOCKS_FOR_HASH; ++i) hash_val.push_back({});
	}
}