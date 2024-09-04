#pragma once

#include "global.h"

#include "permutation.h"
#include "vectors.h"

#include "mpc_communicator.h"
#include "Song_shuffle.h"

namespace gjcShuffle {
    // Shuffle Correlation
    class shuffle_session;
    class shuffle_cor {
        friend shuffle_session;
    protected:
        bool initialized = false;
    public:
        shuffle_cor() = default;

        permutation perm;
        ShareType beta; // MAC key
        std::vector<vectors<ShareType>> r, beta_r, rp;
        std::vector<vectors<ShareType>> permuted_r, permuted_beta_r, permuted_rp;
        vectors<ClearType> z[2]; // If the party is not zero party, z is non-empty.
    };

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

            cor.r.resize(n_party);
            cor.beta_r.resize(n_party);
            cor.rp.resize(n_party);
            cor.permuted_r.resize(n_party);
            cor.permuted_beta_r.resize(n_party);
            cor.permuted_rp.resize(n_party);
            cor.perm = perm;

            for (int i = 0; i < n_party; ++i) {
                permute_sessions[i].init<T>(i, logsz, 3 * veclen, batch, cor.perm);
            }
        }

        // Perform the magic of shuffle protocol! (online phase)
        void perform(mpc_comm& com, vectors<ShareType>& val);

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

    // Check if beta * a = b + r
    void verify(mpc_comm& com, ClearType a, ClearType b, ShareType beta, ShareType r);
    // Check if beta * a = b + r
    void verify(mpc_comm & com, int who, const vectors<ClearType>& a, const vectors<ClearType>& b, ShareType beta, const vectors<ShareType>& r);
}
