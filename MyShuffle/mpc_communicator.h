#pragma once

#ifndef MPC_COMMUNICATOR_H
#define MPC_COMMUNICATOR_H

#include <span>
#include <random>

#include <cryptoTools/Network/Channel.h>
#include <cryptoTools/Network/Session.h>
#include <cryptoTools/Network/IOService.h>
#include <cryptoTools/Common/BitVector.h>

#include "Math/gfp.hpp"
#include "Machines/SPDZ.hpp"
#include "Protocols/ProtocolSet.h"
#include "deps/libOTe/libOTe/Base/SimplestOT.h"
// #include "deps/libOTe/libOTe/TwoChooseOne/OTExtInterface.h"
// #include "deps/libOTe/libOTe/TwoChooseOne/IknpOtExtSender.h"
// #include "deps/libOTe/libOTe/TwoChooseOne/IknpOtExtReceiver.h"
#include "deps/libOTe/libOTe/TwoChooseOne/KosOtExtSender.h"
#include "deps/libOTe/libOTe/TwoChooseOne/KosOtExtReceiver.h"
#include "libOTe/TwoChooseOne/SoftSpokenOT/TwoOneMalicious.h"

#include "global.h"
#include "vectors.h"
#include "block_wrapper.h"

namespace gjcShuffle {
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

        osuCrypto::PRNG prng;
    public:
        mpc_comm(int n_party, int my_number);
        
        void init(Input<ShareType> *input, SPDZ<ShareType> *protocol, MAC_Check_<ShareType> *output);

        CryptoPlayer& get_P();
        ProtocolSetup<ShareType>& get_setup();
        int get_port(int party = -1) const;
        int get_my_number() const;
        int get_n_party() const;

        void rand_bytes(octet *dest, size_t size);
        void rand_blocks(block_wrapper *dest, size_t num);
        
        template <typename T>
        void send(int party, T val);
        template <typename T>
        void recv(int party, T& val);
        template <typename T>
        void unchecked_broadcast(int party, T& val);

        template <typename T>
        void send(int party, const vectors<T>& val);
        template <typename T>
        void recv(int party, vectors<T>& val);
        template <typename T>
        void send(int party, const std::vector<T>& val);
        template <typename T>
        void recv(int party, std::vector<T>& val);

        template <typename T>
        void send(int party, const osuCrypto::span<T> val);
        template <typename T>
        void recv(int party, osuCrypto::span<T> val);

        void send(int recver, const void *data, size_t size);
        void recv(int sender, void *data, size_t size);

        // void base_ot_send(int recver, osuCrypto::span<std::array<osuCrypto::block,2 >> send_msg, osuCrypto::Channel channel = {});
        void recv_base_cor_ot(int sender, osuCrypto::BitVector choices, osuCrypto::span<osuCrypto::block> recv_key, osuCrypto::Channel *channel = nullptr);
        void send_base_cor_ot(int recver, osuCrypto::span<std::array<osuCrypto::block, 2>> send_key, osuCrypto::Channel *channel = nullptr);
        void send_base_cor_ot(int recver, osuCrypto::span<std::array<block_wrapper, 2>> send_key);
        void recv_base_cor_ot(int sender, osuCrypto::BitVector choices, osuCrypto::span<block_wrapper> recv_key);

        void send_ext_cor_ot(int recver, osuCrypto::span<std::array<block_wrapper, 2>> send_key);
        void recv_ext_cor_ot(int sender, osuCrypto::BitVector choices, osuCrypto::span<block_wrapper> recv_key);

        void ext_ot_send(int recver, osuCrypto::span<std::array<block_wrapper, 2>> sendMsg);
        void ext_ot_send(int recver, osuCrypto::span<block_wrapper> msg0, osuCrypto::span<block_wrapper> msg1);
        void ext_ot_recv(int sender, osuCrypto::BitVector choices, osuCrypto::span<block_wrapper> recvMsg);

        ~mpc_comm(void);
    };

    template <typename T>
    inline void mpc_comm::send(int party, T val)
    {
        octetStream o;
        o.store(val);
        P.send_to(party, o);
    }
    template <>
    inline void mpc_comm::send<bool>(int party, bool val)
    {
        octetStream o;
        o.store_bit(val);
        P.send_to(party, o);
    }

    template <typename T>
    inline void mpc_comm::recv(int party, T &val)
    {
        octetStream o;
        P.receive_player(party, o);
        o.get(val);
    }

    template <>
    inline void mpc_comm::recv(int party, bool &val)
    {
        octetStream o;
        P.receive_player(party, o);
        val = o.get_bit();
    }


    template <typename T>
    inline void mpc_comm::unchecked_broadcast(int party, T &val)
    {
        std::vector<octetStream> buff(n_party);
        if (party == my_number) buff[party].store(val);
        // buff[party].store_bytes(const_cast<octet *>(reinterpret_cast<const octet *>(&val)), sizeof(T));
        P.unchecked_broadcast(buff);
        buff[party].get(val);
    }
    
    template <>
    inline void mpc_comm::unchecked_broadcast<bool>(int party, bool &val)
    {
        int tmp = val;
        unchecked_broadcast(party, tmp);
        val = tmp;
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
    inline void mpc_comm::send(int party, const std::vector<T> &val)
    {
        octetStream o;
        for (const T& v : val) {
            o.store(v);
        }
        P.send_to(party, o);
    }

    template <typename T>
    inline void mpc_comm::recv(int party, std::vector<T> &val)
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
}
#endif
