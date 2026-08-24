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
                // Prepare <beta>, <r_i>, <r^'_i>, <c_i>, and the random values
                // used to compress and blind the online verification residuals.
                com.prepare_more_random_lazy(2 * n_party * n + 2 * n_party + 1);
                // Prepare multiplication of <beta> with <r_i> and the input.
                com.prepare_more_mul_lazy(n_party * n);
                // Each PartialVerify and the final Verify use one additional
                // multiplication to blind their residual before opening it.
                require_mul += n + n_party;
                // For every PartialVerify, compute c^2, ..., c^(n-1), followed
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

    static bool open_blinded_residual(mpc_comm& com,
            const ShareType& residual, bool authenticate_now)
    {
        ShareType gamma = com.get_random();
        com.mul_init();
        com.mul_append(gamma, residual);
        com.mul_exchange();
        ShareType blinded = com.mul_consume();

        ClearType res;
        com.output_immediately(blinded, res);
        if (authenticate_now) {
            com.output_check();
        }
        return res.is_zero();
    }

    bool verify(mpc_comm& com, const vectors<ClearType>& a,
            const vectors<ClearType>& b, ShareType beta,
            const vectors<ShareType>& r, bool authenticate_now)
    {
        if (a.size() == 0 || a.size() != b.size() || a.size() != r.size()) {
            throw std::runtime_error("verify : Invalid input size.");
        }

        ShareType shared_e = com.get_random();
        ClearType e;
        com.output_immediately(shared_e, e);
        if (authenticate_now) {
            com.output_check();
        }

        auto mac_key = ShareType::get_mac_key();
        ClearType power(1);
        ShareType residual = a.at(0) * beta
                - ShareType::constant(b.at(0), com.get_my_number(), mac_key)
                - r.at(0);
        for (size_t i(1); i != a.size(); ++i) {
            power *= e;
            residual += power * (a.at(i) * beta
                    - ShareType::constant(b.at(i), com.get_my_number(), mac_key)
                    - r.at(i));
        }
        return open_blinded_residual(com, residual, authenticate_now);
    }

    bool partial_verify(mpc_comm & com, int who, const vectors<ClearType>& a,
            const vectors<ClearType>& b, ShareType beta,
            ShareType c, ShareType d, bool authenticate_now)
    {
        ClearType opened_c, w1(0), w2(0);
        com.private_output_init();
        com.private_output_append(who, c);
        com.private_output_exchange();
        com.private_output_consume(who, opened_c);
        if (authenticate_now) {
            com.output_check();
        }

        if (com.get_my_number() == who) {
            if (a.size() == 0 || a.size() != b.size()) {
                throw std::runtime_error("partial_verify : Invalid input size.");
            }
            ClearType power(1);
            for (size_t i(0); i != a.size(); ++i) {
                w1 += power * a.at(i);
                w2 += power * b.at(i);
                power *= opened_c;
            }
        }
        std::vector<ClearType> msg = { w1, w2 };
        com.broadcast(who, msg);
        w1 = msg[0], w2 = msg[1];
        ShareType residual = w1 * beta
                - ShareType::constant(w2, com.get_my_number(), ShareType::get_mac_key())
                - d;
        return open_blinded_residual(com, residual, authenticate_now);
    }

    void shuffle_session::set_init_flag()
    {
        cor.initialized = true;
    }

    void shuffle_session::destroy()
    {
        destroyed = true;
    }

    void shuffle_session::perform(mpc_comm &com, vectors<ShareType> &val,
            bool strong_abort_privacy)
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

        // In the strong instantiation, authenticate the private opening before
        // party 0 can apply its secret permutation to it. The batched-check
        // instantiation deliberately defers this and all relation-opening MAC
        // checks until the end, which is faster in high-latency networks but
        // does not provide abort privacy.
        if (strong_abort_privacy) {
            com.output_check();
        }

        bool verification_failed = false;
        auto partial_verify_opening = [&](int who, const vectors<ClearType>& a,
                                          const vectors<ClearType>& b) {
            bool valid = partial_verify(com, who, a, b, cor.beta,
                    cor.c[who], cor.d[who],
                    strong_abort_privacy);
            if (!valid && strong_abort_privacy) {
                std::cerr << FAIL_INFO << " : Verification failed." << std::endl;
                throw std::runtime_error("verify : Verification failed.");
            }
            verification_failed = verification_failed || !valid;
        };

        int me = com.get_my_number();
        if (me == 0) {
            // perform permute on z00 and z01
            cor.perm.perform(z00);
            cor.perm.perform(z01);
            z = vectors<ClearType>::cat(z00, z01);
            com.send(1, z);
            for (int who(1); who != com.get_n_party(); ++who) {
                partial_verify_opening(who, {}, {});
            }
        } else {
            for (int who(1); who != me; ++who) {
                partial_verify_opening(who, {}, {});
            }
            com.recv(me - 1, z);
            z.split(num, z00, z01);
            partial_verify_opening(me, z00, z01);
            z00 += cor.z[0];
            z01 += cor.z[1];
            // Algorithm 6 verifies the incoming message before this party's
            // secret permutation can affect the next message.
            cor.perm.perform(z00);
            cor.perm.perform(z01);
            if (me + 1 != com.get_n_party()) {
                z = vectors<ClearType>::cat(z00, z01);
                com.send(me + 1, z);
            }
            for (int who(me + 1); who != com.get_n_party(); ++who) {
                partial_verify_opening(who, {}, {});
            }
        }
        z = vectors<ClearType>::cat(z00, z01);
        com.broadcast(com.get_n_party() - 1, z);
        z.split(num, z00, z01);

        // Algorithm 3 verifies the final public pair after the last party's
        // permutation and before it is converted back to a shared output.
        bool final_valid = verify(com, z00, z01, cor.beta,
                cor.permuted_rp[com.get_n_party() - 1],
                strong_abort_privacy);
        if (!final_valid && strong_abort_privacy) {
            std::cerr << FAIL_INFO << " : Final verification failed." << std::endl;
            throw std::runtime_error("verify : Final verification failed.");
        }
        verification_failed = verification_failed || !final_valid;

        if (!strong_abort_privacy) {
            com.output_check();
            if (verification_failed) {
                std::cerr << FAIL_INFO << " : Verification failed." << std::endl;
                throw std::runtime_error("verify : Verification failed.");
            }
        }

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
