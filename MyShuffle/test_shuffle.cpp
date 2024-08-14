#include "test_shuffle.h"

bool test_Chase_shuffle(mpc_comm &com)
{
    static std::mt19937 eng;
    using namespace chase2020;
    int num_test(1000);
    int permuter, sender, logsz, veclen, batch;
    while (num_test--) {
        if (com.me == 1) std::cout << "Start test : " << num_test << std::endl;
        if (com.me == 1) { // Host
            permuter = rand() % com.n + 1;
            do {
                sender = rand() % com.n + 1;
            } while (sender == permuter);
            logsz = rand() % 10 + 1;
            veclen = rand() % 10 + 1;
            batch = rand() % (2 * logsz + 1) + 1;
        }
        com.broadcast(1, sender);
        com.broadcast(1, permuter);
        com.broadcast(1, logsz);
        com.broadcast(1, veclen);
        com.broadcast(1, batch);
        vectors<block_wrapper> val(1 << logsz, veclen);
        std::vector<int> perm(1 << logsz);
        bool fail(false);
        if (com.me == sender) {
            auto plan = prepare_permute(com, sender, permuter, perm, 1 << logsz, veclen, batch);
            permute(com, val, plan);
            com.send(permuter, val);
        }
        if (com.me == permuter) {
            for (int i(0); i != (1 << logsz); ++i) {
                for (int j(0); j != veclen; ++j) {
                    val[i][j] = makeBlock(0, i);
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
                    if (!IS_ZERO_BLOCK(val[i][j] - makeBlockWrapper(0, perm[i]))) {
                        fail = true;
                    }
                }
            }
        }
        com.broadcast(permuter, fail);
        if (fail) {
            std::cerr << "test_Chase_shuffle : test FAILED!!!!!" << std::endl;
            return false;
        }
    }
    if (com.me == 1) std::cerr << "test_Chase_shuffle : test passed." << std::endl;
    return true;
}

bool test_Song_shuffle(mpc_comm &com)
{
    static std::mt19937 eng;
    using namespace song2023;
    int num_test(10);
    std::vector<int> all_permuter, all_logsz, all_veclen, all_batch;
    std::vector<permutation> all_perm;
    if (com.me == 1) {
        for (int cnt(0); cnt != num_test; ++cnt) {
            if (com.me == 1) { // Host
                all_permuter.push_back(rand() % com.n + 1);
                all_logsz.push_back(rand() % 7 + 1);
                all_veclen.push_back(rand() % 10 + 1);
                //all_permuter.push_back(1);
                //all_logsz.push_back(2);
                //all_veclen.push_back(1);
                all_batch.push_back(rand() % (2 * all_logsz.back() + 1) + 1);
                all_perm.push_back(permutation(1 << all_logsz.back(), true));
            }
        }
    } else {
        all_permuter.resize(num_test);
        all_logsz.resize(num_test);
        all_veclen.resize(num_test);
        all_batch.resize(num_test);
        all_perm.resize(num_test);
    }
    com.broadcast(1, all_permuter);
    com.broadcast(1, all_logsz);
    com.broadcast(1, all_veclen);
    com.broadcast(1, all_batch);
    for (int rank(0); rank != num_test; ++rank) {
        if (com.me != 1) all_perm[rank] = permutation(1 << all_logsz[rank]);
        com.broadcast(1, all_perm[rank].perm);
    }
    std::vector<permute_session *> plans;
    for (int rank(0); rank != num_test; ++rank) {
        plans.push_back(book_permute_session(com, all_permuter[rank], 
                        all_logsz[rank], all_veclen[rank], all_batch[rank], all_perm[rank]));
    }
    process_all_orders(com);
    for (int rank(0); rank != num_test; ++rank) {
        int permuter = all_permuter[rank], logsz = all_logsz[rank], veclen = all_veclen[rank];
        int sz = 1 << logsz;
        permutation perm = all_perm[rank];
        vectors<block_wrapper> val(sz, veclen), ans(sz, veclen);
        permute_session *plan = plans[rank];
        bool fail(false);
        if (com.me != permuter) {
            insecure_share(com, permuter, val);
            plan->perform(com, val);
            delete plan;
            insecure_recon(com, permuter, val);
        } else {
            emp::PRG prg;
            prg.random_block(reinterpret_cast<emp::block *>(val.data()), sz * veclen);
            for (int i(0); i != sz; ++i) {
                for (int j(0); j != veclen; ++j) {
                    ans[i][j] = val[plan->perm[i]][j];
                }
            }
            insecure_share(com, permuter, val);
            plan->perform(com, val);
            delete plan;
            insecure_recon(com, permuter, val);
            // for (int i(0); i != sz; ++i) std::cout << val[i][0] << std::endl;
            for (int i(0); i != sz; ++i) {
                for (int j(0); j != veclen; ++j) {
                    if (!IS_ZERO_BLOCK(val[i][j] - ans[i][j])) {
                        fail = true;
                    }
                }
            }
        }
        com.broadcast(permuter, fail);
        if (fail) {
            std::cerr << "test_Song_shuffle : test FAILED!!!!!" << std::endl;
            return false;
        }
    }
    
    if (com.me == 1) std::cerr << "test_Song_shuffle : test passed." << std::endl;
    return true;
}
