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

namespace myShuffle{
	/*
		The opv of length 2^n.

		A canonical usage should be like:

			1. sender_append_OPV/receiver_append_OPV
				Which translates the target OPV to OT sessions.
				You can arbitrarily run xxx_append_OPV at this step.
				
			2. perform OTe and record the OT messages
				The OT messages should include PRG seeds and hash_val

			3. opv_2n(...)
				Sequentially calling opv_2n(...) to reconstruct opvs from OT messages.
	
		Caution: this implementation ALONE is not secure against active adversaries,
				in the sense that selective faliure attack may be carried out.
		See also Song_shuffle.h/cpp for the usage of this opv.
	
	*/
	class opv_2n {
	public:
		opv_2n() = default;
		
		/*
			Reconstruct all entries of opv from OT messages.
		*/
		opv_2n(int n, int pos, const prg_seed ot_msg[], const block_wrapper hash_val[]);

		/*
			Reconstruct one opv from OT messages.
			Pointer ot_msg and next_hash_val will be moved to the first unused OT messages.
		*/
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
}


#endif // !OVP_H
