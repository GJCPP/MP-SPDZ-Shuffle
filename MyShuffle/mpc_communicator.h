#pragma once

#ifndef MPC_COMMUNICATOR_H
#define MPC_COMMUNICATOR_H

#include "Math/gfp.hpp"
#include "Machines/SPDZ.hpp"
#include "Protocols/ProtocolSet.h"

#include "global.h"
#include "vectors.h"


class mpc_comm {
protected:
    int n_party, my_number;
    std::vector<octetStream> out_buff, in_buff;

    Names N;
    CryptoPlayer P;
    ProtocolSetup<ShareType> setup;

    Input<ShareType> *input;
    SPDZ<ShareType> *protocol;
    MAC_Check_<ShareType> *output;
public:
    mpc_comm(int n_party, int my_number);
    
    void init(Input<ShareType> *input, SPDZ<ShareType> *protocol, MAC_Check_<ShareType> *output);

    CryptoPlayer& get_P();
    ProtocolSetup<ShareType>& get_setup();
    int get_port(int party = -1);
    
    template <typename T>
    void send(int party, T val);
    template <typename T>
    void recv(int party, T& val);

    template <typename T>
    void send(int party, const vectors<T>& val);
    template <typename T>
    void recv(int party, vectors<T>& val);
};

template <typename T>
void mpc_comm::send(int party, T val)
{
    octetStream o;
    o.store(val);
    P.send_to(party, o);
}

template <typename T>
void mpc_comm::recv(int party, T &val)
{
    octetStream o;
    P.receive_player(party, o);
    o.get(val);
}

template <typename T>
inline void mpc_comm::send(int party, const vectors<T> &val)
{
    octetStream o;
    for (const T& v : val) {
        o.store(v);
    }
    P.send_to(party, o);
}

template <typename T>
inline void mpc_comm::recv(int party, vectors<T> &val)
{
    octetStream o;
    P.receive_player(party, o);
    for (T& v : val) {
        o.get(v);
    }
}

#endif
