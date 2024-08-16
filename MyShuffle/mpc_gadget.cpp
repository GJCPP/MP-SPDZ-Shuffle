#include "mpc_gadget.h"

namespace gjcShuffle {
    void insecure_share(mpc_comm &com, int owner, vectors<block_wrapper> &val)
    {
        int me = com.get_my_number(), n = com.get_n_party();
        if (me == owner) {
            for (int i(0); i != n; ++i) {
                if (i == me) continue;
                vectors<block_wrapper> rand_val(val.num, val.len);
                com.rand_blocks(rand_val.data(), rand_val.size());
                com.send(i, rand_val);
                val -= rand_val;
            }
        } else {
            com.recv(owner, val);
        }
    }

    void insecure_recon(mpc_comm &com, int towho, vectors<block_wrapper> &val)
    {
        int me = com.get_my_number(), n = com.get_n_party();
        if (me == towho) {
            for (int i(0); i != n; ++i) {
                if (i == me) continue;
                vectors<block_wrapper> rand_val(val.num, val.len);
                com.recv(i, rand_val);
                val += rand_val;
            }
        } else {
            com.send(towho, val);
        }
    }

}