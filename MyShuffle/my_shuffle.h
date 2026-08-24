#pragma once

#include "global.h"

#include "permutation.h"
#include "vectors.h"

#include "mpc_communicator.h"
#include "Song_shuffle.h"

/*
    This is the implementation of current paper.

    A canonical execution includes

        Offline:
            1. plan = book_shuffle_session(...)
                This returns a pointer to necessary (unallocated) resources for performing shuffle
                The "resource" is random correlation.
            2. process_all_orders
                This fuels all resources for booked shuffle session.

        Online:
            1. plan->perform(...)
                This consumes the correlation and performs the shuffle.
                A plan should be used only once.

    Note: in namespace song2023, we also have book_shuffle_session, which differs from myShuffle::book_shuffle_session.
*/

namespace myShuffle {
    
    class shuffle_session;

    /*
        Shuffle correlation, which is generated in offline.
        Used to achieve linear online communication.
        See the paper for detailed explanation of each entry.
    */
    class shuffle_cor {
        friend shuffle_session;
    protected:
        bool initialized = false;
    public:
        shuffle_cor() = default;

        permutation perm; // The (secret) permutation chosen by the party.
        ShareType beta; // MAC key
        std::vector<vectors<ShareType>> r, beta_r, rp;
        std::vector<vectors<ShareType>> permuted_r, permuted_beta_r, permuted_rp;
        vectors<ClearType> z[2]; // If the party is not zero party, z is non-empty.
    };

    /*
        Parameters for the shuffle.
        Booked shuffle sessions is recorded by
            std::map<shuffle_info, std::vector<order_info>> booked_shuffle

        Hence, sessions with same shuffle_info will be batched, whose resources
            will be generated within same OTe session, for saving base OT.
    */
    class shuffle_info {
    public:
        shuffle_info() = default;
        shuffle_info(int logsz, size_t veclen);

        int logsz;
        size_t veclen;

        bool operator<(const shuffle_info& info) const;
    };

    class shuffle_session {
        friend void book_shuffle_session(mpc_comm &com, shuffle_session *session);

        friend void process_all_orders(mpc_comm& com);

        friend void malloc_random_resource(mpc_comm& com);
        friend void fill_in_random_resource(mpc_comm& com);
        friend void compute_beta_r(mpc_comm& com);
        friend void compute_permuted_random_resource(mpc_comm& com);
        friend void compute_z(mpc_comm& com);
        friend void set_init_flag();
        friend void clear_unused();

    protected:
        int n_party, logsz;
        size_t veclen;
        int batch;
        permutation perm;

        shuffle_cor cor;
        std::vector<song2023::permute_session> permute_sessions;

        // If this flag is set, this session cannot be used anymore, and should be delted or init again.
        bool destroyed = false;

        void set_init_flag();
        void destroy();
    public:
        shuffle_session() = default;

        template <typename T>
        void init(int _n_party, int _logsz, size_t _veclen, int _batch, const permutation& _perm = {}) {
            n_party = _n_party;
            logsz = _logsz;
            veclen = _veclen; // permute r, beta*r, rp
            batch = _batch;
            if (_perm.n != 0) {
                perm = _perm;
            } else {
                perm = permutation(1 << logsz, true);
            }

            destroyed = false;
            permute_sessions.resize(n_party);

            // initialize null shuffle correlation
            cor.r.resize(n_party);
            cor.beta_r.resize(n_party);
            cor.rp.resize(n_party);
            cor.permuted_r.resize(n_party);
            cor.permuted_beta_r.resize(n_party);
            cor.permuted_rp.resize(n_party);
            cor.perm = perm;

            for (int i = 0; i < n_party; ++i) {
                // three times veclen: permuting (r, beta r, rp)
                permute_sessions[i].init<T>(i, logsz, 3 * veclen, batch, cor.perm);
            }
        }

        // Perform shuffle protocol
        void perform(mpc_comm& com, vectors<ShareType>& val,
                bool strong_abort_privacy = false);

        const permutation& get_perm() const;
    };

    class order_info {
    public:
        order_info() = default;
        order_info(shuffle_session* session);

        shuffle_session *session; // which session this order belongs to.
    };

    
    extern std::map<shuffle_info, std::vector<order_info>> booked_shuffle;

    // Book resource for a permute session.
    void book_shuffle_session(mpc_comm& com, shuffle_session* session);

    // Create new shuffle session and book resource for it.
    template <typename T>
    shuffle_session *book_shuffle_session(mpc_comm& com, int logsz, size_t veclen, int batch, const permutation& perm = {}) {
        shuffle_session *new_session = new shuffle_session();
        new_session->init<T>(com.get_n_party(), logsz, veclen, batch, perm);
        book_shuffle_session(com, new_session);
        return new_session;
    }

    /*
        This function processes all orders in booked_permute.
    */
    void process_all_orders(mpc_comm& com);

    // Algorithm 3: check publicly whether beta * a = b + r.
    bool verify(mpc_comm& com, const vectors<ClearType>& a,
            const vectors<ClearType>& b, ShareType beta,
            const vectors<ShareType>& r, bool authenticate_now = true);
    // Algorithm 4: check the compressed relation received by one party.
    bool partial_verify(mpc_comm & com, int who, const vectors<ClearType>& a,
            const vectors<ClearType>& b, ShareType beta,
            const vectors<ShareType>& r, bool authenticate_now = true);
}
