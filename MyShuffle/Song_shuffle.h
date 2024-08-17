#pragma once
#include <map>

#include "Tools/Hash.h"

#include "global.h"

#include "math_gadget.h"
#include "vectors.h"
#include "Benes_network.h"
#include "permutation.h"

#include "OPV.h"
#include "mpc_communicator.h"
#include "mpc_gadget.h"

#define MAX_BATCH_SIZE 32

namespace song2023 {
    using namespace gjcShuffle;

    class permute_pair {
    public:
        permute_pair() = default;
        permute_pair(const permutation& perm, const vectors<ClearType>& a, const vectors<ClearType>& b, const vectors<ClearType>& delta);

        permutation perm;
        vectors<ClearType> a, b, delta;
    };

    class permute_info {
    public:
        permute_info() = default;
        permute_info(int permuter, int logsz, int veclen);

        int permuter;
        int logsz, veclen;

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
            
            friend permute_session *book_permute_session(mpc_comm& com, int permuter, int logsz, int veclen, int batch, const permutation& perm);

            friend void process_all_orders(mpc_comm& com);
    protected:
        int permuter;
        int logsz;
        size_t veclen;
        int batch;
        permutation perm;

        /*
            If the party is not the permuter, only pairs[permuter] is non-empty.
            Otherwise, pairs[sender] is the permute_pair for (permuter, sender).
        */
        std::vector<permute_pair> pairs[MAXN][MAX_BATCH_SIZE];
    public:
        permute_session() = default;

        template <typename T>
        void book(int _permuter, int _logsz, int _veclen, int _batch, const permutation& _perm) {
            permuter = _permuter;
            logsz = _logsz;
            veclen = _veclen;
            batch = _batch;
            perm = _perm;
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

    // Book resource for a permute session.
    void book_permute_session(mpc_comm& com, permute_session* session);

    template <typename T>
    permute_session *book_permute_session(mpc_comm& com, int permuter, int logsz, int veclen, int batch, const permutation& perm) {
        permute_session *new_session = new permute_session();
        new_session->book<T>(permuter, logsz, veclen, batch, perm);
        book_permute_session(com, new_session);
        return new_session;
    }

    /*
        This function processes all orders in booked_permute.
    */
    void process_all_orders(mpc_comm& com);
}
