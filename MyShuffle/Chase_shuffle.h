#pragma once

#include "global.h"

#include "vectors.h"
#include "mpc_communicator.h"
#include "Benes_network.h"
#include "OPV.h"

namespace chase2020 {
	/*
		This is the implementation for the following paper:

			@inproceedings{chase2020secret,
				title={Secret-shared shuffle},
				author={Chase, Melissa and Ghosh, Esha and Poburinnaya, Oxana},
				booktitle={Advances in Cryptology--ASIACRYPT 2020: 26th International Conference on the Theory and Application of Cryptology and Information Security, Daejeon, South Korea, December 7--11, 2020, Proceedings, Part III 26},
				pages={342--372},
				year={2020},
				organization={Springer}
			}

		Warning: the implementation is slightly different from the paper. It is still semi-honest secure though.
				 
		Also, this implementation is neither used by Song_shuffle.cpp nor my_shuffle.cpp.



		A personal note:
			Originally I wish to implement the malicious secure version of this protocol, then add in the several enhancements by Song et al. Ideally, I want to:
				
				1. implement enhanced Chase et al. permutation

				2. build Song et al. shuffle, using Chase et al. permutation as sub-routine


			
			However, this turns out to be difficult.
			
			The optimization (or enhancement) requires batching several shuffle sessions together; it breaks shuffle sessions into permutation sessions (of possibly different sizes), and then put the permutation sessions with same size together, fuel them, randomly permute them (as a challenge/obfuscation against adversary), and then put them back to form shuffle sessions.
			
			Well, this process is surely not very black-box, and a standalone Chase et al. permutation does not seem to help.

			So this implementation of Chase et al. permutation is only a half-way towards malicious security, in that it features some of the enhancements proposed by Song et al., but not all of.
	*/
	using namespace myShuffle;
	
	/*
		A shuffle_pair is used for a one-sided permutation, i.e. the permuter knows the permutation.
		A complete two-party shuffle protocol consumes two shuffle_pair.
	*/
	class shuffle_pair {
	public:
		int sender, permuter;
		int logsz, veclen, bat;
		std::vector<std::vector<block_wrapper>> a, b, delta; // a[vec_entry][pos]
		std::vector<int> perm;
	};

	/*
		Prepare shuffle_pair for permutation.
	*/
	shuffle_pair prepare_permute(myShuffle::mpc_comm& com, int sender, int permuter,
		const std::vector<int>& perm, int sz, int veclen, int batch);

	/*
		A complete two-party shuffle protocol consists of two calls to permute.
	*/
	void permute(myShuffle::mpc_comm& com, vectors<block_wrapper>& val, shuffle_pair& plan);
}
