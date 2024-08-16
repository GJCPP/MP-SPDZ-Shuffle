#pragma once

#ifndef MPC_COMMUNICATOR_H
#define MPC_COMMUNICATOR_H

#include <span>

#include <cryptoTools/Network/Channel.h>
#include <cryptoTools/Network/Session.h>
#include <cryptoTools/Network/IOService.h>
#include <cryptoTools/Common/BitVector.h>

#include "Math/gfp.hpp"
#include "Machines/SPDZ.hpp"
#include "Protocols/ProtocolSet.h"
#include "deps/libOTe/libOTe/Base/SimplestOT.h"
// #include "deps/libOTe/libOTe/TwoChooseOne/OTExtInterface.h"
#include "deps/libOTe/libOTe/TwoChooseOne/IknpOtExtSender.h"
#include "deps/libOTe/libOTe/TwoChooseOne/IknpOtExtReceiver.h"

#include "global.h"
#include "vectors.h"
#include "block_wrapper.h"


class mpc_comm {
protected:
    int n_party, my_number;
    std::vector<octetStream> out_buff, in_buff;
    std::vector<osuCrypto::Session> sessions;

    osuCrypto::IOService ios;

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

    template <typename T>
    void send(int party, const osuCrypto::span<T> val);
    template <typename T>
    void recv(int party, osuCrypto::span<T> val);

    void send(int recver, const void *data, size_t size);
    void recv(int sender, void *data, size_t size);

    // void send_base_ot(int recver, osuCrypto::span<std::array<osuCrypto::block,2 >> send_msg, osuCrypto::Channel channel = {});
    void recv_base_ot(int sender, osuCrypto::BitVector choices, osuCrypto::span<osuCrypto::block> recv_msg, osuCrypto::Channel *channel = nullptr);
    void send_base_ot(int recver, osuCrypto::span<std::array<osuCrypto::block,2 >> send_msg, osuCrypto::Channel *channel = nullptr);
    void send_base_ot(int recver, osuCrypto::span<std::array<block_wrapper,2 >> send_msg);
    void recv_base_ot(int sender, osuCrypto::BitVector choices, osuCrypto::span<block_wrapper> recv_msg);

    void send_ext_ot(int recver, osuCrypto::span<std::array<block_wrapper,2 >> send_msg);
    void recv_ext_ot(int sender, osuCrypto::BitVector choices, osuCrypto::span<block_wrapper> recv_msg);

    ~mpc_comm(void);
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

template <typename T>
inline void mpc_comm::send(int party, const osuCrypto::span<T> val)
{
    octetStream o;
    for (const T& v : val) {
        o.store(v);
    }
    P.send_to(party, o);
}

template <typename T>
inline void mpc_comm::recv(int party, osuCrypto::span<T> val)
{
    octetStream o;
    P.receive_player(party, o);
    for (T& v : val) {
        o.get(v);
    }
}

#endif
