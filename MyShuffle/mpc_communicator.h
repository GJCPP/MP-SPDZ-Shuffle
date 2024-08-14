#pragma once

#include <cstdlib>
#include <unistd.h>

#include <vector>
#include <set>

#include<emp-tool/emp-tool.h>
#include<emp-ot/emp-ot.h>

#include "global.h"
#include "field_int.h"
#include "block_wrapper.h"
#include "vectors.h"

class mpc_comm {
public:
    mpc_comm(int num);
    /*
    * Call fork() to create processes until the number reaches n.
    */
    bool init();

    void send(int who, const block_wrapper* data, int64_t length);
    void send(int who, const std::vector<block_wrapper>& data);
    void recv(int who, block_wrapper* data, int64_t length);

    template <typename T>
    void send(int who, const T& val);
    template <typename T>
    void recv(int who, T& val);
    template <typename T>
    void broadcast(int who, T& val);
    
    template <typename T>
    void send(int who, const std::vector<T>& val);
    template <typename T>
    void recv(int who, std::vector<T>& val);
    template <typename T>
    void broadcast(int who, std::vector<T>& val);
    
    template <typename T>
    void send(int who, const vectors<T>& val);
    template <typename T>
    void recv(int who, vectors<T>& val);
    template <typename T>
    void broadcast(int who, vectors<T>& val);

    void ot_send(int who, const block_wrapper* data0, const block_wrapper* data1, int64_t length);
    void ot_recv(int who, block_wrapper *data, const unsigned char *r, int64_t length);
    void ot_recv(int who, block_wrapper *data, const bool *r, int64_t length);

    int n, me;
protected:
    /*
    * Each unordered pair of partys (P1, P2) is associated with a port. (IP = 127.0.01)
    * Which is used for P1 P2 to send message to each other.
    */
    std::vector<std::vector<int>> port;
    std::vector<emp::NetIO *> sock;
};

template <typename T>
inline void mpc_comm::send(int who, const T &val)
{
    sock[who]->send_data(&val, sizeof(T));
#ifdef DEBUG
    sock[who]->flush();
#endif
}

template <typename T>
inline void mpc_comm::recv(int who, T &val)
{
    sock[who]->recv_data(&val, sizeof(T));
#ifdef DEBUG
    sock[who]->flush();
#endif
}

template <typename T>
inline void mpc_comm::broadcast(int who, T &val)
{
    if (who == me) {
        for (int i(1); i <= n; ++i) {
            if (i == me) continue;
            send(i, val);
        }
    } else {
        recv(who, val);
    }
}

template <typename T>
inline void mpc_comm::send(int who, const std::vector<T> &val)
{
    sock[who]->send_data(val.data(), sizeof(T) * val.size());
#ifdef DEBUG
    sock[who]->flush();
#endif
}

template <typename T>
inline void mpc_comm::recv(int who, std::vector<T> &val)
{
    sock[who]->recv_data(val.data(), sizeof(T) * val.size());
#ifdef DEBUG
    sock[who]->flush();
#endif
}

template <typename T>
inline void mpc_comm::broadcast(int who, std::vector<T> &val)
{
    if (who == me) {
        for (int i(1); i <= n; ++i) {
            if (i == me) continue;
            send(i, val);
        }
    } else {
        recv(who, val);
    }
}

template <typename T>
inline void mpc_comm::send(int who, const vectors<T> &val)
{
    sock[who]->send_data(val.data(), sizeof(T) * val.size());
#ifdef DEBUG
    sock[who]->flush();
#endif
}

template <typename T>
inline void mpc_comm::recv(int who, vectors<T> &val)
{
    sock[who]->recv_data(val.data(), sizeof(T) * val.size());
#ifdef DEBUG
    sock[who]->flush();
#endif
}

template <typename T>
inline void mpc_comm::broadcast(int who, vectors<T> &val)
{
    if (who == me) {
        for (int i(1); i <= n; ++i) {
            if (i == me) continue;
            send(i, val);
        }
    } else {
        recv(who, val);
    }
}
