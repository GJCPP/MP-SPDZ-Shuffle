#include "Song_shuffle.h"
#include "my_timer.h"

namespace song2023 {
    using namespace myShuffle;
    std::map<permute_info, std::vector<order_info>> booked_permute;
    std::map<int, size_t> count_permute_task;
    std::vector<permute_session *> booked_sessions;

    /*
    *  This is the bucket size table by Song et al, specified for security parameter lambda = 40.
    *  bucket_size[logsz - 4][log  batch size - 4] is the size of each bucket.
    *  
    *  For example, bucket_size[0][1] = 24, which means that when
    *       - logsz = 4, i.e. the size of permutation is 2^4 = 16,
    *       - log batch size = 5, i.e. there are in total 2^5 = 32 permutation task (of logsize 4),
    *  then each bucket contains 24 permutation tasks, which is in total 24 * 32 = 768 permutation tasks.
    * 
    *  C.f. Table V of the paper "Secret-Shared Shuffle with Malicious Security" by Sont et al.
    */
    const int bucket_size[7][17] = {
        {27, 24, 21, 20, 19, 18, 17, 16, 15, 15, 15, 14, 14, 14, 14, 13, 13},
        {25, 22, 19, 18, 16, 15, 15, 13, 13, 13, 12, 12, 12, 12, 12, 11, 11},
        {23, 20, 18, 17, 15, 14, 14, 13, 12, 12, 12, 11, 11, 11, 11, 10, 10},
        {22, 19, 17, 16, 14, 13, 13, 12, 11, 11, 11, 10, 10, 10, 10,  9,  9},
        {21, 18, 16, 14, 13, 12, 12, 11, 10, 10, 10,  9,  9,  9,  9,  8,  8},
        {21, 18, 16, 14, 13, 12, 12, 11, 10, 10, 10,  9,  9,  9,  9,  8,  8},
        {20, 17, 15, 13, 12, 11, 10, 10,  9,  9,  9,  8,  8,  8,  8,  7,  7}
    };

    
    int get_bucket_size(int logsz, int batch_sz)
    {
        int log_batch_sz = math_gadget::log2(batch_sz);
        if (logsz < 4 || log_batch_sz < 4 || logsz > 11 || log_batch_sz > 20) {
            return DEFAULT_BUCKET_SIZE;
        }
        return bucket_size[logsz - 4][log_batch_sz - 4];
    }

    permute_info::permute_info(int _permuter, int _logsz)
        : permuter(_permuter), logsz(_logsz)
    {
    }

    bool permute_info::operator<(const permute_info &info) const
    {
        if (permuter != info.permuter) return permuter < info.permuter;
        return logsz < info.logsz;
    }

    /*
    *   Decompose a permutation task into smaller tasks and record the resource required.
    *   Note that only after all sessions are booked can bucket size be determined.
    */
    void book_permute_session(mpc_comm &com, permute_session *session)
    {
        int me = com.get_my_number(), permuter = session->permuter,
            logsz = session->logsz, batch = session->batch;
        int sz = 1 << logsz;
        std::vector<int> dest(sz);
		if (me == permuter) {
			for (int i(0); i != sz; ++i) dest[session->perm[i]] = i;
		} else {
			for (int i(0); i != sz; ++i) dest[i] = i;
		}
		const auto& all_tasks = BenesNetwork::task_decompose(logsz, batch);
		auto route = BenesNetwork::route(logsz, dest);
		auto sub_route = BenesNetwork::decompose(route, batch);
		BenesNetwork::desttask_to_permtask(sz, sub_route);
			for (size_t bat(0); bat != sub_route.size(); ++bat) {
				const auto& task = all_tasks[bat];
	            size_t batch_sz(task[0].size());
				int log_batch_sz = math_gadget::log2(batch_sz);
	            count_permute_task[log_batch_sz] += task.size();
			}
        booked_sessions.push_back(session);
        // One random mask for the batched post-execution MAC check. Public
        // coefficients are derived from a committed joint seed after all
        // intermediate states have been fixed.
        com.prepare_more_random_lazy(1);
    }

    void decompose_permute_sessions(mpc_comm &com)
    {
        for (auto session : booked_sessions) {
            int me = com.get_my_number(), permuter = session->permuter,
                logsz = session->logsz, batch = session->batch;
            int sz = 1 << logsz;
            std::vector<int> dest(sz);
            if (me == permuter) {
                for (int i(0); i != sz; ++i) dest[session->perm[i]] = i;
            } else {
                for (int i(0); i != sz; ++i) dest[i] = i;
            }
            const auto& all_tasks = BenesNetwork::task_decompose(logsz, batch);
            auto route = BenesNetwork::route(logsz, dest);
            auto sub_route = BenesNetwork::decompose(route, batch);
            BenesNetwork::desttask_to_permtask(sz, sub_route);
            std::vector<int> map(sz), inv_map(sz);
            for (size_t bat(0); bat != sub_route.size(); ++bat) {
                const auto& task = all_tasks[bat];
                int next(0), batch_sz(task[0].size());
                int log_batch_sz = math_gadget::log2(batch_sz);
                for (const auto& touched : task) {
                    next = 0;
                    for (int v : touched) { // touched is the component specified by (depth, rank of task)
                        inv_map[next] = v; // map the position back.
                        map[v] = next++;
                    }
                    int bucket_size = get_bucket_size(log_batch_sz, count_permute_task[log_batch_sz]);
                    session->bucket_size[log_batch_sz] = bucket_size;
                    if (me == permuter) {
                        permutation small_perm(batch_sz);
                        for (int v : touched) {
                            small_perm[map[v]] = map[sub_route[bat][v]];
                        }
                        for (int cnt(1); cnt != bucket_size; ++cnt) {
                            // Create extra permutation for statistical security.
                            permutation extra_perm(batch_sz, true);
                            small_perm = extra_perm.inverse() * small_perm;
                            booked_permute[{permuter, log_batch_sz}].push_back({extra_perm, session});
                        }
                        booked_permute[{permuter, log_batch_sz}].push_back({small_perm, session});
                    } else {
                        for (int cnt(0); cnt != bucket_size; ++cnt) {
                            booked_permute[{permuter, log_batch_sz}].push_back({{}, session});
                        }
                    }
                }
            }
        }
        booked_sessions.clear();
    }

    void book_shuffle_session(mpc_comm &com, shuffle_session *session)
    {
        int n = com.get_n_party();
        for (int i(0); i != n; ++i) {
            book_permute_session(com, &session->permute_sessions[i]);
        }
    }

    void shuffle_session::destroy()
    {
        destroyed = true;
    }

    const permutation & shuffle_session::get_perm(int who) const
    {
        return permute_sessions[who].get_perm();
    }

    permute_pair pair_from_opvm(size_t veclen, const vectors<block_wrapper> &opvm, const permutation &perm, bool oblivious)
    {
        size_t sz = opvm.num;
        permute_pair pair; // a is the sum along column, b is the negative sum along row.
        if (oblivious) {
            permutation inv = perm.inverse();
            pair.perm = perm;
            pair.delta.resize(sz, veclen);
            for (size_t i(0); i != sz; ++i) {
                for (size_t j(0); j != sz; ++j) {
                    if (perm[i] == j) continue;
                    if (veclen > 1) { // Extend seed to veclen.
                        vectors<ClearType> val(1, veclen);
                        arbitrary_prg(opvm[i][j], val);
                        for (size_t k(0); k != veclen; ++k) {
                            pair.delta[i][k] -= val.at(k);
                            pair.delta[inv[j]][k] += val.at(k);
                        }
                    }
                }
            }
        } else {
            pair.perm = permutation(sz);
            pair.a.resize(sz, veclen);
            pair.b.resize(sz, veclen);
            for (size_t i(0); i != sz; ++i) {
                for (size_t j(0); j != sz; ++j) {
                    if (veclen > 1) { // Extend seed to veclen.
                        vectors<ClearType> val(1, veclen);
                        arbitrary_prg(opvm[i][j], val);
                        for (size_t k(0); k != veclen; ++k) {
                            pair.a[j][k] += val.at(k);
                            pair.b[i][k] -= val.at(k);
                        }
                    }
                }
            }
        }
        return pair;
    }

    void append_left_right_opvm(const vectors<block_wrapper> &opvm, std::vector<vectors<block_wrapper>> &left_opvms,
            std::vector<vectors<block_wrapper>> &right_opvms)
    {
        int sz = opvm.num;
        vectors<block_wrapper> left_opvm(sz, sz), right_opvm(sz, sz);
        for (int i(0); i != sz; ++i) {
            for (int j(0); j != sz; ++j) {
                auto expand = double_length_prg(opvm[i][j]);
                left_opvm[i][j] = expand[0];
                right_opvm[i][j] = expand[1];
            }
        }
        left_opvms.push_back(left_opvm);
        right_opvms.push_back(right_opvm);
    }

    void put_opvm_into_hash(Hash& hash, const vectors<block_wrapper>& opvm)
    {
        int sz = opvm.num;
        for (int i(0); i != sz; ++i) {
            for (int j(0); j != sz; ++j) {
                hash.update(&opvm[i][j], sizeof(block_wrapper));
            }
        }
    }

    void process_orders(mpc_comm &com,
        const permute_info &info,
        int sender,
        const std::vector<order_info> &orders,
        std::vector<permute_pair>& output)
    {
        // Each order will produce a permute_pair as output.
        int me = com.get_my_number();
        int permuter = info.permuter, logsz = info.logsz;
        size_t sz = size_t(1) << logsz;

        osuCrypto::BitVector choose;
        std::vector<block_wrapper> msg0, msg1, hash_val, col_sum;
        std::vector<opv_2n> opvs;
        std::vector<vectors<block_wrapper>> left_opvms, right_opvms;
        Hash left_hash; // Compute hash value of all left_opvm.
        for (const order_info &order : orders) {
            if (me == permuter && order.perm.n != sz) {
                std::cerr << "process_orders : size mismatch, " << order.perm.n << " != " << sz << std::endl;
                throw std::runtime_error("process_orders : size mismatch.");
            }
            vectors<block_wrapper> opvm(sz, sz);
            for (size_t i(0); i != sz; ++i) {
                if (me == permuter) {
                    // Permuter acts as OPV receiver
                    receiver_append_OPV(choose, hash_val, logsz, order.perm[i]);
                } else {
                    sender_append_OPV(msg0, msg1, hash_val, logsz, opvs);
                    for (size_t j(0); j != sz; ++j) opvm[i][j] = opvs.back()[j];
                }
            }
            if (me == sender) {
                append_left_right_opvm(opvm, left_opvms, right_opvms);
                // Sender computes the sum of each column.
                const vectors<block_wrapper>& left_opvm = left_opvms.back();
                for (size_t j(0); j != sz; ++j) { // j : row-th number
                    block_wrapper sum = {};
                    for (size_t i(0); i != sz; ++i) {
                        sum = sum + left_opvm[i][j];
                    }
                    col_sum.push_back(sum);
                }
                put_opvm_into_hash(left_hash, left_opvm);
            } else {
                // Permuter prepares to receive column sum.
                col_sum.resize(col_sum.size() + sz);
            }
        }
        size_t num_ot = me == permuter ? choose.size() : msg0.size();
        std::vector<block_wrapper> opv_msg(num_ot);
        block_wrapper *next_opv_msg = nullptr, *next_hash_msg = nullptr;
        if (me == permuter) {
            com.ext_ot_recv(sender, choose, opv_msg);
            com.recv(sender, hash_val);
            com.recv(sender, col_sum);
            next_opv_msg = opv_msg.data();
            next_hash_msg = hash_val.data();
        } else {
            com.ext_ot_send(permuter, msg0, msg1);
            com.send(permuter, hash_val);
            com.send(permuter, col_sum);
        }
        auto next_right_opvm = right_opvms.begin();
        auto next_col_sum = col_sum.begin();
        for (const order_info &order : orders) {
            // Reconstruct OPVM
            vectors<block_wrapper> opvm_seed(sz, sz);
            vectors<block_wrapper> left_opvm(sz, sz), right_opvm(sz, sz);
            if (me == permuter) {
                // Permuter reconstruct OPVM
                for (size_t i(0); i != sz; ++i) {
                    opv_2n opv;
                    opv = opv_2n(logsz, order.perm[i], next_opv_msg, next_hash_msg);

                    for (size_t j(0); j != sz; ++j) {
#ifdef DEBUG
                        if (order.perm[i] == j && opv[j].is_nonzero()) {
                            std::cerr << "process_orders : unexpected non-zero block at oblivious position." << std::endl;
                            throw std::runtime_error("process_orders : unexpected non-zero block at oblivious position.");
                        }
#endif
                        opvm_seed[i][j] = opv[j];
                        if (order.perm[i] != j) {
                            auto expand = double_length_prg(opvm_seed[i][j]);
                            left_opvm[i][j] = expand[0];
                            right_opvm[i][j] = expand[1];
                            next_col_sum[j] -= left_opvm[i][j];
                        }
                    }
                }
                // Recover the entire left_opvm.
                for (size_t i(0); i != sz; ++i) {
                    left_opvm[i][order.perm[i]] = next_col_sum[order.perm[i]];
                }
                put_opvm_into_hash(left_hash, left_opvm);
                next_col_sum += sz;
            } else {
                // Sender fetches next opvm.
                right_opvm = *next_right_opvm++;
            }
            // Use left OPVM to check integrity.

            // Use right OPVM to build the permute_pair.
            output.push_back(permute_pair(order.perm, right_opvm));
        }
        // Permuter reports hash value of left_opvm.
        if (me == permuter) {
            block_wrapper hash_val[BLOCKS_FOR_HASH];
            left_hash.final(reinterpret_cast<octet *>(hash_val), sizeof(hash_val));
            com.send(sender, hash_val, sizeof(hash_val));
        } else {
            block_wrapper hash_val[BLOCKS_FOR_HASH], correct_val[BLOCKS_FOR_HASH];
            left_hash.final(reinterpret_cast<octet *>(correct_val), sizeof(correct_val));
            com.recv(permuter, hash_val, sizeof(hash_val));
            for (int i(0); i != BLOCKS_FOR_HASH; ++i) {
                if (hash_val[i] != correct_val[i]) {
                    std::cerr << "process_orders : Left OPVM hash value incorrect." << std::endl;
                    throw std::runtime_error("process_orders : Left OPVM hash value incorrect.");
                }
            }
        }
    }

    void process_all_orders(mpc_comm &com)
    {
        for (auto& keyval : booked_permute) {
            for (auto& session : keyval.second) {
                if (session.session->destroyed) {
                    std::cerr << FAIL_INFO << "Session destroyed." << std::endl;
                    throw std::runtime_error("process_all_orders : Session destroyed.");
                }
            }
        }
        size_t sequential_offline_rounds = 0;
        size_t parallel_offline_rounds = 0;
        auto add_parallel_task_rounds = [&](size_t before) {
            size_t after = com.count_total_rounds();
            size_t delta = after - before;
            sequential_offline_rounds += delta;
            parallel_offline_rounds = std::max(parallel_offline_rounds, delta);
        };
        size_t rounds_before = com.count_total_rounds();
        com.prepare_more_random_now();
        add_parallel_task_rounds(rounds_before);
        decompose_permute_sessions(com);
        int me = com.get_my_number(), n = com.get_n_party();
        for (auto keyval : booked_permute) {
            permute_info info = keyval.first;
            std::vector<order_info> &orders = keyval.second;
            if (me == info.permuter) {
                for (int sender(0); sender != n; ++sender) {
                    if (me == sender) continue;
                    rounds_before = com.count_total_rounds();

                    // Select a random permutation to hide the order of the orders.
                    permutation random_perm(orders.size(), true);
                    std::vector<order_info> prac_orders(orders.size()); // The practical order of orders.
                    for (size_t i(0); i != orders.size(); ++i) {
                        prac_orders[random_perm[i]] = orders[i]; // If random_perm[i] == 0, then orders[i] is the first to be processed.
                    }

                    std::vector<permute_pair> output, reordered_output;
                    process_orders(com, info, sender, prac_orders, output);

                    // Reveal the random_perm to the sender.
                    com.send(sender, random_perm.perm);

                    for (size_t i(0); i != output.size(); ++i) {
                        reordered_output.push_back(output[random_perm[i]]);
                    }

                    auto next_output(reordered_output.begin());
                    for (order_info &order : orders) {
                        next_output->expand(order.session->veclen, true);
                        order.session->pairs[sender][info.logsz].push_back(*next_output++);
                    }
                    add_parallel_task_rounds(rounds_before);
                }
            } else {
                rounds_before = com.count_total_rounds();
                std::vector<permute_pair> output, reordered_output;
                process_orders(com, info, me, orders, output);

                permutation perm(orders.size());
                com.recv(info.permuter, perm.perm);

                for (size_t i(0); i != output.size(); ++i) {
                    reordered_output.push_back(output[perm[i]]);
                }

                auto next_output(reordered_output.begin());
                for (order_info &order : orders) {
                    next_output->expand(order.session->veclen, false);
                    order.session->pairs[info.permuter][info.logsz].push_back(*next_output++);
                }
                add_parallel_task_rounds(rounds_before);
            }
        }
        if (sequential_offline_rounds > parallel_offline_rounds) {
            com.add_round_adjustment(-static_cast<long long>(
                    sequential_offline_rounds - parallel_offline_rounds));
        }
        for (auto& keyval : booked_permute) {
            for (auto& session : keyval.second) {
                session.session->initialized = true;
            }
        }

        booked_permute.clear();
        count_permute_task.clear();
    }

    order_info::order_info(const permutation &_perm, permute_session *_session)
        : perm(_perm), session(_session)
    {
    }

    void permute_session::destroy()
    {
        destroyed = true;
    }

    void permute_session::perform(mpc_comm &com, vectors<ClearType> &val,
            vectors<ClearType>* checked_intermediates)
    {
        int n = com.get_n_party(), me = com.get_my_number();
        size_t sz = (1 << logsz);

        if (sz != val.num || veclen != val.len) {
            std::cerr << "permute_session::perform : size mismatch, " 
                << sz << " != " << val.num << " or " << veclen << " != " << val.len << std::endl;
            throw std::runtime_error("permute_session::perform : size mismatch.");
        }
        if (!initialized) {
            std::cerr << FAIL_INFO << "Not initialized. Please call function process_all_orders." << std::endl;
            throw std::runtime_error("permute_session::perform : Not initialized.");
        }
        if (destroyed) {
            std::cerr << FAIL_INFO << "permute session destroyed. Do not use one session twice." << std::endl;
            throw std::runtime_error("permute_session::perform : permute session destroyed.");
        }


        static std::vector<int> dest; dest.resize(sz);
		if (me == permuter) {
			for (size_t i(0); i != sz; ++i) dest[perm[i]] = i;
		} else {
			for (size_t i(0); i != sz; ++i) dest[i] = i;
		}
		const auto& all_tasks = BenesNetwork::task_decompose(logsz, batch);
		auto route = BenesNetwork::route(logsz, dest);
		auto sub_route = BenesNetwork::decompose(route, batch);
		BenesNetwork::desttask_to_permtask(sz, sub_route);

		static std::vector<int> map, inv_map;
        map.resize(sz); inv_map.resize(sz);

        if (checked_intermediates != nullptr) {
            size_t check_entries = 0;
            for (size_t bat(0); bat != sub_route.size(); ++bat) {
                const auto& task = all_tasks[bat];
                const int log_batch_sz = math_gadget::log2(task[0].size());
                const size_t rep = bucket_size[log_batch_sz];
                for (const auto& touched : task) {
                    check_entries += rep * touched.size();
                }
            }
            checked_intermediates->resize(check_entries, veclen);
        }

        // The permuter's original local share also passes through every
        // sub-permutation. Record its contribution to each authenticated
        // intermediate state before adding the receiver-side contributions
        // associated with the other parties.
        if (checked_intermediates != nullptr && me == permuter) {
            if (n < 2) {
                throw std::runtime_error("permute_session::perform : At least two parties required.");
            }
            const int first_sender = permuter == 0 ? 1 : 0;
            static std::vector<permute_pair>::iterator base_next_pair[MAX_BATCH_SIZE];
            for (int i(0); i != MAX_BATCH_SIZE; ++i) {
                base_next_pair[i] = pairs[first_sender][i].begin();
            }
            vectors<ClearType> base_val(val), local_val, temp_val;
            size_t check_pos = 0;
            for (size_t bat(0); bat != sub_route.size(); ++bat) {
                const auto& task = all_tasks[bat];
                for (const auto& touched : task) {
                    const int batch_sz = task[0].size();
                    const int log_batch_sz = math_gadget::log2(batch_sz);
                    int next = 0;
                    for (int v : touched) {
                        inv_map[next] = v;
                        map[v] = next++;
                    }
                    local_val.resize(batch_sz, veclen);
                    temp_val.resize(batch_sz, veclen);
                    for (int v : touched) {
                        for (size_t j(0); j != veclen; ++j) {
                            local_val[map[v]][j] = base_val[v][j];
                        }
                    }
                    const int rep = bucket_size[log_batch_sz];
                    for (int cnt(0); cnt != rep; ++cnt) {
                        if (base_next_pair[log_batch_sz]
                                == pairs[first_sender][log_batch_sz].end()) {
                            throw std::runtime_error(
                                    "permute_session::perform : unexpected end of base permute_pair.");
                        }
                        const permutation& small_perm = base_next_pair[log_batch_sz]->perm;
                        for (int i(0); i != batch_sz; ++i) {
                            for (size_t j(0); j != veclen; ++j) {
                                temp_val[i][j] = local_val[small_perm[i]][j];
                            }
                        }
                        local_val = temp_val;
                        for (int i(0); i != batch_sz; ++i) {
                            for (size_t j(0); j != veclen; ++j) {
                                (*checked_intermediates)[check_pos + i][j] = local_val[i][j];
                            }
                        }
                        check_pos += batch_sz;
                        ++base_next_pair[log_batch_sz];
                    }
                    for (int v : touched) {
                        for (size_t j(0); j != veclen; ++j) {
                            base_val[v][j] = local_val[map[v]][j];
                        }
                    }
                }
            }
        }
        // permute_pair * next_pair[MAX_BATCH_SIZE];
        // for (int bat(0); bat != MAX_BATCH_SIZE; ++bat) {
        //     if (me == permuter) next_pair[i] = &pairs[bat][0][0];
        //     else next_pair[i] = &pairs[me][i][i];
        // }
        static vectors<ClearType> result; result.resize(val.num, val.len);
        for (size_t i(0); i != val.num; ++i) {
            for (size_t j(0); j != val.len; ++j) {
                result[i][j] = val[perm[i]][j];
            }
        }

        size_t sequential_sender_rounds = 0;
        size_t parallel_sender_rounds = 0;
        for (int sender(0); sender != n; ++sender) {
            if (sender == permuter) continue;
            if (me != permuter && me != sender) continue;
            size_t rounds_before = com.count_total_rounds();
            static std::vector<permute_pair>::iterator next_pair[MAX_BATCH_SIZE];
            vectors<ClearType> dum_val(val.num, val.len); // Permuter only permutes dummy values.

            if (me == permuter) {
                for (int i(0); i != MAX_BATCH_SIZE; ++i) {
                    next_pair[i] = pairs[sender][i].begin();
                }
            } else {
                for (int i(0); i != MAX_BATCH_SIZE; ++i) {
                    next_pair[i] = pairs[permuter][i].begin();
                }
            }
            size_t sub_route_size = sub_route.size();
            size_t check_pos = 0;
            for (size_t bat(0); bat != sub_route_size; ++bat) {
                const auto& task = all_tasks[bat];
                int next(0), batch_sz(task[0].size());
                int log_batch_sz = math_gadget::log2(batch_sz);
                size_t sequential_component_rounds = 0;
                size_t parallel_component_rounds = 0;
                for (const auto& touched : task) {
                    size_t component_rounds_before = com.count_total_rounds();
                    next = 0;

                    for (int v : touched) { // touched is the component specified by (depth, rank of task)
                        inv_map[next] = v; // map the position back.
                        map[v] = next++;
                    }

                    static vectors<ClearType> local_val, recv_val, temp_val;
                    local_val.resize(batch_sz, veclen); recv_val.resize(batch_sz, veclen); temp_val.resize(batch_sz, veclen);

                    for (int v : touched) {
                        for (size_t j(0); j != veclen; ++j) {
                            if (me == permuter) local_val[map[v]][j] = dum_val[v][j];
                            else local_val[map[v]][j] = val[v][j];
                        }
                    }

                    int rep = bucket_size[log_batch_sz];
                    if (me == permuter) { // Receiver of OT
                        for (int cnt(0); cnt != rep; ++cnt) {
                            // Create extra permutation for statistical security.
                            if (next_pair[log_batch_sz] == pairs[sender][log_batch_sz].end()) {
                                std::cerr << "permute_session::perform : unexpected end of permute_pair." << std::endl;
                                throw std::runtime_error("permute_session::perform : unexpected end of permute_pair.");
                            }
                            const permutation& small_perm = next_pair[log_batch_sz]->perm;

                            com.recv(sender, recv_val);


                            for (int i(0); i != batch_sz; ++i) {
                                for (size_t j(0); j != veclen; ++j) {
                                    temp_val[i][j] = recv_val[small_perm[i]][j] +
                                                    local_val[small_perm[i]][j] -
                                                    next_pair[log_batch_sz]->delta[i][j];
                                }
                            }

                            local_val = temp_val;
                            if (checked_intermediates != nullptr) {
                                for (int i(0); i != batch_sz; ++i) {
                                    for (size_t j(0); j != veclen; ++j) {
                                        (*checked_intermediates)[check_pos + i][j]
                                                += local_val[i][j];
                                    }
                                }
                                check_pos += batch_sz;
                            }
                            ++next_pair[log_batch_sz];
                        }
                        // booked_permute[{permuter, logsz, veclen}].push_back({small_perm, session});
                    } else {
                        for (int cnt(0); cnt != rep; ++cnt) {
                            if (next_pair[log_batch_sz] == pairs[permuter][log_batch_sz].end()) {
                                std::cerr << "permute_session::perform : unexpected end of permute_pair." << std::endl;
                                throw std::runtime_error("permute_session::perform : unexpected end of permute_pair.");
                            }
                            static vectors<ClearType> sum;
                            sum = next_pair[log_batch_sz]->a + local_val;
                            
                            com.send(permuter, sum);

                            local_val = next_pair[log_batch_sz]->b;
                            if (checked_intermediates != nullptr) {
                                for (int i(0); i != batch_sz; ++i) {
                                    for (size_t j(0); j != veclen; ++j) {
                                        (*checked_intermediates)[check_pos + i][j]
                                                = local_val[i][j];
                                    }
                                }
                                check_pos += batch_sz;
                            }
                            ++next_pair[log_batch_sz];
                        }
                    }

                    for (int v : touched) {
                        for (size_t j(0); j != veclen; ++j) {
                            if (me == permuter) dum_val[v][j] = local_val[map[v]][j];
                            else val[v][j] = local_val[map[v]][j];
                        }
                    }
                    size_t component_rounds_after = com.count_total_rounds();
                    size_t component_rounds = component_rounds_after - component_rounds_before;
                    sequential_component_rounds += component_rounds;
                    parallel_component_rounds = std::max(parallel_component_rounds, component_rounds);
                }
                if (sequential_component_rounds > parallel_component_rounds) {
                    com.add_round_adjustment(-static_cast<long long>(
                            sequential_component_rounds - parallel_component_rounds));
                }
            }
            // Add the permuted dummy value to result.
            result += dum_val;
            size_t rounds_after = com.count_total_rounds();
            size_t delta_rounds = rounds_after - rounds_before;
            sequential_sender_rounds += delta_rounds;
            parallel_sender_rounds = std::max(parallel_sender_rounds, delta_rounds);
        }
        if (sequential_sender_rounds > parallel_sender_rounds) {
            com.add_round_adjustment(-static_cast<long long>(
                    sequential_sender_rounds - parallel_sender_rounds));
        }
        if (me == permuter) {
            val = result;
        }
        destroy();
        com.output_check();
    }
    
    void permute_session::perform(mpc_comm &com, vectors<ShareType>& val)
    {
        vectors<ClearType> valMac(val.num, val.len * 2);
        for (size_t i(0); i != val.num; ++i) {
            for (size_t j(0); j != val.len; ++j) {
                valMac[i][j * 2] = val[i][j].get_share();
                valMac[i][j * 2 + 1] = val[i][j].get_mac();
            }
        }
        vectors<ClearType> intermediate_valmac;
        perform(com, valMac, &intermediate_valmac);
        for (size_t i(0); i != val.num; ++i) {
            for (size_t j(0); j != val.len; ++j) {
                val[i][j].set_share(valMac[i][j * 2]);
                val[i][j].set_mac(valMac[i][j * 2 + 1]);
            }
        }
        vectors<ShareType> checked(intermediate_valmac.num + val.num, val.len);
        for (size_t i(0); i != intermediate_valmac.num; ++i) {
            for (size_t j(0); j != val.len; ++j) {
                checked[i][j].set_share(intermediate_valmac[i][j * 2]);
                checked[i][j].set_mac(intermediate_valmac[i][j * 2 + 1]);
            }
        }
        for (size_t i(0); i != val.num; ++i) {
            for (size_t j(0); j != val.len; ++j) {
                checked[intermediate_valmac.num + i][j] = val[i][j];
            }
        }
        com.mac_check(checked);
    }

    const permutation& permute_session::get_perm() const
    {
        return perm;
    }

    void permute_session::clear()
    {
        initialized = false;
        for (int i(0); i != MAX_BATCH_SIZE; ++i) {
            for (int j(0); j != MAX_BATCH_SIZE; ++j) {
                pairs[i][j].clear();
            }
        }
    }

    permute_pair::permute_pair(const permutation &perm, const vectors<ClearType> &a, const vectors<ClearType> &b, const vectors<ClearType> &delta)
        : perm(perm), a(a), b(b), delta(delta), opvm()
    {
    }

    permute_pair::permute_pair(const permutation& _perm, const vectors<prg_seed> &_opvm)
        : perm(_perm), a(), b(), delta(), opvm(_opvm)
    {
    }

    void permute_pair::expand(size_t veclen, bool oblivious)
    {
        size_t sz = opvm.num;
        if (oblivious) {
            permutation inv = perm.inverse();
            delta.resize(sz, veclen);
            for (size_t i(0); i != sz; ++i) {
                for (size_t j(0); j != sz; ++j) {
                    if (perm[i] == j) continue;
                    if (veclen > 1) { // Extend seed to veclen.
                        vectors<ClearType> val(1, veclen);
                        arbitrary_prg(opvm[i][j], val);
                        for (size_t k(0); k != veclen; ++k) {
                            delta[i][k] -= val.at(k);
                            delta[inv[j]][k] += val.at(k);
                        }
                    }
                }
            }
        } else {
            perm = permutation(sz); // default initialization.
            a.resize(sz, veclen);
            b.resize(sz, veclen);
            for (size_t i(0); i != sz; ++i) {
                for (size_t j(0); j != sz; ++j) {
                    if (veclen > 1) { // Extend seed to veclen.
                        vectors<ClearType> val(1, veclen);
                        arbitrary_prg(opvm[i][j], val);
                        for (size_t k(0); k != veclen; ++k) {
                            a[j][k] += val.at(k);
                            b[i][k] -= val.at(k);
                        }
                    }
                }
            }
        }
        opvm.clear();
    }
}
