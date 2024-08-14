#include "vectors.h"

#include "mpc_communicator.h"

/*
    Caution: the functions provided in this file are not secure, only for testing purpose.
*/

template <typename T>
void peek_sum(mpc_comm& com, std::string name, const vectors<T> &val)
{
    vectors<T> tem_val = val, recv_val = val;
    if (com.me == 1) {
        for (int sender(1); sender <= com.n; ++sender) {
            if (sender == com.me) continue;
            com.recv(sender, recv_val);
            tem_val += recv_val;
        }
    } else {
        com.send(1, val);
    }
    if (com.me == 1) {
        std::cout << "Peek sum of " << name << " : " << std::endl;
        for (int i(0); i != val.num; ++i) {
            for (int j(0); j != val.len; ++j) {
                std::cout << tem_val[i][j] << " ";
            }
            std::cout << std::endl;
        } std::cout << std::endl;
    }
}


void insecure_share(mpc_comm& com, int owner, vectors<block_wrapper>& val);

void insecure_recon(mpc_comm& com, int towho, vectors<block_wrapper>& val);
