#include "my_shuffle.h"

namespace gjcShuffle {
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

    void book_shuffle_session(mpc_comm &com, shuffle_session *session)
    {
        shuffle_info info(session->logsz, session->veclen);
        auto que = booked_shuffle.find(info);
        if (que == booked_shuffle.end()) {
            booked_shuffle.insert(std::make_pair(info, std::vector<order_info>()));
            que = booked_shuffle.find(info);
        }
        que->second.push_back(order_info(session));
    }
    
    void malloc_random_resource(mpc_comm& com) {
        // Fetch all random resource
        for (auto &info : booked_shuffle) {
            for (auto &order : info.second) {
                shuffle_session &session = *order.session;
                size_t num = size_t(1) << session.logsz;
                size_t len = session.veclen;
                shuffle_cor &cor = session.cor;
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
        song2023::process_all_orders(com);
        for (auto& info : booked_shuffle) {
            for (auto& order : info.second) {
                shuffle_session &session = *order.session;
                shuffle_cor &cor = session.cor;
                static vectors<ShareType> r_betar_rp;
                r_betar_rp.resize(1 << session.logsz, session.veclen * 3);
                for (int party(0); party != session.n_party; ++party) {
                    for (size_t i(0); i != (1 << session.logsz); ++i) {
                        for (size_t j(0); j != 3 * session.veclen; j += 3) {
                            r_betar_rp[i][j] = cor.r[party][i][j / 3];
                            r_betar_rp[i][j + 1] = cor.beta_r[party][i][j / 3];
                            r_betar_rp[i][j + 2] = cor.rp[party][i][j / 3];
                        }
                    }
                    session.permute_sessions[party].perform(com, r_betar_rp);
                    for (size_t i(0); i != 1 << session.logsz; ++i) {
                        for (size_t j(0); j != 3 * session.veclen; j += 3) {
                            cor.permuted_r[party][i][j / 3] = r_betar_rp[i][j];
                            cor.permuted_beta_r[party][i][j / 3] = r_betar_rp[i][j + 1];
                            cor.permuted_rp[party][i][j / 3] = r_betar_rp[i][j + 2];
                        }
                    }
                }
            }
        }
    }

    void compute_z(mpc_comm& com) {
        com.private_output_init();
        for (auto& info : booked_shuffle) {
            for (auto& order : info.second) {
                shuffle_session &session = *order.session;
                shuffle_cor &cor = session.cor;
                for (int party(1); party != session.n_party; ++party) {
                    static vectors<ShareType> z;
                    z.resize(1 << session.logsz, session.veclen * 2);
                    for (size_t i(0); i != 1 << session.logsz; ++i) {
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
                buff.resize(1 << session.logsz, session.veclen * 2);
                for (int party(1); party != session.n_party; ++party) {
                    com.private_output_consume(party, buff);
                    if (com.get_my_number() == party) {
                        cor.z[0].resize(1 << session.logsz, session.veclen);
                        cor.z[1].resize(1 << session.logsz, session.veclen);
                        for (size_t i(0); i != 1 << session.logsz; ++i) {
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
                // Prepare <beta> and <r_i>, <r^'_i>
                com.prepare_more_random_lazy(2 * n_party * n + 1);
                // Prepare multiplication of <r_i> and <real data> and randomness of verification
                com.prepare_more_mul_lazy(n_party * n);
                require_mul += n;
                // Prepare private output of <z_i> (offline) and <masked real data> (online)
                for (int party(0); party != n_party; ++party) {
                    com.prepare_more_private_output_lazy(party, 2 * n);
                }
            }
        }

        com.prepare_more_random_now();
        // std::cout << "Random resource prepared." << std::endl;
        com.prepare_more_mul_now();
        // std::cout << "Mul resource prepared." << std::endl;
        com.prepare_more_private_output_now();
        // std::cout << "Private resource prepared." << std::endl;

        malloc_random_resource(com);
        fill_in_random_resource(com); // 2 * n * n_party randoms.
        compute_beta_r(com); // n * n_party mults.

        // std::cout << "Beta r computed." << std::endl;

        compute_permuted_random_resource(com);
        compute_z(com); // 2 * n private output PER party.

        // std::cout << "z computed." << std::endl;

        set_init_flag();
        clear_unused();
        booked_shuffle.clear();
        com.prepare_more_mul_now(require_mul);
        
        com.output_check();
    }

    void verify(mpc_comm &com, ClearType a, ClearType b, ShareType beta, ShareType r)
    {
        ClearType res;
        ShareType val = a * beta - ShareType::constant(b, com.get_my_number(), ShareType::get_mac_key()) - r;
        com.output_immediately(val, res);
        if (res.is_zero() == false) {
            std::cerr << __FUNCTION__ << " : Verification failed." << std::endl;
            throw std::runtime_error("verify : Verification failed.");
        }
        com.output_check();
    }

    void verify(mpc_comm & com, int who, const vectors<ClearType>& a, const vectors<ClearType>& b, ShareType beta, const vectors<ShareType>& r)
    {
        ClearType c(com.rand_int()), w1(0), w2(0); // Challenge.
        std::vector<ClearType> msg;
        if (com.get_my_number() == who) {
            for (auto& v : a) {
                w1 = w1 * c + v;
            }
            for (auto& v : b) {
                w2 = w2 * c + v;
            }
        }
        msg = { c, w1, w2 };
        com.broadcast(who, msg);
        c = msg[0], w1 = msg[1], w2 = msg[2];
        ShareType val = w1 * beta - ShareType::constant(w2, com.get_my_number(), ShareType::get_mac_key());
        ShareType sum_r = r.at(0);
        for (size_t i(1); i != r.size(); ++i) {
            sum_r = c * sum_r + r.at(i);
        }
        ClearType res;
        com.output_immediately(val - sum_r, res);
        com.output_check();
        if (res.is_zero() == false) {
            std::cerr << FAIL_INFO << " : Verification failed." << std::endl;
            throw std::runtime_error("verify : Verification failed.");
        }
    }

    void shuffle_session::set_init_flag()
    {
        cor.initialized = true;
    }

    void shuffle_session::destroy()
    {
        destroyed = true;;
    }

    void shuffle_session::perform(mpc_comm &com, vectors<ShareType> &val)
    {
        if (val.num != (1 << logsz) || val.len != veclen) {
            std::cerr << "shuffle_session::perform : Invalid input size," << val
                    << " != " << (1 << logsz) << " or " << val.len << " != " << veclen << std::endl;
            throw std::runtime_error("shuffle_session::perform : Invalid input size.");
        }
        if (!cor.initialized) {
            std::cerr << FAIL_INFO << "uninitialized shuffle session. Please call process_all_orders first." << std::endl;
            throw std::runtime_error("shuffle_session::perform : uninitialized shuffle session.");
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

        int me = com.get_my_number();
        if (me == 0) {
            // perform permute on z00 and z01
            cor.perm.perform(z00);
            cor.perm.perform(z01);
            z = vectors<ClearType>::cat(z00, z01);
            com.send(1, z);
            for (int who(1); who != com.get_n_party(); ++who) {
                verify(com, who, {}, {}, cor.beta, cor.permuted_rp[who - 1]);
            }
        } else {
            for (int who(1); who != me; ++who) {
                verify(com, who, {}, {}, cor.beta, cor.permuted_rp[who - 1]);
            }
            com.recv(me - 1, z);
            z.split(num, z00, z01);
            verify(com, me, z00, z01, cor.beta, cor.permuted_rp[me - 1]);
            z00 += cor.z[0];
            z01 += cor.z[1];
            cor.perm.perform(z00);
            cor.perm.perform(z01);
            if (me + 1 != com.get_n_party()) {
                z = vectors<ClearType>::cat(z00, z01);
                com.send(me + 1, z);
            }
            for (int who(me + 1); who != com.get_n_party(); ++who) {
                verify(com, who, {}, {}, cor.beta, cor.permuted_rp[who - 1]);
            }
        }
        z = vectors<ClearType>::cat(z00, z01);
        com.broadcast(com.get_n_party() - 1, z);
        z.split(num, z00, z01);

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