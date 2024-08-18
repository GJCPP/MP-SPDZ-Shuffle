#include "my_shuffle.h"

namespace gjcShuffle {
    std::map<shuffle_info, std::vector<order_info>> booked_shuffle;

    shuffle_info::shuffle_info(int _ogsz, int _veclen)
        : logsz(_ogsz), veclen(_veclen)
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
                shuffle_cor &cor = session.cor;
                for (int party(0); party != session.n_party; ++party) {
                    cor.r[party].resize(1 << session.logsz, session.veclen);
                    cor.beta_r[party].resize(1 << session.logsz, session.veclen);
                    cor.rp[party].resize(1 << session.logsz, session.veclen);
                    cor.permuted_r[party].resize(1 << session.logsz, session.veclen);
                    cor.permuted_beta_r[party].resize(1 << session.logsz, session.veclen);
                    cor.permuted_rp[party].resize(1 << session.logsz, session.veclen);
                    cor.z[0].resize(1 << session.logsz, session.veclen);
                    cor.z[1].resize(1 << session.logsz, session.veclen);
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
                vectors<ShareType> r_betar_rp(1 << session.logsz, session.veclen * 3);
                for (int party(0); party != session.n_party; ++party) {
                    for (int i(0); i != (1 << session.logsz); ++i) {
                        for (size_t j(0); j != 3 * session.veclen; j += 3) {
                            r_betar_rp[i][j] = cor.r[party][i][j / 3];
                            r_betar_rp[i][j + 1] = cor.beta_r[party][i][j / 3];
                            r_betar_rp[i][j + 2] = cor.rp[party][i][j / 3];
                        }
                    }
                    session.permute_sessions[party].perform(com, r_betar_rp);
                    for (int i(0); i != 1 << session.logsz; ++i) {
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
        for (auto& info : booked_shuffle) {
            for (auto& order : info.second) {
                shuffle_session &session = *order.session;
                shuffle_cor &cor = session.cor;
                for (int party(1); party != session.n_party; ++party) {
                    com.prepare_more_private_output_lazy(party, (1 << session.logsz) * session.veclen * 2);
                }
            }
        }
        com.private_output_init();
        for (auto& info : booked_shuffle) {
            for (auto& order : info.second) {
                shuffle_session &session = *order.session;
                shuffle_cor &cor = session.cor;
                for (int party(1); party != session.n_party; ++party) {
                    vectors<ShareType> z(1 << session.logsz, session.veclen * 2);
                    for (int i(0); i != 1 << session.logsz; ++i) {
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
                        for (int i(0); i != 1 << session.logsz; ++i) {
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

    void process_all_orders(mpc_comm &com)
    {
        // Book all permute sessions and count the total number of secret random values.
        for (auto &info : booked_shuffle) {
            for (auto &order : info.second) {
                for (int i = 0; i < order.session->n_party; ++i) {
                    song2023::book_permute_session(com, &order.session->permute_sessions[i]);
                }
                // Prepare <beta> and <r_i>, <r^'_i>
                com.prepare_more_random_lazy(2 * (1 << order.session->logsz) * order.session->veclen + 1);
            }
        }
        malloc_random_resource(com);
        fill_in_random_resource(com);
        compute_beta_r(com);
        compute_permuted_random_resource(com);
        compute_z(com);
        clear_unused();
    }

    void shuffle_session::perform(mpc_comm &com, vectors<ShareType> &val)
    {
        if (val.num != (1 << logsz) || val.len != veclen) {
            std::cerr << "shuffle_session::perform : Invalid input size," << val
                    << " != " << (1 << logsz) << " or " << val.len << " != " << veclen << std::endl;
            throw std::runtime_error("shuffle_session::perform : Invalid input size.");
        }
        size_t num = 1 << logsz, len = veclen;
        vectors<ShareType> beta_val(num, len);
        com.mul_init();
        for (auto& v : val) {
            com.mul_append(v, cor.beta);
        }
        com.mul_exchange();
        com.mul_consume(beta_val);
        vectors<ShareType> shared_z00(num, len), shared_z01(num, len);
        shared_z00 = val - cor.r[0];
        shared_z01 = beta_val - cor.beta_r[0] - cor.rp[0];
        vectors<ClearType> z00(num, len), z01(num, len);
        com.prepare_more_private_output_lazy(0, z00.size() + z01.size());
        com.private_output_init();
        com.private_output_append(0, shared_z00);
        com.private_output_append(0, shared_z01);
        com.private_output_exchange();
        com.private_output_consume(0, z00);
        com.private_output_consume(0, z01);

        int me = com.get_my_number();
        if (me == 0) {
            // perform permute on z00 and z01
            cor.perm.perform(z00);
            cor.perm.perform(z01);
            com.send(1, z00);
            com.send(1, z01);
        } else {
            com.recv(me - 1, z00);
            com.recv(me - 1, z01);
            z00 += cor.z[0];
            z01 += cor.z[1];
            cor.perm.perform(z00);
            cor.perm.perform(z01);
            if (me + 1 != com.get_n_party()) {
                com.send(me + 1, z00);
                com.send(me + 1, z01);
            }
        }
        com.unchecked_broadcast(com.get_n_party() - 1, z00);
        com.unchecked_broadcast(com.get_n_party() - 1, z01);
        vectors<ShareType> shared_y(num, len);
        for (size_t i(0); i != num; ++i) {
            for (size_t j(0); j != len; ++j) {
                shared_y[i][j] = ShareType::constant(z00[i][j], me, ShareType::get_mac_key());
            }
        }
        shared_y += cor.permuted_r[com.get_n_party() - 1];
        val = shared_y;
    }
}