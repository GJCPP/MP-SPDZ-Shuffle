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
#include <libOTe/libOTe/Base/SimplestOT.h>
// #include "deps/libOTe/libOTe/TwoChooseOne/OTExtInterface.h"
// #include "deps/libOTe/libOTe/TwoChooseOne/IknpOtExtSender.h"
// #include "deps/libOTe/libOTe/TwoChooseOne/IknpOtExtReceiver.h"
#include <libOTe/libOTe/TwoChooseOne/KosOtExtSender.h>
#include <libOTe/libOTe/TwoChooseOne/KosOtExtReceiver.h>
#include "libOTe/TwoChooseOne/SoftSpokenOT/TwoOneMalicious.h"

#include "global.h"
#include "vectors.h"
#include "block_wrapper.h"
#include "my_ote.h"

namespace myShuffle {
    
    static const int default_port_base(9999);
    
    class shuffle_session;

    /*
        mpc_comm serves as communicator for parties.
        It supports primitives including
            0. basic send/recv
            1. sharing a secret
            2. openning a secret publicly or privately
            3. generate shared randomneess 
            3. multiply shared secrets
            4. two-party OTe
    */
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
        long long round_adjustment;
        
        std::vector<osuCrypto::Channel *> otSendChannel, otRecvChannel;

        bool online_phase = false;

        // Base OT family. Used by OTe.
        void recv_base_cor_ot(int sender, osuCrypto::BitVector choices, osuCrypto::span<osuCrypto::block> recv_key, osuCrypto::Channel *channel = nullptr);
        void send_base_cor_ot(int recver, osuCrypto::span<std::array<osuCrypto::block, 2>> send_key, osuCrypto::Channel *channel = nullptr);
        void send_base_cor_ot(int recver, osuCrypto::span<std::array<block_wrapper, 2>> send_key);
        void recv_base_cor_ot(int sender, osuCrypto::BitVector choices, osuCrypto::span<block_wrapper> recv_key);

        static constexpr size_t large_message_chunk_bytes = size_t(1) << 30;

        static size_t chunk_count(size_t size);
        void adjust_chunk_rounds(size_t messages);
        void send_chunked_payload(int party, const octet *data, size_t size, bool send_empty = true);
        void recv_chunked_payload(int party, octet *data, size_t size, bool recv_empty = true);

        template <typename T, typename Range>
        void send_range_chunked(int party, const Range& val);
        template <typename T, typename Range>
        void recv_range_chunked(int party, Range& val);
    public:
        mpc_comm(int n_party, int my_number, int port_base = default_port_base);
        
        void init(MascotFieldPrep<ShareType> *_prep, Input<ShareType> *_input, SPDZ<ShareType> *_protocol, MAC_Check_<ShareType> *_output);

        CryptoPlayer& get_P();
        ProtocolSetup<ShareType>& get_setup();

        int get_port(int party = -1) const; // get the port of party
        int get_my_number() const; // get the id of this party
        int get_n_party() const; // get the number of parties

        // Generate random resources (locally).
        void rand_bytes(octet *dest, size_t size);
        void rand_blocks(block_wrapper *dest, size_t num);
        ClearType rand_int();
        void rand_int(ClearType &dest);
        void rand_int(std::vector<ClearType> &dest);
        void rand_int(vectors<ClearType> &dest);

        /*
            Take inputs from parties.
            Following an MP-SPDZ manner, a canonical excution should be
                1. init
                2. append(who to input, the input value)
                3. exchange; communication happens at this step
                4. consume; assign the input (from buffer) to variables
            Note: you can append arbitrarily before exchange.
        */
        void input_init();
        void input_append(int party, const ClearType& val); // If me != party, val is not used.
        void input_append_all(const ClearType& val); // Each party inputs one value
        void input_append(int party, const vectors<ClearType>& val); // if me != party, val is not used
        void input_append_all(const vectors<ClearType>& val); // Each party inputs same amount of values
        void input_exchange(); // Communicate to share values
        void input_consume(int party, ShareType& val); // Fetch one input for specific party
        void input_consume(int party, vectors<ShareType>& val);
        ShareType input_consume(int party);
        
        /*
            Prepare Beaver's triple for multiplications.
            prepare_more_mul_now should be called at the end of offline phase.
        */
        void prepare_more_mul(size_t num);
        void prepare_more_mul_lazy(size_t num);
        void prepare_more_mul_now(size_t num = 0);

        /*
            Compute multiplications.
            Fetch Beaver's triple from triple_resource. Generate more triples first upon deficiency.
            Following an MP-SPDZ manner, a canonical execution should be:
                1. init
                2. append pairs of shared secret
                3. exchange, which requires communication
                4. consume, which fetches the result of multiplication (from buffer)
            Note: you can append arbitrarily before exchange
        */
        void mul_init();
        void mul_append(const ShareType& v1, const ShareType& v2);
        void mul_append(const vectors<ShareType>& v1, const vectors<ShareType>& v2);
        void mul_exchange();
        void mul_consume(ShareType& val);
        void mul_consume(vectors<ShareType>& val);
        ShareType mul_consume();


        /*
            Reveal a shared secret publicly.
            Following an MP-SPDZ manner, a canonical execution should be:
                1. init
                2. append
                3. exchange, which requires communication
                4. consume, which fetches reconstructed secret (from buffer)
        */
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
        ClearType output_consume();
        void output_check(); // Checks the integrity of all values opened by far.

        /*
            Prepare shared randomness used for input.
        */
        void prepare_more_random_lazy(size_t num);
        void prepare_more_random_now(size_t num = 0);
        ShareType get_random();

        void prepare_output_mask(size_t expand);


        /*
            Reveal a shared secret to a specified party. 
        */
        void prepare_more_private_output_lazy(int party, size_t num);
        void prepare_more_private_output_now(size_t num = 0);
        void private_output_init();
        void private_output_append(int party, const ShareType& val);
        void private_output_append(int party, const vectors<ShareType>& val);
        void private_output_exchange();
        void private_output_consume(int party, ClearType& val);
        void private_output_consume(int party, vectors<ClearType>& val);
        ClearType private_output_consume(int party);

        /*
            Plaintext send and recv
        */
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
        void broadcast(int party, T& val); // Broadcast with consistency check
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


        /*
            OTe family.

        */
        void send_ext_cor_ot(int recver, osuCrypto::span<std::array<block_wrapper, 2>> send_key);
        void recv_ext_cor_ot(int sender, osuCrypto::BitVector choices, osuCrypto::span<block_wrapper> recv_key);

        void ext_ot_send(int recver, osuCrypto::span<std::array<block_wrapper, 2>> sendMsg);
        void ext_ot_send(int recver, osuCrypto::span<block_wrapper> msg0, osuCrypto::span<block_wrapper> msg1);
        void ext_ot_recv(int sender, osuCrypto::BitVector choices, osuCrypto::span<block_wrapper> recvMsg);

        /**
         * This function is used to commit a message and open it.
         * Each party commits its own content, and after all commitments are received,
         * open all messages and check.
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

        /*
            MAC check for the shuffle protocol by Song et al.
        */
        void mac_check(const vectors<ShareType>& valMac);

        /*
            Count the total communication of THIS SINGLE party.
        */
        size_t count_total_comm() const;
        size_t count_raw_total_rounds() const;
        size_t count_total_rounds() const;
        void add_round_adjustment(long long adjustment);
        void reset_total_comm();

        /*
            Set the phase.
            In online phase, upon deficiency of random resources, warnings will be generated.
            There is no other difference for offline/online phases.
        */
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
        send_range_chunked<T>(party, val);
    }

    template <typename T>
    inline void mpc_comm::recv(int party, vectors<T> &val)
    {
        recv_range_chunked<T>(party, val);
    }

    template <typename T>
    inline void mpc_comm::send(int party, const std::vector<T> &val)
    {
        send_range_chunked<T>(party, val);
    }

    template <typename T>
    inline void mpc_comm::recv(int party, std::vector<T> &val)
    {
        recv_range_chunked<T>(party, val);
    }

    template <typename T>
    inline void mpc_comm::send(int party, const osuCrypto::span<T> val)
    {
        send_range_chunked<T>(party, val);
    }

    template <typename T>
    inline void mpc_comm::recv(int party, osuCrypto::span<T> val)
    {
        recv_range_chunked<T>(party, val);
    }

    template <typename T, typename Range>
    inline void mpc_comm::send_range_chunked(int party, const Range& val)
    {
        const size_t max_items = std::max<size_t>(1, large_message_chunk_bytes / std::max<size_t>(sizeof(T), 1));
        size_t remaining = val.size();
        size_t messages = remaining == 0 ? 1 : (remaining + max_items - 1) / max_items;
        auto it = val.begin();

        if (remaining == 0) {
            octetStream o;
            P.send_to(party, o);
            return;
        }

        while (remaining > 0) {
            const size_t take = std::min(max_items, remaining);
            octetStream o;
            for (size_t i = 0; i < take; ++i, ++it) {
                o.store(*it);
            }
            P.send_to(party, o);
            remaining -= take;
        }
        adjust_chunk_rounds(messages);
    }

    template <typename T, typename Range>
    inline void mpc_comm::recv_range_chunked(int party, Range& val)
    {
        const size_t max_items = std::max<size_t>(1, large_message_chunk_bytes / std::max<size_t>(sizeof(T), 1));
        size_t remaining = val.size();
        size_t messages = remaining == 0 ? 1 : (remaining + max_items - 1) / max_items;
        auto it = val.begin();

        if (remaining == 0) {
            octetStream o;
            P.receive_player(party, o);
            return;
        }

        while (remaining > 0) {
            const size_t take = std::min(max_items, remaining);
            octetStream o;
            P.receive_player(party, o);
            for (size_t i = 0; i < take; ++i, ++it) {
                o.get(*it);
            }
            remaining -= take;
        }
        adjust_chunk_rounds(messages);
    }
}
#endif
