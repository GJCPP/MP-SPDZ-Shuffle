#pragma once

#include "global.h"

#include "vectors.h"
#include "mpc_communicator.h"
#include "Benes_network.h"
#include "OPV.h"

namespace chase2020 {
	/*
	*	@inproceedings{chase2020secret,
			title={Secret-shared shuffle},
			author={Chase, Melissa and Ghosh, Esha and Poburinnaya, Oxana},
			booktitle={Advances in Cryptology--ASIACRYPT 2020: 26th International Conference on the Theory and Application of Cryptology and Information Security, Daejeon, South Korea, December 7--11, 2020, Proceedings, Part III 26},
			pages={342--372},
			year={2020},
			organization={Springer}
		}

		Only the last implementation, i.e. batch_sophisticated_permute, should be used,
			which is fully optimized.
	*/
	using namespace gjcShuffle;
	class shuffle_pair {
	public:
		int sender, permuter;
		int logsz, veclen, bat;
		std::vector<std::vector<block_wrapper>> a, b, delta; // a[vec_entry][pos]
		std::vector<int> perm;
	};

	shuffle_pair prepare_permute(gjcShuffle::mpc_comm& com, int sender, int permuter,
		const std::vector<int>& perm, int sz, int veclen, int batch);

	void permute(gjcShuffle::mpc_comm& com, vectors<block_wrapper>& val, shuffle_pair& plan);
}
