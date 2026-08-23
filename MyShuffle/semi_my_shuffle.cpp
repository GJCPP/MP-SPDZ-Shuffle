#include "semi_my_shuffle.h"

namespace semiHonest {
    std::map<shuffle_info, std::vector<order_info>> booked_shuffle;

    namespace {
        void private_reconstruct_to(mpc_comm& com, int party,
                const vectors<ClearType>& share, vectors<ClearType>& clear)
        {
            int me = com.get_my_number();
            int n = com.get_n_party();
            if (me == party) {
                clear = share;
                vectors<ClearType> buff(share.num, share.len);
                for (int sender(0); sender != n; ++sender) {
                    if (sender == party) continue;
                    com.recv(sender, buff);
                    clear += buff;
                }
            } else {
                com.send(party, share);
            }
        }
    }

    void chase_permute_session::init(int _permuter, int _logsz, size_t _veclen,
            int _batch, const permutation& _perm)
    {
        permuter = _permuter;
        logsz = _logsz;
        veclen = _veclen;
        batch = _batch;
        perm = _perm;
        plans.clear();
        initialized = false;
        destroyed = false;
    }

    void chase_permute_session::prepare(mpc_comm& com)
    {
        int n = com.get_n_party();
        plans.resize(n);
        int sz = 1 << logsz;
        std::vector<int> int_perm;
        int_perm.reserve(perm.perm.size());
        for (auto x : perm.perm) {
            int_perm.push_back(x);
        }
        size_t sequential_rounds = 0;
        size_t parallel_rounds = 0;
        for (int sender(0); sender != n; ++sender) {
            if (sender == permuter) continue;
            size_t rounds_before = com.count_total_rounds();
            plans[sender] = chase2020::prepare_permute_clear(
                    com, sender, permuter, int_perm, sz, veclen, batch);
            size_t rounds_after = com.count_total_rounds();
            size_t delta_rounds = rounds_after - rounds_before;
            sequential_rounds += delta_rounds;
            parallel_rounds = std::max(parallel_rounds, delta_rounds);
        }
        if (sequential_rounds > parallel_rounds) {
            com.add_round_adjustment(-static_cast<long long>(sequential_rounds - parallel_rounds));
        }
        initialized = true;
    }

    void chase_permute_session::perform(mpc_comm& com, vectors<ClearType>& val)
    {
        int n = com.get_n_party();
        int me = com.get_my_number();
        size_t sz = size_t(1) << logsz;

        if (val.num != sz || val.len != veclen) {
            std::cerr << "chase_permute_session::perform : size mismatch, "
                    << val.num << "x" << val.len << " vs "
                    << sz << "x" << veclen << std::endl;
            throw std::runtime_error("chase_permute_session::perform : size mismatch.");
        }
        if (!initialized) {
            std::cerr << FAIL_INFO << "Not initialized. Please call process_all_orders." << std::endl;
            throw std::runtime_error("chase_permute_session::perform : not initialized.");
        }
        if (destroyed) {
            std::cerr << FAIL_INFO << "permute session destroyed." << std::endl;
            throw std::runtime_error("chase_permute_session::perform : destroyed.");
        }

        vectors<ClearType> result;
        if (me == permuter) {
            result = val;
            perm.perform(result);
        }

        size_t sequential_rounds = 0;
        size_t parallel_rounds = 0;
        for (int sender(0); sender != n; ++sender) {
            if (sender == permuter) continue;
            if (me != sender && me != permuter) continue;

            size_t rounds_before = com.count_total_rounds();
            vectors<ClearType> local(val.num, val.len);
            if (me == sender) {
                local = val;
            }
            chase2020::permute(com, local, plans[sender]);
            if (me == sender) {
                val = local;
            } else {
                result += local;
            }
            size_t rounds_after = com.count_total_rounds();
            size_t delta_rounds = rounds_after - rounds_before;
            sequential_rounds += delta_rounds;
            parallel_rounds = std::max(parallel_rounds, delta_rounds);
        }
        if (sequential_rounds > parallel_rounds) {
            com.add_round_adjustment(-static_cast<long long>(sequential_rounds - parallel_rounds));
        }

        if (me == permuter) {
            val = result;
        }
        destroy();
    }

    void chase_permute_session::destroy()
    {
        destroyed = true;
    }

    const permutation& chase_permute_session::get_perm() const
    {
        return perm;
    }

    shuffle_info::shuffle_info(int _logsz, size_t _veclen)
        : logsz(_logsz), veclen(_veclen)
    {
    }

    bool shuffle_info::operator<(const shuffle_info& info) const
    {
        if (logsz != info.logsz) return logsz < info.logsz;
        return veclen < info.veclen;
    }

    order_info::order_info(shuffle_session* _session)
        : session(_session)
    {
    }

    void shuffle_session::init(int _n_party, int _logsz, size_t _veclen,
            int _batch, const permutation& _perm)
    {
        n_party = _n_party;
        logsz = _logsz;
        veclen = _veclen;
        batch = _batch;
        if (_perm.n != 0) {
            perm = _perm;
        } else {
            perm = permutation(size_t(1) << logsz, true);
        }

        destroyed = false;
        permute_sessions.resize(n_party);

        cor.initialized = false;
        cor.perm = perm;
        cor.r.resize(n_party);
        cor.permuted_r.resize(n_party);
        cor.z.clear();

        for (int i(0); i != n_party; ++i) {
            permute_sessions[i].init(i, logsz, veclen, batch, cor.perm);
        }
    }

    void shuffle_session::set_init_flag()
    {
        cor.initialized = true;
    }

    void shuffle_session::destroy()
    {
        destroyed = true;
    }

    const permutation& shuffle_session::get_perm() const
    {
        return perm;
    }

    void book_shuffle_session(mpc_comm&, shuffle_session* session)
    {
        shuffle_info info(session->logsz, session->veclen);
        booked_shuffle[info].push_back(order_info(session));
    }

    shuffle_session *book_shuffle_session(mpc_comm& com, int logsz, size_t veclen,
            int batch, const permutation& perm)
    {
        shuffle_session *new_session = new shuffle_session();
        new_session->init(com.get_n_party(), logsz, veclen, batch, perm);
        book_shuffle_session(com, new_session);
        return new_session;
    }

    void process_all_orders(mpc_comm& com)
    {
        for (auto& info : booked_shuffle) {
            for (auto& order : info.second) {
                shuffle_session& session = *order.session;
                if (session.destroyed) {
                    std::cerr << FAIL_INFO << "destroyed shuffle session." << std::endl;
                    throw std::runtime_error("process_all_orders : destroyed shuffle session.");
                }

                size_t num = size_t(1) << session.logsz;
                size_t prepare_sequential_rounds = 0;
                size_t prepare_parallel_rounds = 0;
                for (int party(0); party != session.n_party; ++party) {
                    session.cor.r[party].resize(num, session.veclen);
                    session.cor.permuted_r[party].resize(num, session.veclen);
                    com.rand_int(session.cor.r[party]);
                    session.cor.permuted_r[party] = session.cor.r[party];
                    size_t rounds_before = com.count_total_rounds();
                    session.permute_sessions[party].prepare(com);
                    size_t rounds_after = com.count_total_rounds();
                    size_t delta_rounds = rounds_after - rounds_before;
                    prepare_sequential_rounds += delta_rounds;
                    prepare_parallel_rounds = std::max(prepare_parallel_rounds, delta_rounds);
                }
                if (prepare_sequential_rounds > prepare_parallel_rounds) {
                    com.add_round_adjustment(-static_cast<long long>(
                            prepare_sequential_rounds - prepare_parallel_rounds));
                }
            }
        }

        for (auto& info : booked_shuffle) {
            for (auto& order : info.second) {
                shuffle_session& session = *order.session;
                size_t perform_sequential_rounds = 0;
                size_t perform_parallel_rounds = 0;
                for (int party(0); party != session.n_party; ++party) {
                    size_t rounds_before = com.count_total_rounds();
                    session.permute_sessions[party].perform(com, session.cor.permuted_r[party]);
                    size_t rounds_after = com.count_total_rounds();
                    size_t delta_rounds = rounds_after - rounds_before;
                    perform_sequential_rounds += delta_rounds;
                    perform_parallel_rounds = std::max(perform_parallel_rounds, delta_rounds);
                }
                if (perform_sequential_rounds > perform_parallel_rounds) {
                    com.add_round_adjustment(-static_cast<long long>(
                            perform_sequential_rounds - perform_parallel_rounds));
                }
            }
        }

        for (auto& info : booked_shuffle) {
            for (auto& order : info.second) {
                shuffle_session& session = *order.session;
                size_t num = size_t(1) << session.logsz;
                size_t reconstruct_sequential_rounds = 0;
                size_t reconstruct_parallel_rounds = 0;
                for (int party(1); party != session.n_party; ++party) {
                    vectors<ClearType> shared_z =
                            session.cor.permuted_r[party - 1] - session.cor.r[party];
                    vectors<ClearType> clear_z(num, session.veclen);
                    size_t rounds_before = com.count_total_rounds();
                    private_reconstruct_to(com, party, shared_z, clear_z);
                    size_t rounds_after = com.count_total_rounds();
                    size_t delta_rounds = rounds_after - rounds_before;
                    reconstruct_sequential_rounds += delta_rounds;
                    reconstruct_parallel_rounds = std::max(reconstruct_parallel_rounds, delta_rounds);
                    if (com.get_my_number() == party) {
                        session.cor.z = clear_z;
                    }
                }
                if (reconstruct_sequential_rounds > reconstruct_parallel_rounds) {
                    com.add_round_adjustment(-static_cast<long long>(
                            reconstruct_sequential_rounds - reconstruct_parallel_rounds));
                }
                session.set_init_flag();
            }
        }

        booked_shuffle.clear();
    }

    void shuffle_session::perform(mpc_comm& com, vectors<ClearType>& val)
    {
        if (val.num != (size_t(1) << logsz) || val.len != veclen) {
            std::cerr << "semiHonest::shuffle_session::perform : Invalid input size,"
                    << val << " != " << (size_t(1) << logsz)
                    << " or " << val.len << " != " << veclen << std::endl;
            throw std::runtime_error("semiHonest::shuffle_session::perform : Invalid input size.");
        }
        if (!cor.initialized) {
            std::cerr << FAIL_INFO << "uninitialized shuffle session. Please call process_all_orders first." << std::endl;
            throw std::runtime_error("semiHonest::shuffle_session::perform : uninitialized.");
        }
        if (destroyed) {
            std::cerr << FAIL_INFO << "shuffle session destroyed. Do not use one session twice." << std::endl;
            throw std::runtime_error("semiHonest::shuffle_session::perform : destroyed.");
        }

        int me = com.get_my_number();
        int n = com.get_n_party();
        size_t num = size_t(1) << logsz;
        vectors<ClearType> masked(num, veclen);

        vectors<ClearType> shared_masked = val - cor.r[0];
        private_reconstruct_to(com, 0, shared_masked, masked);

        if (me == 0) {
            cor.perm.perform(masked);
            if (n > 1) {
                com.send(1, masked);
            }
        } else {
            com.recv(me - 1, masked);
            masked += cor.z;
            cor.perm.perform(masked);
            if (me + 1 != n) {
                com.send(me + 1, masked);
            }
        }

        com.unchecked_broadcast(n - 1, masked);

        val = cor.permuted_r[n - 1];
        if (me == 0) {
            val += masked;
        }
        destroy();
    }
}
