/*
    This file implements the secure shuffle protocol designed in the paper:
        @article{song2023secret,
        title={Secret-Shared Shuffle with Malicious Security},
        author={Song, Xiangfu and Yin, Dong and Bai, Jianli and Dong, Changyu and Chang, Ee-Chien},
        journal={Cryptology ePrint Archive},
        year={2023}
        }
*/

#pragma once
#include <map>

#include "Tools/Hash.h"

#include "global.h"

#include "permutation.h"
#include "math_gadget.h"
#include "vectors.h"
#include "Benes_network.h"
#include "permutation.h"

#include "OPV.h"
#include "mpc_communicator.h"
#include "mpc_gadget.h"

#define MAX_BATCH_SIZE 32

namespace song2023 {
    using gjcShuffle::mpc_comm;

    #define DEFAULT_BUCKET_SIZE 30
    extern const int bucket_size[7][17];

    int get_bucket_size(int logsz, int log_batch_sz);

    class permute_pair {
    public:
        permute_pair() = default;
        permute_pair(const permutation& perm, const vectors<ClearType>& a, const vectors<ClearType>& b, const vectors<ClearType>& delta);
        permute_pair(const permutation& perm, const vectors<prg_seed>& opvm);

        void expand(size_t veclen, bool ovlivious);

        permutation perm;
        vectors<ClearType> a, b, delta;
        vectors<prg_seed> opvm;
    };

    class permute_info {
    public:
        permute_info() = default;
        permute_info(int permuter, int logsz);

        int permuter;
        int logsz;

        bool operator<(const permute_info& info) const;
    };

    /*
        A permute session specifies all information/resource of/for a permute protocol, including
            - The permuter, who knows the permutation performed.
            - The size of vectors to be permuted, i.e. number of vectors and length of each.
            - The batch size of the permute protocol. (Hence the parties could use Benes network to reconstruct each small permutation.)
            - The permutation itself, known only by the permuter.
            - The permute_pair, n-1 for permuter, 1 for each sender.
        All parties are involved in the permute protocol, each (except permuter) acts as a sender by turn.
    */
    class permute_session {

            friend void book_permute_session(mpc_comm& com, permute_session* session);
            
            friend permute_session *book_permute_session(mpc_comm& com, int permuter, int logsz, size_t veclen, int batch, const permutation& perm);

            friend void decompose_permute_sessions(mpc_comm& com);

            friend void process_all_orders(mpc_comm& com);
    protected:
        int permuter;
        int logsz;
        size_t veclen;
        int batch;
        permutation perm;
        bool initialized = false;
        bool destroyed = false;
        std::map<int, int> bucket_size = {};

        /*
            If the party is not the permuter, only pairs[permuter] is non-empty.
            Otherwise, pairs[sender] is the permute_pair for (permuter, sender).
        */
        std::vector<permute_pair> pairs[MAXN][MAX_BATCH_SIZE];

        void destroy();
    public:
        permute_session() = default;

        template <typename T>
        void init(int _permuter, int _logsz, size_t _veclen, int _batch, const permutation& _perm) {
            permuter = _permuter;
            logsz = _logsz;
            veclen = _veclen;
            batch = _batch;
            perm = _perm;
            for (int i = 0; i < MAXN; ++i) {
                for (int j = 0; j < MAX_BATCH_SIZE; ++j) {
                    pairs[i][j].clear();
                }
            }
            initialized = false;
            destroyed = false;
            bucket_size = {};
            if (typeid(T) == typeid(ClearType)) {
                ;
            } else if (typeid(T) == typeid(ShareType)) {
                veclen <<= 1;
            } else {
                std::cerr << "permute_session::book : Unknown type." << std::endl;
                throw std::runtime_error("permute_session::book : Unknown type.");
            }
        }

        // Perform the magic of permute protocol!
        void perform(mpc_comm& com, vectors<ClearType>& val);
        void perform(mpc_comm& com, vectors<ShareType>& val);

        const permutation& get_perm() const;

        void clear();
    };

    class shuffle_session {
        friend void book_shuffle_session(mpc_comm &com, shuffle_session *session);
    protected:
        int n_party, logsz;
        size_t veclen;
        int batch;

        std::vector<permute_session> permute_sessions;

        bool destroyed = false;

        void destroy();
    public:
        shuffle_session() = default;

        // Perform the magic of shuffle protocol!
        template <typename T>
        void perform(mpc_comm& com, vectors<T>& val) {
            for (int i = 0; i < n_party; ++i) {
                permute_sessions[i].perform(com, val);
            }
            destroy();
        }

        template <typename T>
        void init(int _n_party, int _logsz, size_t _veclen, int _batch, const permutation& perm) {
            n_party = _n_party;
            logsz = _logsz;
            veclen = _veclen;
            batch = _batch;
            permute_sessions.resize(n_party);
            for (int i = 0; i < n_party; ++i) {
                permute_sessions[i].init<T>(i, logsz, veclen, batch, perm);
            }
            destroyed = false;
        }

        const permutation& get_perm(int who) const;
    };

    /*
        This class stores the information of a permute order.
        In specific, the permutation required to perform and the session it belongs to.
        For a sender in the session, perm is empty.
    */
    class order_info {
    public:
        order_info() = default;
        order_info(const permutation& perm, permute_session* session);

        permutation perm; // the permutation required to perform.
        permute_session *session; // which session this order belongs to.
    };

    /*
        A permute session requires resource (i.e. permute_pair).
        A big permutation is separated into smaller ones.
        All smaller one with same (permuter, logsz, veclen) is stored in booked_permute, and is to be processed in one batch.
        When orders are processed, resource will be allocate to each session (specified by pointer).
    */
    extern std::map<permute_info, std::vector<order_info>> booked_permute;
    extern std::map<int, size_t> count_permute_task;

    // Book resource for a permute session.
    void book_permute_session(mpc_comm& com, permute_session* session);
    // Book resource for a shuffle session.
    void book_shuffle_session(mpc_comm& com, shuffle_session* session);

    template <typename T>
    permute_session *book_permute_session(mpc_comm& com, int permuter, int logsz, size_t veclen, int batch, const permutation& perm) {
        permute_session *new_session = new permute_session();
        new_session->init<T>(permuter, logsz, veclen, batch, perm);
        book_permute_session(com, new_session);
        return new_session;
    }
    template <typename T>
    shuffle_session *book_shuffle_session(mpc_comm& com, int logsz, size_t veclen, int batch, const permutation& perm) {
        shuffle_session *new_session = new shuffle_session();
        new_session->init<T>(com.get_n_party(), logsz, veclen, batch, perm);
        book_shuffle_session(com, new_session);
        return new_session;
    }


    /*
        This function processes all orders in booked_permute.
    */
    void process_all_orders(mpc_comm& com);
}
