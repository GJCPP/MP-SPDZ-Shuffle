#include "mpc_gadget.h"

void insecure_share(mpc_comm &com, int owner, vectors<block_wrapper> &val)
{
    if (com.me == owner) {
        for (int i(1); i <= com.n; ++i) {
            if (i == com.me) continue;
            vectors<block_wrapper> rand_val(val.num, val.len);
            emp::PRG prg;
            prg.random_block(reinterpret_cast<emp::block *>(rand_val.data()), val.num * val.len);
            com.send(i, rand_val);
            val -= rand_val;
        }
    } else {
        com.recv(owner, val);
    }
}

void insecure_recon(mpc_comm &com, int towho, vectors<block_wrapper> &val)
{
    if (com.me == towho) {
        for (int i(1); i <= com.n; ++i) {
            if (i == com.me) continue;
            vectors<block_wrapper> rand_val(val.num, val.len);
            com.recv(i, rand_val);
            val += rand_val;
        }
    } else {
        com.send(towho, val);
    }
}
