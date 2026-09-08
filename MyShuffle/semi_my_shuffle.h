#pragma once

#include <map>
#include <vector>

#include "global.h"
#include "vectors.h"
#include "permutation.h"
#include "mpc_communicator.h"
#include "Chase_shuffle.h"

namespace semiHonest {
    using myShuffle::mpc_comm;

    class chase_permute_session {
    protected:
        int permuter = 0;
        int logsz = 0;
        size_t veclen = 0;
        int batch = 0;
        permutation perm;
        std::vector<chase2020::clear_shuffle_pair> plans;
        bool initialized = false;
        bool destroyed = false;

        void destroy();
    public:
        chase_permute_session() = default;

        void init(int _permuter, int _logsz, size_t _veclen, int _batch,
                const permutation& _perm);

        void prepare(mpc_comm& com);
        void perform(mpc_comm& com, vectors<ClearType>& val);

        const permutation& get_perm() const;
    };

    class shuffle_cor {
        friend class shuffle_session;
    protected:
        bool initialized = false;
    public:
        permutation perm;
        std::vector<vectors<ClearType>> r;
        std::vector<vectors<ClearType>> permuted_r;
        vectors<ClearType> z;
    };

    class shuffle_info {
    public:
        shuffle_info() = default;
        shuffle_info(int logsz, size_t veclen);

        int logsz = 0;
        size_t veclen = 0;

        bool operator<(const shuffle_info& info) const;
    };

    class shuffle_session {
        friend void book_shuffle_session(mpc_comm& com, shuffle_session* session);
        friend void process_all_orders(mpc_comm& com);
    protected:
        int n_party = 0;
        int logsz = 0;
        size_t veclen = 0;
        int batch = 0;
        permutation perm;

        shuffle_cor cor;
        std::vector<chase_permute_session> permute_sessions;

        bool destroyed = false;

        void set_init_flag();
        void destroy();
    public:
        shuffle_session() = default;

        void init(int _n_party, int _logsz, size_t _veclen, int _batch,
                const permutation& _perm = {});

        void perform(mpc_comm& com, vectors<ClearType>& val);

        const permutation& get_perm() const;
    };

    class order_info {
    public:
        order_info() = default;
        order_info(shuffle_session* session);

        shuffle_session *session = nullptr;
    };

    extern std::map<shuffle_info, std::vector<order_info>> booked_shuffle;

    void book_shuffle_session(mpc_comm& com, shuffle_session* session);

    shuffle_session *book_shuffle_session(mpc_comm& com, int logsz, size_t veclen,
            int batch, const permutation& perm = {});

    void process_all_orders(mpc_comm& com);
}
