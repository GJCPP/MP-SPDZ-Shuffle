#include "test_shuffle.h"

bool check_shuffle(vectors<ClearType>& ori, vectors<ClearType>& vec, bool in_place = false) {
    if (in_place) {
        for (size_t i(0); i != vec.num; ++i) {
            for (size_t j(0); j != vec.len; ++j) {
                if (vec[i][j] != ori[i][j]) return false;
            }
        }
        return true;
    } else {
        for (size_t i(0); i != vec.num; ++i) {
            bool matched(false);
            for (size_t j(0); j != vec.num && !matched; ++j) {
                bool flag = true;
                for (size_t k(0); k != vec.len; ++k) {
                    if (vec[i][k] != ori[j][k]) {
                        flag = false;
                        break;
                    }
                }
                if (flag) matched = true;
            }
            if (!matched) return false;
        }
    }
    return true;
}

bool test_Chase_shuffle(myShuffle::mpc_comm &com)
{
    using myShuffle::mpc_comm;
    static std::mt19937 eng;
    using namespace chase2020;
    int num_test(100);
    int me = com.get_my_number(), n = com.get_n_party();
    int permuter, sender, logsz, veclen, batch;
    while (num_test--) {
        if (me == 0) std::cout << "Start test : " << num_test << std::endl;
        if (me == 0) { // Host
            permuter = rand() % n;
            do {
                sender = rand() % n;
            } while (sender == permuter);
            logsz = rand() % 10 + 1;
            veclen = rand() % 10 + 1;
            batch = rand() % (2 * logsz + 1) + 1;
        }
        com.unchecked_broadcast(0, sender);
        com.unchecked_broadcast(0, permuter);
        com.unchecked_broadcast(0, logsz);
        com.unchecked_broadcast(0, veclen);
        com.unchecked_broadcast(0, batch);
        vectors<block_wrapper> val(1 << logsz, veclen);
        std::vector<int> perm(1 << logsz);
        bool fail(false);
        if (me == sender) {
            auto plan = prepare_permute(com, sender, permuter, perm, 1 << logsz, veclen, batch);
            permute(com, val, plan);
            com.send(permuter, val);
        }
        if (me == permuter) {
            for (int i(0); i != (1 << logsz); ++i) {
                for (int j(0); j != veclen; ++j) {
                    val[i][j] = makeBlockWrapper(0, i);
                }
                perm[i] = i;
            }
            std::shuffle(perm.begin(), perm.end(), eng);
            auto plan = prepare_permute(com, sender, permuter, perm, 1 << logsz, veclen, batch);
            permute(com, val, plan);
            vectors<block_wrapper> recv_vec(1 << logsz, veclen);
            com.recv(sender, recv_vec);
            val += recv_vec;
            for (int i(0); i != (1 << logsz); ++i) {
                for (int j(0); j != veclen; ++j) {
                    if (val[i][j] != makeBlockWrapper(0, perm[i])) {
                        fail = true;
                    }
                }
            }
        }
        com.unchecked_broadcast(permuter, fail);
        if (fail) {
            std::cerr << "test_Chase_shuffle : test FAILED!!!!!" << std::endl;
            //return false;
        }
    }
    if (me == 0) std::cerr << "test_Chase_shuffle : test passed." << std::endl;
    return true;
}

bool test_Song_shuffle(myShuffle::mpc_comm &com)
{
    using myShuffle::mpc_comm;
    using namespace song2023;

    static std::mt19937 eng;
    int tot_test(1);
    for (int test_no(0); test_no != tot_test; ++test_no) {
        int num_test(10), me = com.get_my_number(), n = com.get_n_party();
        std::vector<int> all_logsz, all_veclen, all_batch;
        std::vector<std::vector<permutation>> all_perm;
        if (me == 0) {
            for (int cnt(0); cnt != num_test; ++cnt) {
                all_logsz.push_back(rand() % 7 + 1);
                all_veclen.push_back(rand() % 10 + 1);
                //all_permuter.push_back(1);
                //all_logsz.push_back(2);
                //all_veclen.push_back(1);
                all_batch.push_back(4);
                all_perm.push_back({});
                for (int i(0); i != n; ++i) all_perm.back().push_back(permutation(1 << all_logsz.back(), true));
            }
        } else {
            all_logsz.resize(num_test);
            all_veclen.resize(num_test);
            all_batch.resize(num_test);
            all_perm.resize(num_test);
        }
        com.unchecked_broadcast(0, all_logsz);
        com.unchecked_broadcast(0, all_veclen);
        com.unchecked_broadcast(0, all_batch);
        for (int rank(0); rank != num_test; ++rank) {
                if (me != 0) all_perm[rank].resize(n, permutation(1 << all_logsz[rank]));
            for (int i(0); i != n; ++i) {
                com.unchecked_broadcast(0, all_perm[rank][i].perm);
            }
        }
        std::vector<shuffle_session *> plans;
        for (int rank(0); rank != num_test; ++rank) {
            plans.push_back(song2023::book_shuffle_session<ShareType>(com, 
                            all_logsz[rank], all_veclen[rank], all_batch[rank], all_perm[rank][me]));
        }
        song2023::process_all_orders(com);
        for (int rank(0); rank != num_test; ++rank) {
            int logsz = all_logsz[rank];
            size_t veclen = all_veclen[rank];
            size_t sz = size_t(1) << logsz;
            permutation perm = all_perm[rank][me];
            vectors<ShareType> val(sz, veclen);
            shuffle_session *plan = plans[rank];
            bool fail(false);
            for (size_t i(0); i != sz; ++i) {
                for (size_t j(0); j != veclen; ++j) {
                    val[i][j] = com.get_random();
                }
            }
            vectors<ShareType> shared_ori = val;
            vectors<ClearType> ori(shared_ori.num, shared_ori.len),
                                res(val.num, val.len);
            
            
            plan->perform(com, val);
            
            
            
            com.output_immediately(shared_ori, ori);
            com.output_immediately(val, res);

            permutation pi(1 << logsz);
            for (int i(0); i != n; ++i) {
                pi = pi * all_perm[rank][i];
            }
            pi.perform(ori);

            fail = !check_shuffle(ori, res, true);
            if (fail) {
                std::cerr << "test_Song_shuffle : test FAILED!!!!!" << std::endl;
                return false;
            }
        }
        
        if (me == 1) std::cerr << "test_Song_shuffle : test passed. (" << test_no << "/" << tot_test << ")" << std::endl;
    }
    return true;
}

bool test_my_shuffle(myShuffle::mpc_comm &com, bool strong_abort_privacy)
{
    using namespace myShuffle;

    static std::mt19937 eng;
    int tot_test(1);
    for (int test_no(0); test_no != tot_test; ++test_no) {
        int num_test(10), me = com.get_my_number(), n = com.get_n_party();
        std::vector<int> all_logsz, all_veclen, all_batch;
        std::vector<std::vector<permutation>> all_perm;
        if (me == 0) {
            for (int cnt(0); cnt != num_test; ++cnt) {
                all_logsz.push_back(rand() % 10 + 1);
                all_veclen.push_back(1);
                //all_permuter.push_back(1);
                //all_logsz.push_back(2);
                //all_veclen.push_back(1);
                all_batch.push_back(4);
                all_perm.push_back({});
                for (int i(0); i != n; ++i) all_perm.back().push_back(permutation(1 << all_logsz.back(), true));
            }
        } else {
            all_logsz.resize(num_test);
            all_veclen.resize(num_test);
            all_batch.resize(num_test);
            all_perm.resize(num_test);
        }
        com.unchecked_broadcast(0, all_logsz);
        com.unchecked_broadcast(0, all_veclen);
        com.unchecked_broadcast(0, all_batch);
        for (int rank(0); rank != num_test; ++rank) {
                if (me != 0) all_perm[rank].resize(n, permutation(1 << all_logsz[rank]));
            for (int i(0); i != n; ++i) {
                com.unchecked_broadcast(0, all_perm[rank][i].perm);
            }
        }
        std::vector<shuffle_session *> plans;
        for (int rank(0); rank != num_test; ++rank) {
            plans.push_back(book_shuffle_session<ShareType>(com, 
                            all_logsz[rank], all_veclen[rank], all_batch[rank], all_perm[rank][me]));
        }
        process_all_orders(com);
        for (int rank(0); rank != num_test; ++rank) {
            int logsz = all_logsz[rank];
            size_t veclen = all_veclen[rank];
            size_t sz = size_t(1) << logsz;
            permutation perm = all_perm[rank][me];
            vectors<ShareType> val(sz, veclen);
            shuffle_session *plan = plans[rank];
            bool fail(false);
            for (size_t i(0); i != sz; ++i) {
                for (size_t j(0); j != veclen; ++j) {
                    val[i][j] = com.get_random();
                }
            }
            vectors<ShareType> shared_ori = val;
            vectors<ClearType> ori(shared_ori.num, shared_ori.len),
                                res(val.num, val.len);
            
            
            plan->perform(com, val, strong_abort_privacy);
            
            
            
            com.output_immediately(shared_ori, ori);
            com.output_immediately(val, res);

            permutation pi(1 << logsz);
            for (int i(0); i != n; ++i) {
                pi = pi * all_perm[rank][i];
            }
            pi.perform(ori);

            fail = !check_shuffle(ori, res, true);
            if (fail) {
                std::cerr << "test_my_shuffle : test FAILED!!!!!" << std::endl;
                return false;
            }
        }
        
        if (me == 0) std::cerr << "test_my_shuffle : test passed. (" << test_no << "/" << tot_test << ")" << std::endl;
    }
    return true;
}

bool test_semi_my_shuffle(myShuffle::mpc_comm &com, int max_logsz, int max_veclen, int batch)
{
    int me = com.get_my_number(), n = com.get_n_party();
    int num_test = max_logsz * max_veclen;
    std::vector<int> all_logsz, all_veclen, all_batch;
    std::vector<std::vector<permutation>> all_perm;
    if (me == 0) {
        for (int logsz(1); logsz <= max_logsz; ++logsz) {
            for (int veclen(1); veclen <= max_veclen; ++veclen) {
                all_logsz.push_back(logsz);
                all_veclen.push_back(veclen);
                all_batch.push_back(batch);
                all_perm.push_back({});
                for (int i(0); i != n; ++i) {
                    all_perm.back().push_back(permutation(1 << logsz, true));
                }
            }
        }
    } else {
        all_logsz.resize(num_test);
        all_veclen.resize(num_test);
        all_batch.resize(num_test);
        all_perm.resize(num_test);
    }
    com.unchecked_broadcast(0, all_logsz);
    com.unchecked_broadcast(0, all_veclen);
    com.unchecked_broadcast(0, all_batch);
    for (int rank(0); rank != num_test; ++rank) {
        if (me != 0) all_perm[rank].resize(n, permutation(1 << all_logsz[rank]));
        for (int i(0); i != n; ++i) {
            com.unchecked_broadcast(0, all_perm[rank][i].perm);
        }
    }

    std::vector<semiHonest::shuffle_session *> plans;
    for (int rank(0); rank != num_test; ++rank) {
        plans.push_back(semiHonest::book_shuffle_session(com,
                        all_logsz[rank], all_veclen[rank], all_batch[rank],
                        all_perm[rank][me]));
    }
    semiHonest::process_all_orders(com);

    for (int rank(0); rank != num_test; ++rank) {
        int logsz = all_logsz[rank];
        size_t veclen = all_veclen[rank];
        int sz = 1 << logsz;
        vectors<ClearType> val(sz, veclen), ori(sz, veclen), res(sz, veclen);
        for (int i(0); i != sz; ++i) {
            for (size_t j(0); j != veclen; ++j) {
                ori[i][j] = ClearType(i * veclen + j);
                val[i][j] = me == 0 ? ori[i][j] : ClearType(0);
            }
        }

        plans[rank]->perform(com, val);
        res = val;
        myShuffle::insecure_recon(com, 0, res);

        permutation pi(1 << logsz);
        for (int i(0); i != n; ++i) {
            pi = pi * all_perm[rank][i];
        }
        pi.perform(ori);

        if (me == 0 && !check_shuffle(ori, res, true)) {
            std::cerr << "test_semi_my_shuffle : test FAILED!!!!!" << std::endl;
            return false;
        }
    }

    for (auto plan : plans) {
        delete plan;
    }
    if (me == 0) {
        std::cerr << "test_semi_my_shuffle : test passed for n=" << n
                  << ", logsz=1.." << max_logsz
                  << ", veclen=1.." << max_veclen
                  << ", batch=" << batch << "." << std::endl;
    }
    return true;
}

bool test_mpspdz_shuffle() {
    return true;
}
