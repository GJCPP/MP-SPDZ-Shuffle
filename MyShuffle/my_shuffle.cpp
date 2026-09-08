#include "my_shuffle.h"

namespace myShuffle {
    std::map<shuffle_info, std::vector<order_info>> booked_shuffle;

    shuffle_info::shuffle_info(int _logsz, size_t _veclen)
        : logsz(_logsz), veclen(_veclen)
    {
        ;
    }
    

    bool shuffle_info::operator<(const shuffle_info &info) const
    {
        if (logsz != info.logsz) return logsz < info.logsz;
        return veclen < info.veclen;
    }
    order_info::order_info(shuffle_session *_session)
        : session(_session)
    {
        ;
    }

    void book_shuffle_session(mpc_comm&, shuffle_session *session)
    {
        shuffle_info info(session->logsz, session->veclen);
        auto que = booked_shuffle.find(info);
        if (que == booked_shuffle.end()) {
            booked_shuffle.insert(std::make_pair(info, std::vector<order_info>()));
            que = booked_shuffle.find(info);
        }
        que->second.push_back(order_info(session));
    }
    
    void malloc_random_resource(mpc_comm&) {
        // Fetch all random resource
        for (auto &info : booked_shuffle) {
            for (auto &order : info.second) {
                shuffle_session &session = *order.session;
                size_t num = size_t(1) << session.logsz;
                size_t len = session.veclen;
                shuffle_cor &cor = session.cor;
                cor.c.resize(session.n_party);
                cor.d.resize(session.n_party);
                for (int party(0); party != session.n_party; ++party) {
                    cor.r[party].resize(num, len);
                    cor.beta_r[party].resize(num, len);
                    cor.rp[party].resize(num, len);
                    cor.permuted_r[party].resize(num, len);
                    cor.permuted_beta_r[party].resize(num, len);
                    cor.permuted_rp[party].resize(num, len);
                    cor.z[0].resize(num, len);
                    cor.z[1].resize(num, len);
                }
            }
        }
    }
    
    void fill_in_random_resource(mpc_comm& com) {
        // Fetch all random resource
        for (auto &info : booked_shuffle) {
            for (auto &order : info.second) {
                shuffle_session &session = *order.session;
                shuffle_cor &cor = session.cor;
                cor.beta = com.get_random();
                for (int party(1); party != session.n_party; ++party) {
                    cor.c[party] = com.get_random();
                }
                for (int party(0); party != session.n_party; ++party) {
                    for (auto& v : cor.r[party]) {
                        v = com.get_random();
                    }
                    for (auto& v : cor.rp[party]) {
                        v = com.get_random();
                    }
                }
            }
        }
    }

    void compute_partial_verification_correlations(mpc_comm& com) {
        struct correlation_work {
            shuffle_cor* cor;
            std::vector<std::vector<ShareType>> powers;
        };

        for (auto& info : booked_shuffle) {
            const size_t n = (size_t(1) << info.first.logsz) * info.first.veclen;
            std::vector<correlation_work> work;
            work.reserve(info.second.size());
            for (auto& order : info.second) {
                work.push_back({&order.session->cor,
                        std::vector<std::vector<ShareType>>(order.session->n_party)});
                if (n > 1) {
                    for (int party(1); party != order.session->n_party; ++party) {
                        work.back().powers[party].resize(n);
                        work.back().powers[party][1] = order.session->cor.c[party];
                    }
                }
            }

            // Compute c^2, ..., c^(n-1) in O(log n) multiplication rounds.
            size_t highest_power = 1;
            while (n > 1 && highest_power < n - 1) {
                const size_t next_highest = std::min(n - 1, 2 * highest_power);
                com.mul_init();
                for (auto& item : work) {
                    for (int party(1); party != com.get_n_party(); ++party) {
                        for (size_t exponent = highest_power + 1;
                                exponent <= next_highest; ++exponent) {
                            const size_t left = exponent / 2;
                            com.mul_append(item.powers[party][left],
                                    item.powers[party][exponent - left]);
                        }
                    }
                }
                com.mul_exchange();
                for (auto& item : work) {
                    for (int party(1); party != com.get_n_party(); ++party) {
                        for (size_t exponent = highest_power + 1;
                                exponent <= next_highest; ++exponent) {
                            item.powers[party][exponent] = com.mul_consume();
                        }
                    }
                }
                highest_power = next_highest;
            }

            if (n > 1) {
                com.mul_init();
                for (auto& item : work) {
                    for (int party(1); party != com.get_n_party(); ++party) {
                        const auto& r = item.cor->permuted_rp[party - 1];
                        for (size_t j(1); j != n; ++j) {
                            com.mul_append(item.powers[party][j], r.at(j));
                        }
                    }
                }
                com.mul_exchange();
            }
            for (auto& item : work) {
                for (int party(1); party != com.get_n_party(); ++party) {
                    item.cor->d[party] = item.cor->permuted_rp[party - 1].at(0);
                    for (size_t j(1); j != n; ++j) {
                        item.cor->d[party] += com.mul_consume();
                    }
                }
            }
        }
    }

    void compute_beta_r(mpc_comm& com) {
        // Compute beta x r
        com.mul_init();
        for (auto& info : booked_shuffle) {
            for (auto& order : info.second) {
                shuffle_session &session = *order.session;
                shuffle_cor &cor = session.cor;
                for (int party(0); party != session.n_party; ++party) {
                    for (ShareType& v : cor.r[party]) {
                        com.mul_append(v, order.session->cor.beta);
                    }
                }
            }
        }
        com.mul_exchange();
        for (auto& info : booked_shuffle) {
            for (auto& order : info.second) {
                shuffle_session &session = *order.session;
                shuffle_cor &cor = session.cor;
                for (int party(0); party != session.n_party; ++party) {
                    for (ShareType& v : cor.beta_r[party]) {
                        v = com.mul_consume();
                    }
                }
            }
        }
    }

    void compute_permuted_random_resource(mpc_comm& com) {
        // Perform all permute.
        size_t sequential_session_rounds = 0;
        size_t parallel_session_rounds = 0;
        for (auto& info : booked_shuffle) {
            for (auto& order : info.second) {
                size_t session_rounds_before = com.count_total_rounds();
                shuffle_session &session = *order.session;
                shuffle_cor &cor = session.cor;
                const size_t num = size_t(1) << session.logsz;
                static vectors<ShareType> r_betar_rp;
                r_betar_rp.resize(num, session.veclen * 3);
                size_t sequential_rounds = 0;
                size_t parallel_rounds = 0;
                for (int party(0); party != session.n_party; ++party) {
                    for (size_t i(0); i != num; ++i) {
                        for (size_t j(0); j != 3 * session.veclen; j += 3) {
                            r_betar_rp[i][j] = cor.r[party][i][j / 3];
                            r_betar_rp[i][j + 1] = cor.beta_r[party][i][j / 3];
                            r_betar_rp[i][j + 2] = cor.rp[party][i][j / 3];
                        }
                    }
                    size_t rounds_before = com.count_total_rounds();
                    session.permute_sessions[party].perform(com, r_betar_rp);
                    size_t rounds_after = com.count_total_rounds();
                    size_t delta_rounds = rounds_after - rounds_before;
                    sequential_rounds += delta_rounds;
                    parallel_rounds = std::max(parallel_rounds, delta_rounds);
                    for (size_t i(0); i != num; ++i) {
                        for (size_t j(0); j != 3 * session.veclen; j += 3) {
                            cor.permuted_r[party][i][j / 3] = r_betar_rp[i][j];
                            cor.permuted_beta_r[party][i][j / 3] = r_betar_rp[i][j + 1];
                            cor.permuted_rp[party][i][j / 3] = r_betar_rp[i][j + 2];
                        }
                    }
                }
                if (sequential_rounds > parallel_rounds) {
                    com.add_round_adjustment(-static_cast<long long>(sequential_rounds - parallel_rounds));
                }
                size_t session_rounds_after = com.count_total_rounds();
                size_t session_rounds = session_rounds_after - session_rounds_before;
                sequential_session_rounds += session_rounds;
                parallel_session_rounds = std::max(parallel_session_rounds, session_rounds);
            }
        }
        if (sequential_session_rounds > parallel_session_rounds) {
            com.add_round_adjustment(-static_cast<long long>(
                    sequential_session_rounds - parallel_session_rounds));
        }
    }

    void compute_z(mpc_comm& com) {
        com.private_output_init();
        for (auto& info : booked_shuffle) {
            for (auto& order : info.second) {
                shuffle_session &session = *order.session;
                shuffle_cor &cor = session.cor;
                const size_t num = size_t(1) << session.logsz;
                for (int party(1); party != session.n_party; ++party) {
                    static vectors<ShareType> z;
                    z.resize(num, session.veclen * 2);
                    for (size_t i(0); i != num; ++i) {
                        for (size_t j(0); j != session.veclen; ++j) {
                            z[i][j] = cor.permuted_r[party - 1][i][j] - cor.r[party][i][j];
                            z[i][j + session.veclen] = cor.permuted_beta_r[party - 1][i][j]
                                                        + cor.permuted_rp[party - 1][i][j]
                                                        - cor.beta_r[party][i][j]
                                                        - cor.rp[party][i][j];
                        }
                    }
                    com.private_output_append(party, z);
                }
            }
        }
        com.private_output_exchange();
        vectors<ClearType> buff;
        for (auto& info : booked_shuffle) {
            for (auto& order : info.second) {
                shuffle_session &session = *order.session;
                shuffle_cor &cor = session.cor;
                const size_t num = size_t(1) << session.logsz;
                buff.resize(num, session.veclen * 2);
                for (int party(1); party != session.n_party; ++party) {
                    com.private_output_consume(party, buff);
                    if (com.get_my_number() == party) {
                        cor.z[0].resize(num, session.veclen);
                        cor.z[1].resize(num, session.veclen);
                        for (size_t i(0); i != num; ++i) {
                            for (size_t j(0); j != session.veclen; ++j) {
                                cor.z[0][i][j] = buff[i][j];
                                cor.z[1][i][j] = buff[i][j + session.veclen];
                            }
                        }
                    }
                }
            }
        }
    }

    void clear_unused() {
        for (auto& info : booked_shuffle) {
            for (auto& order : info.second) {
                shuffle_session &session = *order.session;
                shuffle_cor &cor = session.cor;
                for (int party(1); party != session.n_party; ++party) {
                    cor.r[party].clear();
                    cor.beta_r[party].clear();
                }
                for (int party(0); party != session.n_party - 1; ++party) {
                    cor.permuted_r[party].clear();
                    cor.permuted_beta_r[party].clear();
                }
            }
        }
    }

    void set_init_flag() {
        for (auto& info : booked_shuffle) {
            for (auto& order : info.second) {
                shuffle_session &session = *order.session;
                session.set_init_flag();
            }
        }
    }

    void process_all_orders(mpc_comm &com)
    {
        int n_party = com.get_n_party();
        // Book all permute sessions and count the total number of secret random values.
        size_t require_mul = 0;
        for (auto &info : booked_shuffle) {
            for (auto &order : info.second) {
                if (order.session->destroyed) {
                    std::cerr << FAIL_INFO << "destroyed shuffle session." << std::endl;
                    throw std::runtime_error("process_all_orders : destroyed shuffle session.");
                }
                for (int i = 0; i < order.session->n_party; ++i) {
                    song2023::book_permute_session(com, &order.session->permute_sessions[i]);
                }
                size_t n = (1 << order.session->logsz) * order.session->veclen;
                // Prepare <beta>, <r_i>, <r^'_i>, <c_i>, and the three
                // terminal-Verify random values <e>, <rho>, and <gamma>.
                com.prepare_more_random_lazy(
                        2 * n_party * n + n_party + 3);
                // Prepare multiplication of <beta> with <r_i> and the input.
                com.prepare_more_mul_lazy(n_party * n);
                // Online computes beta*x and one blinded aggregate gamma*s.
                require_mul += n + 1;
                // For every receiver, compute c^2, ..., c^(n-1), followed
                // by d = sum_j c^j * permuted_rp[j].
                if (n > 1) {
                    require_mul += (n_party - 1) * (2 * n - 3);
                }
                // Prepare private output of <z_i> (offline) and <masked real data> (online)
                for (int party(0); party != n_party; ++party) {
                    com.prepare_more_private_output_lazy(party,
                            2 * n + (party == 0 ? 0 : 1));
                }
            }
        }

        size_t parallel_prepare_sequential_rounds = 0;
        size_t parallel_prepare_rounds = 0;
        auto add_prepare_task_rounds = [&](size_t before) {
            size_t after = com.count_total_rounds();
            size_t delta = after - before;
            parallel_prepare_sequential_rounds += delta;
            parallel_prepare_rounds = std::max(parallel_prepare_rounds, delta);
        };

        size_t stage_rounds_before = com.count_total_rounds();
        com.prepare_more_random_now();
        add_prepare_task_rounds(stage_rounds_before);
        // std::cout << "Random resource prepared." << std::endl;
        com.prepare_more_mul_lazy(require_mul);
        stage_rounds_before = com.count_total_rounds();
        com.prepare_more_mul_now();
        add_prepare_task_rounds(stage_rounds_before);
        // std::cout << "Mul resource prepared." << std::endl;
        stage_rounds_before = com.count_total_rounds();
        com.prepare_more_private_output_now();
        add_prepare_task_rounds(stage_rounds_before);
        // std::cout << "Private resource prepared." << std::endl;
        stage_rounds_before = com.count_total_rounds();
        song2023::process_all_orders(com);
        add_prepare_task_rounds(stage_rounds_before);
        if (parallel_prepare_sequential_rounds > parallel_prepare_rounds) {
            com.add_round_adjustment(-static_cast<long long>(
                    parallel_prepare_sequential_rounds - parallel_prepare_rounds));
        }

        malloc_random_resource(com);
        fill_in_random_resource(com); // 2 * n * n_party randoms.
        compute_beta_r(com); // n * n_party mults.

        // std::cout << "Beta r computed." << std::endl;

        compute_permuted_random_resource(com);
        compute_partial_verification_correlations(com);
        compute_z(com); // 2 * n private output PER party.

        // std::cout << "z computed." << std::endl;

        set_init_flag();
        clear_unused();
        booked_shuffle.clear();
    }

    void verify(mpc_comm& com,
            const vectors<ClearType>& incoming_a,
            const vectors<ClearType>& incoming_b,
            const vectors<ClearType>& final_a,
            const vectors<ClearType>& final_b,
            ShareType beta,
            const std::vector<ShareType>& c,
            const std::vector<ShareType>& d,
            const vectors<ShareType>& final_r)
    {
        const int me = com.get_my_number();
        const int n_party = com.get_n_party();
        if (c.size() != static_cast<size_t>(n_party)
                || d.size() != static_cast<size_t>(n_party)
                || final_a.size() == 0
                || final_a.size() != final_b.size()
                || final_a.size() != final_r.size()) {
            throw std::runtime_error("verify: invalid correlation size");
        }
        if (me != 0 && (incoming_a.size() == 0
                || incoming_a.size() != incoming_b.size())) {
            throw std::runtime_error("verify: invalid retained message size");
        }

        // Algorithm 3 privately opens every c_i in one MPC opening round.
        // Only receiver i can remove its one-time output mask.
        std::vector<ClearType> opened_c(n_party);
        com.private_output_init();
        for (int who = 1; who != n_party; ++who) {
            com.private_output_append(who, c[who]);
        }
        com.private_output_exchange();
        for (int who = 1; who != n_party; ++who) {
            com.private_output_consume(who, opened_c[who]);
        }

        // All receiver broadcasts are independent and fixed by one parallel
        // authenticated-broadcast invocation.
        std::vector<std::array<ClearType, 2>> compressed(n_party);
        for (auto& pair : compressed) {
            pair = {ClearType(0), ClearType(0)};
        }
        if (me != 0) {
            ClearType power(1);
            for (size_t j = 0; j != incoming_a.size(); ++j) {
                compressed[me][0] += power * incoming_a.at(j);
                compressed[me][1] += power * incoming_b.at(j);
                power *= opened_c[me];
            }
        }
        com.broadcast_pairs(compressed);

        const auto mac_key = ShareType::get_mac_key();
        std::vector<ShareType> residuals;
        residuals.reserve(n_party);
        for (int who = 1; who != n_party; ++who) {
            residuals.push_back(compressed[who][0] * beta
                    - ShareType::constant(compressed[who][1], me, mac_key)
                    - d[who]);
        }

        ShareType shared_e = com.get_random();
        ClearType e;
        com.output_immediately(shared_e, e);

        ClearType power(1);
        ShareType final_residual = final_a.at(0) * beta
                - ShareType::constant(final_b.at(0), me, mac_key)
                - final_r.at(0);
        for (size_t j = 1; j != final_a.size(); ++j) {
            power *= e;
            final_residual += power * (final_a.at(j) * beta
                    - ShareType::constant(final_b.at(j), me, mac_key)
                    - final_r.at(j));
        }
        residuals.push_back(final_residual);

        ShareType shared_rho = com.get_random();
        ClearType rho;
        com.output_immediately(shared_rho, rho);
        power = ClearType(1);
        ShareType aggregate = residuals.at(0);
        for (size_t i = 1; i != residuals.size(); ++i) {
            power *= rho;
            aggregate += power * residuals[i];
        }

        ShareType gamma = com.get_random();
        com.mul_init();
        com.mul_append(gamma, aggregate);
        com.mul_exchange();
        ShareType blinded = com.mul_consume();

        // SPDZ realization of Section 8.1: authenticate every tentative
        // opening and multiplication transcript used to compute f before f
        // itself is revealed. A failure here aborts without exposing f.
        com.output_check();

        ClearType f;
        com.output_immediately(blinded, f);
        com.output_check();
        if (!f.is_zero()) {
            std::cerr << FAIL_INFO << " : Verification failed." << std::endl;
            throw std::runtime_error("verify: verification failed");
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

    void shuffle_session::perform(mpc_comm &com, vectors<ShareType> &val)
    {
        if (val.num != (size_t(1) << logsz) || val.len != veclen) {
            std::cerr << "shuffle_session::perform : Invalid input size," << val
                    << " != " << (1 << logsz) << " or " << val.len << " != " << veclen << std::endl;
            throw std::runtime_error("shuffle_session::perform : Invalid input size.");
        }
        if (!cor.initialized) {
            std::cerr << FAIL_INFO << "uninitialized shuffle session. Please call process_all_orders first." << std::endl;
            throw std::runtime_error("shuffle_session::perform : uninitialized shuffle session.");
        }
        if (destroyed) {
            std::cerr << FAIL_INFO << "shuffle session destroyed. Do not use one session twice." << std::endl;
            throw std::runtime_error("shuffle_session::perform : shuffle session destroyed.");
        }
        size_t num = 1 << logsz, len = veclen;
        vectors<ShareType> beta_val(num, len);

        com.mul_init();
        for (auto& v : val) {
            com.mul_append(v, cor.beta); // n mults.
        }
        com.mul_exchange();
        com.mul_consume(beta_val);
        vectors<ShareType> shared_z00(num, len), shared_z01(num, len);
        shared_z00 = val - cor.r[0];
        shared_z01 = beta_val - cor.beta_r[0] - cor.rp[0];
        vectors<ClearType> z00(num, len), z01(num, len), z(num * 2, len);
        com.private_output_init();
        com.private_output_append(0, shared_z00); // 2 * n private output for 0-th party.
        com.private_output_append(0, shared_z01);
        com.private_output_exchange();
        com.private_output_consume(0, z00);
        com.private_output_consume(0, z01);

        vectors<ClearType> incoming_a, incoming_b;
        int me = com.get_my_number();
        if (me == 0) {
            cor.perm.perform(z00);
            cor.perm.perform(z01);
            z = vectors<ClearType>::cat(z00, z01);
            com.send(1, z);
        } else {
            com.recv(me - 1, z);
            z.split(num, z00, z01);
            incoming_a = z00;
            incoming_b = z01;
            z00 += cor.z[0];
            z01 += cor.z[1];
            cor.perm.perform(z00);
            cor.perm.perform(z01);
            if (me + 1 != com.get_n_party()) {
                z = vectors<ClearType>::cat(z00, z01);
                com.send(me + 1, z);
            }
        }

        // Only after every sequential message has been fixed does the last
        // party broadcast y_n and the parties invoke terminal Verify once.
        z = vectors<ClearType>::cat(z00, z01);
        com.broadcast(com.get_n_party() - 1, z);
        z.split(num, z00, z01);
        verify(com, incoming_a, incoming_b, z00, z01, cor.beta,
                cor.c, cor.d,
                cor.permuted_rp[com.get_n_party() - 1]);

        vectors<ShareType> shared_y(num, len);
        auto mac_key = ShareType::get_mac_key();
        for (size_t i(0); i != num; ++i) {
            for (size_t j(0); j != len; ++j) {
                shared_y[i][j] = ShareType::constant(z00[i][j], me, mac_key);
            }
        }
        shared_y += cor.permuted_r[com.get_n_party() - 1];
        val = shared_y;
        destroy();
    }

    const permutation &shuffle_session::get_perm() const
    {
        return perm;
    }
}
