#include "unit_test.h"

namespace gjcShuffle {
    bool test_com(mpc_comm &com)
    {
        const int n(100);
        int my_number = com.get_my_number();
        vectors<ClearType> arr(n, n), rec(n, n);
        size_t sz = arr.size();
        arr.at(0) = arr.at(1) = 1;
        for (size_t i(2); i != sz; ++i) {
            arr.at(i) = arr.at(i - 1) + arr.at(i - 2);
        }
        bool failed = false;
        for (int i(0); i != com.get_n_party() && !failed; ++i) {
            for (int j(0); j != com.get_n_party() && !failed; ++j) {
                if (i == j) continue;
                if (my_number == i) {
                    com.send(j, arr);
                }
                if (my_number == j) {
                    com.recv(i, rec);
                    for (size_t k(0); k != sz; ++k) {
                        if (rec.at(k) != arr.at(k)) {
                            failed = true;
                            std::cout << "Error at " << k << ": " << rec.at(k) << " != " << arr.at(k) << std::endl;
                            break;
                        }
                    }
                    rec.clear();
                    rec.resize(n, n);
                }
                com.unchecked_broadcast(j, failed);
            }
        }
        if (failed) {
            if (my_number == 0) std::cout << "test_com failed." << std::endl;
            return false;
        } else {
            if (my_number == 0) std::cout << "test_com passed." << std::endl;
            return true;
        }
    }

    template <typename T>
    bool test_broadcast(mpc_comm& com, T special1, T special2) {
        int me = com.get_my_number();
        T val = {};
        if (me == 0) val = special1;
        com.unchecked_broadcast(0, val);
        if (val != special1) {
            return false;
        }

        if (me == 0) val = special2;
        com.unchecked_broadcast(0, val);
        if (val != special2) {
            return false;
        }
        return true;
    }

    bool test_broadcast(mpc_comm& com) {
        bool failed = false;
        if (!test_broadcast<bool>(com, true, false)) {
            if (com.get_my_number() == 0) std::cout << "test_broadcast<bool> failed." << std::endl;
            failed = true;
        }
        if (!test_broadcast<int>(com, 123, 456)) {
            if (com.get_my_number() == 0) std::cout << "test_broadcast<int> failed." << std::endl;
            failed = true;
        }
        if (!test_broadcast<ClearType>(com, 123, 456)) {
            if (com.get_my_number() == 0) std::cout << "test_broadcast<ClearType> failed." << std::endl;
            failed = true;
        }
        if (!test_broadcast<std::string>(com, "Hello", "World")) {
            if (com.get_my_number() == 0) std::cout << "test_broadcast<std::string> failed." << std::endl;
            failed = true;
        }
        return !failed;
    }

    bool test_ote(mpc_comm& com) {
        int my_number = com.get_my_number();
        const int numOTs = 1000, numTests(10);
        bool failed = false;
        if (my_number == 0) {
            for (int cnt(0); cnt != numTests && !failed; ++cnt) {
                std::vector<std::array<block_wrapper, 2>> send_msg(numOTs);
                for (int i = 0; i < numOTs; i++) {
                    send_msg[i][0] = makeBlockWrapper(0, 0);
                    send_msg[i][1] = makeBlockWrapper(0xffffffffffffffff, 0xffffffffffffffff);
                }
                com.send_ext_ot(1, send_msg);
                com.send(1, send_msg.data(), numOTs * 2 * sizeof(block_wrapper));
                com.recv<bool>(1, failed);
            }
        }
        if (my_number == 1) {
            osuCrypto::PRNG prg(osuCrypto::block(1235123,3456123));
            for (int cnt(0); cnt != numTests && !failed; ++cnt) {
                std::vector<std::array<block_wrapper, 2>> send_msg(numOTs);
                std::vector<block_wrapper> recv_msg(numOTs);
                osuCrypto::BitVector choices(numOTs);
                choices.randomize(prg);
                com.recv_ext_ot(0, choices, recv_msg);
                com.recv(0, send_msg.data(), numOTs * 2 * sizeof(block_wrapper));
                for (int i = 0; i < numOTs; i++) {
                        std::cout << "Send " << i << ": " << send_msg[i][0] << " " << send_msg[i][1] << std::endl;
                    std::cout << "Recv " << i << ": " << choices[i] << " " << recv_msg[i];
                    if (recv_msg[i] != send_msg[i][choices[i]]) {
                        std::cout << ", Error!";
                        failed = true;
                        break;
                    }
                    std::cout << std::endl;
                }
                com.send<bool>(0, failed);
            }
        }
        com.unchecked_broadcast(0, failed);
        if (failed) {
            std::cout << "test_ote failed." << std::endl;
            return false;
        } else {
            std::cout << "test_ote passed." << std::endl;
            return true;
        }
    }

    bool test_opv(mpc_comm& com) {
        int me = com.get_my_number();
        std::vector<int> logsz = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 };
        size_t test_num = logsz.size();
        bool failed = false;
        if (me == 0) {
            std::vector<prg_seed> msg0, msg1;
            std::vector<block_wrapper> hash_val;
            std::vector<opv_2n> opvs;
            for (size_t i(0); i != test_num; ++i) {
                sender_append_OPV(msg0, msg1, hash_val, logsz[i], opvs);
            }
            com.send_ext_ot(1, msg0, msg1);
            com.send(1, hash_val.data(), hash_val.size() * sizeof(block_wrapper));
            for (size_t i(0); i != test_num; ++i) {
                com.send(1, opvs[i].data);
            }
        }
        if (me == 1) {
            std::vector<int> pos;
            std::vector<opv_2n> opvs;
            osuCrypto::BitVector choose;
            std::vector<block_wrapper> hash_val, recv_msg;
            std::vector<std::vector<block_wrapper>> ori_msg;
            for (size_t i(0); i != test_num; ++i) {
                pos.push_back(rand() % (1 << logsz[i]));
                receiver_append_OPV(choose, hash_val, logsz[i], pos.back());
            }
            recv_msg.resize(choose.size());
            com.recv_ext_ot(0, choose, recv_msg);
            com.recv(0, hash_val.data(), hash_val.size() * sizeof(block_wrapper));
            prg_seed* next_msg = recv_msg.data();
            block_wrapper* next_hash = hash_val.data();
            for (size_t i(0); i != test_num; ++i) {
                opvs.push_back(opv_2n(logsz[i], pos[i], next_msg, next_hash));
            }
            for (size_t i(0); i != test_num; ++i) {
                ori_msg.push_back(std::vector<block_wrapper>(opvs[i].data.size()));
                com.recv(0, ori_msg[i].data(), ori_msg[i].size() * sizeof(block_wrapper));
            }
            for (size_t i(0); i != test_num; ++i) {
                for (size_t j(0); j != opvs[i].data.size(); ++j) {
                    if (opvs[i][j].is_nonzero() && j == opvs[i].pos) {
                        std::cout << "test_opv : Error at " << i << " " << j << ", should be oblivious." << std::endl;
                        failed = true;
                    }
                    if (j != opvs[i].pos && opvs[i][j] != ori_msg[i][j]) {
                        std::cout << "test_opv : Error at " << i << " " << j << ", value incorrect." << std::endl;
                        failed = true;
                    }
                }
            }
        }
        com.unchecked_broadcast(1, failed);
        if (me == 0) {
            if (failed) {
                std::cout << "test_opv failed." << std::endl;
            } else {
                std::cout << "test_opv passed." << std::endl;
            }
        }
        return !failed;
    }

    bool test_all(mpc_comm& com)
    {
        bool failed = false;
        if (!failed) failed |= test_com(com);
        if (!failed) failed |= test_broadcast(com);
        if (!failed) failed |= test_ote(com);
        if (failed) {
            std::cout << "Some tests failed." << std::endl;
        } else {
            std::cout << "All tests passed." << std::endl;
        }
        return failed;
    }
}