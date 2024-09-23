#pragma once

#include "vectors.h"

#include "mpc_communicator.h"


namespace gjcShuffle
{
    /*
        Caution: the functions provided in this file are not secure, only for testing purpose.
    */

    template <typename T>
    void peek_sum(mpc_comm& com, std::string name, const vectors<T> &val)
    {
        int me = com.get_my_number(), n = com.get_n_party();
        vectors<T> tem_val = val, recv_val = val;
        if (me == 1) {
            for (int sender(0); sender != n; ++sender) {
                if (sender == me) continue;
                com.recv(sender, recv_val);
                tem_val += recv_val;
            }
        } else {
            com.send(1, val);
        }
        if (me == 1) {
            std::cout << "Peek sum of " << name << " : " << std::endl;
            for (int i(0); i != val.num; ++i) {
                for (int j(0); j != val.len; ++j) {
                    std::cout << tem_val[i][j] << " ";
                }
                std::cout << std::endl;
            } std::cout << std::endl;
        }
    }

    /*
    * Works for statically allocated type.
    */
    template <typename T>
    T insecure_static_sum(mpc_comm& com, const T& val) {
        std::vector<T> vals(com.get_n_party());
        vals[com.get_my_number()] = val;
        T ret = {};
        for (int i(0); i != com.get_n_party(); ++i) {
            octetStream os;
            os.serialize(val);
            com.unchecked_broadcast(i, os);
            os.unserialize(vals[i]);
            ret += vals[i];
        }
        return ret;
    }


    void insecure_share(mpc_comm& com, int owner, vectors<block_wrapper>& val);
    void insecure_share(mpc_comm& com, int owner, vectors<ClearType>& val);

    void insecure_recon(mpc_comm& com, int towho, vectors<block_wrapper>& val);
    void insecure_recon(mpc_comm& com, int towho, vectors<ClearType>& val);
}