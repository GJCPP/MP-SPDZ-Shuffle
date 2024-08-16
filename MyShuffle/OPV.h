#pragma once
#ifndef OVP_H
#define OVP_H
#include <vector>
#include <queue>
#include <stack>

#include "Tools/Hash.h"
#include "cryptoTools/Common/BitVector.h"


#include "global.h"
#include "double_length_prg.h"

namespace gjcShuffle{
	class oblivious_punctured_vector {
	public:
		int pos; // The punctured position.
		std::vector<prg_seed> data;
	};

	/*
	*	the opv of length 2^n
	*	Caution: this implementation is not secure against active adversaries,
	*			in the sense that selective faliure attack may be carried out.
	*	This is fixed in Song::shuffle by cut-and-choose technique.
	*	C.f. "Secret-Shared Shuffle with Malicious Security", https://eprint.iacr.org/2023/1794
	*	See also Song_shuffle.h/cpp for the implementation of shuffle using this opv.
	*/
	class opv_2n {
	public:
		opv_2n() = default;
		/*
		* Reconstruct all entries of opv from OT messages.
		*/
		opv_2n(int n, int pos, const prg_seed ot_msg[], const block_wrapper hash_val[]);

		opv_2n(int n, int pos, prg_seed*& ot_msg, block_wrapper*& next_hash_val);

		const prg_seed& operator[](size_t pos) const;

		/*
		* Randomly construct a 2^n opv, return its message.
		*/
		std::array<std::vector<prg_seed>, 3> construct(int n);

		size_t pos; // The punctured position.
		std::vector<prg_seed> data;
	};

	void sender_append_OPV(std::vector<prg_seed>& msg0, std::vector<prg_seed>& msg1,
		std::vector<block_wrapper>& check_val,
		int logsz, std::vector<opv_2n>& opvs);

    void receiver_append_OPV(osuCrypto::BitVector &choose, std::vector<block_wrapper> &hash_val, int logsz, int pos);
    /*
     * Sender is the one who knows all entries.
     */

    //template <typename mpc_int>
	//oblivious_punctured_vector fetch_opv(mpc_comm<mpc_int>& com, int sender, int receiver, int length, int pos);
}


#endif // !OVP_H
