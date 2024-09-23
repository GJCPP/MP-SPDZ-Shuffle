#pragma once

#ifndef MPC_COMMUNICATOR_H
#define MPC_COMMUNICATOR_H

#include <random>
#include <vector>
#include <algorithm>

#include <cryptoTools/Network/Channel.h>
#include <cryptoTools/Network/Session.h>
#include <cryptoTools/Network/IOService.h>
#include <cryptoTools/Common/BitVector.h>
#include <Tools/Commit.h>

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
#include "my_ote.h"

namespace gjcShuffle {
    static const int default_port_base(9999);
    class shuffle_session;
    class mpc_comm {
    protected:
        friend class shuffle_session;

        int n_party, my_number;
        std::vector<osuCrypto::Session> sessions;

        osuCrypto::IOService ios;

        Names N;
        CryptoPlayer P;
        ProtocolSetup<ShareType> setup;

        MascotFieldPrep<ShareType> *prep;
        Input<ShareType> *input;
        SPDZ<ShareType> *protocol;
        MAC_Check_<ShareType> *output;

        osuCrypto::PRNG osuPrg;
        ::PRNG prg;

        std::vector<std::deque<ShareType>> shared_mask; // for private output.
        std::deque<ClearType> clear_mask;
        std::deque<ShareType> random_resource;
        std::deque<std::array<ShareType, 3>> triple_resource;

        size_t expand_random_size, expand_triple_size;
        std::vector<size_t> cnt_private_output;
        
        std::vector<osuCrypto::Channel *> otSendChannel, otRecvChannel;

        bool online_phase = false;

        // void base_ot_send(int recver, osuCrypto::span<std::array<osuCrypto::block,2 >> send_msg, osuCrypto::Channel channel = {});
        void recv_base_cor_ot(int sender, osuCrypto::BitVector choices, osuCrypto::span<osuCrypto::block> recv_key, osuCrypto::Channel *channel = nullptr);
        void send_base_cor_ot(int recver, osuCrypto::span<std::array<osuCrypto::block, 2>> send_key, osuCrypto::Channel *channel = nullptr);
        void send_base_cor_ot(int recver, osuCrypto::span<std::array<block_wrapper, 2>> send_key);
        void recv_base_cor_ot(int sender, osuCrypto::BitVector choices, osuCrypto::span<block_wrapper> recv_key);
    public:
        ShareType alpha;
        mpc_comm(int n_party, int my_number, int port_base = default_port_base);
        
        void init(MascotFieldPrep<ShareType> *prep, Input<ShareType> *input, SPDZ<ShareType> *protocol, MAC_Check_<ShareType> *output);

        void stop();

        CryptoPlayer& get_P();
        ProtocolSetup<ShareType>& get_setup();
        int get_port(int party = -1) const;
        int get_my_number() const;
        int get_n_party() const;

        void rand_bytes(octet *dest, size_t size);
        void rand_blocks(block_wrapper *dest, size_t num);
        ClearType rand_int();
        void rand_int(ClearType &dest);
        void rand_int(std::vector<ClearType> &dest);
        void rand_int(vectors<ClearType> &dest);


        void input_init();
        void input_append(int party, const ClearType& val);
        void input_append_all(const ClearType& val);
        void input_append(int party, const vectors<ClearType>& val);
        void input_append_all(const vectors<ClearType>& val);
        void input_exchange();
        void input_consume(int party, ShareType& val);
        void input_consume(int party, vectors<ShareType>& val);
        ShareType input_consume(int party);
        
        void prepare_more_mul(size_t num); // Prepare more triples.
        void prepare_more_mul_lazy(size_t num);
        void prepare_more_mul_now(size_t num = 0);

        void mul_init();
        void mul_append(const ShareType& v1, const ShareType& v2);
        void mul_append(const vectors<ShareType>& v1, const vectors<ShareType>& v2);
        void mul_exchange();
        void mul_consume(ShareType& val);
        void mul_consume(vectors<ShareType>& val);
        ShareType mul_consume();

        void output_immediately(const ShareType& val, ClearType& res);
        void output_immediately(const vectors<ShareType>& val, vectors<ClearType>& res);
        void output_immediately(const std::vector<ShareType>& val, std::vector<ClearType>& res);
        void output_init();
        void output_append(const ShareType& val);
        void output_append(const vectors<ShareType>& val);
        void output_append(const std::vector<ShareType>& val);
        void output_exchange();
        void output_consume(ClearType& val);
        void output_consume(vectors<ClearType>& val);
        void output_consume(std::vector<ClearType>& val);
        void output_check();
        ClearType output_consume();

        void prepare_more_random_lazy(size_t num);
        void prepare_more_random_now(size_t num = 0);
        ShareType get_random();

        void prepare_output_mask(size_t expand);

        void prepare_more_private_output_lazy(int party, size_t num);
        void prepare_more_private_output_now(size_t num = 0);
        void private_output_init();
        void private_output_append(int party, const ShareType& val);
        void private_output_append(int party, const vectors<ShareType>& val);
        void private_output_exchange();
        void private_output_consume(int party, ClearType& val);
        void private_output_consume(int party, vectors<ClearType>& val);
        ClearType private_output_consume(int party);


        template <typename T>
        void send(int party, T val);
        template <typename T>
        void recv(int party, T& val);
        template <typename T>
        void unchecked_broadcast(int party, T& val);
        //void unchecked_broadcast(int party, octet *val, size_t len);
        template <typename T>
        void unchecked_broadcast(int party, vectors<T> &val);
        template <typename T>
        void broadcast(int party, T& val);
        template <typename T>
        void broadcast(int party, vectors<T>& val);

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

        void send(int party, octetStream &os);
        void recv(int party, octetStream &os);

        void send_ext_cor_ot(int recver, osuCrypto::span<std::array<block_wrapper, 2>> send_key);
        void recv_ext_cor_ot(int sender, osuCrypto::BitVector choices, osuCrypto::span<block_wrapper> recv_key);

        void ext_ot_send(int recver, osuCrypto::span<std::array<block_wrapper, 2>> sendMsg);
        void ext_ot_send(int recver, osuCrypto::span<block_wrapper> msg0, osuCrypto::span<block_wrapper> msg1);
        void ext_ot_recv(int sender, osuCrypto::BitVector choices, osuCrypto::span<block_wrapper> recvMsg);

        /*
        *   This function is used to commit a message and open it.
        *   Each party commits its own content, and after all commitments are received,
        *       open all messages and check.
        */
        template <typename T>
        void commit_and_open(const T& message, std::vector<T>& open_msg) {
            AllCommitments commitment(P);
            octetStream os;
            os.store(message);
            commitment.commit(os);
            commitment.open(os);
            open_msg.resize(P.num_players());
            for (int i = 0; i < P.num_players(); i++) {
                commitment.messages[i].get(open_msg[i]);
            }
            commitment.check(my_number, os);
        }

        void mac_check(const vectors<ShareType>& valMac);

        size_t count_total_comm() const;
        void reset_total_comm();

        void set_online();
        void set_offline();

        ~mpc_comm();
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


    template<typename T>
    inline void mpc_comm::unchecked_broadcast(int party, vectors<T> & val)
    {
        std::vector<octetStream> buff(n_party);
        if (party == my_number) {
            for (const T& v : val)
                buff[party].store(v);
        }
        // buff[party].store_bytes(const_cast<octet *>(reinterpret_cast<const octet *>(&val)), sizeof(T));
        P.unchecked_broadcast(buff);
        for (T& v : val)
            buff[party].get(v);
    }
    
    template <>
    inline void mpc_comm::unchecked_broadcast<bool>(int party, bool &val)
    {
        int tmp = val;
        unchecked_broadcast(party, tmp);
        val = tmp;
    }

    template <>
    inline void mpc_comm::unchecked_broadcast(int party, octetStream &val)
    {
        std::vector<octetStream> buff(n_party);
        if (party == my_number) buff[party] = val;
        // buff[party].store_bytes(const_cast<octet *>(reinterpret_cast<const octet *>(&val)), sizeof(T));
        P.unchecked_broadcast(buff);
        val = buff[party];
    }


    template <typename T>
    inline void mpc_comm::broadcast(int party, T &val)
    {
        std::vector<octetStream> buff(n_party);
        if (party == my_number) buff[party].store(val);
        // buff[party].store_bytes(const_cast<octet *>(reinterpret_cast<const octet *>(&val)), sizeof(T));
        P.Broadcast_Receive(buff);
        buff[party].get(val);
    }

    template<typename T>
    inline void mpc_comm::broadcast(int party, vectors<T> & val)
    {
        std::vector<octetStream> buff(n_party);
        if (party == my_number) {
            for (const T& v : val)
                buff[party].store(v);
        }
        // buff[party].store_bytes(const_cast<octet *>(reinterpret_cast<const octet *>(&val)), sizeof(T));
        P.Broadcast_Receive(buff);
        for (T& v : val)
            buff[party].get(v);
    }
    
    template <>
    inline void mpc_comm::broadcast<bool>(int party, bool &val)
    {
        int tmp = val;
        broadcast(party, tmp);
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
