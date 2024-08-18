#pragma once

#include "global.h"

#include "permutation.h"
#include "vectors.h"

#include "mpc_communicator.h"
#include "Song_shuffle.h"

namespace gjcShuffle {
    // Shuffle Correlation
    class shuffle_cor {
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
        shuffle_info(int logsz, int veclen);

        int logsz, veclen;

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
        friend void clear_unused();

    protected:
        int n_party, logsz;
        size_t veclen;
        int batch;

        shuffle_cor cor;
        std::vector<song2023::permute_session> permute_sessions;
    public:
        shuffle_session() = default;

        template <typename T>
        void init(int _n_party, int _logsz, size_t _veclen, int _batch) {
            n_party = _n_party;
            logsz = _logsz;
            veclen = _veclen; // permute r, beta*r, rp
            batch = _batch;
            permute_sessions.resize(n_party);

            cor.r.resize(n_party);
            cor.beta_r.resize(n_party);
            cor.rp.resize(n_party);
            cor.permuted_r.resize(n_party);
            cor.permuted_beta_r.resize(n_party);
            cor.permuted_rp.resize(n_party);
            cor.perm = permutation(1 << logsz, true);

            for (int i = 0; i < n_party; ++i) {
                permute_sessions[i].init<T>(i, logsz, 3 * veclen, batch, cor.perm);
            }
        }

        // Perform the magic of permute protocol!
        //void perform(mpc_comm& com, vectors<ClearType>& val);
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
    shuffle_session *book_shuffle_session(mpc_comm& com, int logsz, int veclen, int batch) {
        shuffle_session *new_session = new shuffle_session();
        new_session->init<T>(com.get_n_party(), logsz, veclen, batch);
        book_shuffle_session(com, new_session);
        return new_session;
    }

    /*
        This function processes all orders in booked_permute.
    */
    void process_all_orders(mpc_comm& com);
}
