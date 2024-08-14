#include "mpc_communicator.h"

mpc_comm::mpc_comm(int num)
    : n(num), me(1), port(num + 1, std::vector<int>(num + 1)), sock(num + 1, nullptr)
{
    int init_port(43124);
    for (int i(1); i <= n; ++i) {
        for (int j(i + 1); j <= n; ++j) {
            port[i][j] = init_port++;
        }
    }
}

bool mpc_comm::init()
{
    int current(1);
    me = 1;
    while (current != n) {
        int pid = fork();
        if (pid == -1) {
            return false;
        }
        if (pid == 0) break;
        else me = ++current;
    }
    for (int i(1); i < me; ++i) {
        sock[i] = new emp::NetIO(nullptr, port[i][me], true);
    }
    for (int j(me + 1); j <= n; ++j) {
        sock[j] = new emp::NetIO("127.0.0.1", port[me][j], true);
    }
    return true;
}

void mpc_comm::send(int who, const block_wrapper * data, int64_t length)
{
    sock[who]->send_block(reinterpret_cast<const emp::block *>(data), length);
#ifdef DEBUG
    sock[who]->flush();
#endif
}

void mpc_comm::recv(int who, block_wrapper *data, int64_t length)
{
    sock[who]->recv_block(reinterpret_cast<emp::block *>(data), length);
}

void mpc_comm::send(int who, const std::vector<block_wrapper> &data)
{
    sock[who]->send_block(reinterpret_cast<const emp::block *>(data.data()), data.size());
#ifdef DEBUG
    sock[who]->flush();
#endif
}

void mpc_comm::ot_send(int who, const block_wrapper *data0, const block_wrapper *data1, int64_t length)
{
    emp::IKNP<emp::NetIO> np(sock[who], true);
    np.send(reinterpret_cast<const emp::block *>(data0), reinterpret_cast<const emp::block *>(data1), length);
#ifdef DEBUG
    sock[who]->flush();
#endif
}

void mpc_comm::ot_recv(int who, block_wrapper *data, const unsigned char *r, int64_t length)
{
    ot_recv(who, data, reinterpret_cast<const bool *>(r), length);
}

void mpc_comm::ot_recv(int who, block_wrapper *data, const bool *r, int64_t length)
{
    emp::IKNP<emp::NetIO> np(sock[who], true);
    np.recv(reinterpret_cast<emp::block *>(data), r, length);
}
