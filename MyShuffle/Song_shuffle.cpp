#include "Song_shuffle.h"

namespace song2023 {
    using namespace gjcShuffle;
    std::map<permute_info, std::vector<order_info>> booked_permute;

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
    */
    void book_permute_session(mpc_comm &com, permute_session *session)
    {
        int me = com.get_my_number(), permuter = session->permuter,
            logsz = session->logsz, veclen = session->veclen, batch = session->batch;
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
				if (me == permuter) {
                    permutation small_perm(batch_sz);
					for (int v : touched) {
                        small_perm[map[v]] = map[sub_route[bat][v]];
					}
                    for (int cnt(1); cnt != STATISTICAL_LAMBDA; ++cnt) {
                        // Create extra permutation for statistical security.
                        permutation extra_perm(batch_sz, true);
                        small_perm = extra_perm.inverse() * small_perm;
                        booked_permute[{permuter, log_batch_sz}].push_back({extra_perm, session});
                    }
                    booked_permute[{permuter, log_batch_sz}].push_back({small_perm, session});
				} else {
					for (int cnt(0); cnt != STATISTICAL_LAMBDA; ++cnt) {
                        booked_permute[{permuter, log_batch_sz}].push_back({{}, session});
                    }
				}
			}
		}
    }

    void book_shuffle_session(mpc_comm &com, shuffle_session *session)
    {
        int me = com.get_my_number(), n = com.get_n_party();
        for (int i(0); i != n; ++i) {
            book_permute_session(com, &session->permute_sessions[i]);
        }
    }

    void shuffle_session::destroy()
    {
        destroyed = true;
    }

    permute_pair pair_from_opvm(int veclen, const vectors<block_wrapper> &opvm, const permutation &perm, bool oblivious)
    {
        int sz = opvm.num;
        permute_pair pair; // a is the sum along column, b is the negative sum along row.
        if (oblivious) {
            permutation inv = perm.inverse();
            pair.perm = perm;
            pair.delta.resize(sz, veclen);
            for (int i(0); i != sz; ++i) {
                for (int j(0); j != sz; ++j) {
                    if (perm[i] == j) continue;
                    if (veclen > 1) { // Extend seed to veclen.
                        vectors<ClearType> val(1, veclen);
                        arbitrary_prg(opvm[i][j], val);
                        for (int k(0); k != veclen; ++k) {
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
            for (int i(0); i != sz; ++i) {
                for (int j(0); j != sz; ++j) {
                    if (veclen > 1) { // Extend seed to veclen.
                        vectors<ClearType> val(1, veclen);
                        arbitrary_prg(opvm[i][j], val);
                        for (int k(0); k != veclen; ++k) {
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
        int sz = 1 << logsz;

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
            for (int i(0); i != sz; ++i) {
                if (me == permuter) {
                    // Permuter acts as OPV receiver
                    receiver_append_OPV(choose, hash_val, logsz, order.perm[i]);
                } else {
                    sender_append_OPV(msg0, msg1, hash_val, logsz, opvs);
                    for (int j(0); j != sz; ++j) opvm[i][j] = opvs.back()[j];
                }
            }
            if (me == sender) {
                append_left_right_opvm(opvm, left_opvms, right_opvms);
                // Sender computes the sum of each column.
                const vectors<block_wrapper>& left_opvm = left_opvms.back();
                for (int j(0); j != sz; ++j) { // j : row-th number
                    block_wrapper sum = {};
                    for (int i(0); i != sz; ++i) {
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
                for (int i(0); i != sz; ++i) {
                    opv_2n opv;
                    opv = opv_2n(logsz, order.perm[i], next_opv_msg, next_hash_msg);

                    for (int j(0); j != sz; ++j) {
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
                for (int i(0); i != sz; ++i) {
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
        // Permuter reporst hash value of left_opvm.
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
        int me = com.get_my_number(), n = com.get_n_party();
        for (auto keyval : booked_permute) {
            permute_info info = keyval.first;
            std::vector<order_info> &orders = keyval.second;
            if (me == info.permuter) {
                for (int sender(0); sender != n; ++sender) {
                    if (me == sender) continue;

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
                }
            } else {
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
            }
        }
        for (auto& keyval : booked_permute) {
            for (auto& session : keyval.second) {
                session.session->initialized = true;
            }
        }
        booked_permute.clear();
    }

    order_info::order_info(const permutation &_perm, permute_session *_session)
        : perm(_perm), session(_session)
    {
    }

    void permute_session::destroy()
    {
        destroyed = true;
    }

    void permute_session::perform(mpc_comm &com, vectors<ClearType> &val)
    {
        if (!initialized) {
            std::cerr << FAIL_INFO << "Not initialized. Please call function process_all_orders." << std::endl;
            throw std::runtime_error("permute_session::perform : Not initialized.");
        }
        int n = com.get_n_party(), me = com.get_my_number();
        size_t sz = (1 << logsz);
#ifdef DEBUG
        if (sz != val.num || veclen != val.len) {
            std::cerr << "permute_session::perform : size mismatch, " 
                << sz << " != " << val.num << " or " << veclen << " != " << val.len << std::endl;
            throw std::runtime_error("permute_session::perform : size mismatch.");
        }
#endif
        std::vector<int> dest(sz);
		if (me == permuter) {
			for (size_t i(0); i != sz; ++i) dest[perm[i]] = i;
		} else {
			for (size_t i(0); i != sz; ++i) dest[i] = i;
		}
		const auto& all_tasks = BenesNetwork::task_decompose(logsz, batch);
		auto route = BenesNetwork::route(logsz, dest);
		auto sub_route = BenesNetwork::decompose(route, batch);
		BenesNetwork::desttask_to_permtask(sz, sub_route);
		std::vector<int> map(sz), inv_map(sz);

        // permute_pair * next_pair[MAX_BATCH_SIZE];
        // for (int bat(0); bat != MAX_BATCH_SIZE; ++bat) {
        //     if (me == permuter) next_pair[i] = &pairs[bat][0][0];
        //     else next_pair[i] = &pairs[me][i][i];
        // }
        vectors<ClearType> result(val.num, val.len);
        for (size_t i(0); i != val.num; ++i) {
            for (size_t j(0); j != val.len; ++j) {
                result[i][j] = val[perm[i]][j];
            }
        }
        for (int sender(0); sender != n; ++sender) {
            if (sender == permuter) continue;
            if (me != permuter && me != sender) continue;
            std::vector<permute_pair>::iterator next_pair[MAX_BATCH_SIZE];
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
                    vectors<ClearType> local_val(batch_sz, veclen), recv_val(batch_sz, veclen), temp_val(batch_sz, veclen);
                    for (int v : touched) {
                        for (size_t j(0); j != veclen; ++j) {
                            if (me == permuter) local_val[map[v]][j] = dum_val[v][j];
                            else local_val[map[v]][j] = val[v][j];
                        }
                    }
                    if (me == permuter) { // Receiver of OT
                        for (int cnt(0); cnt != STATISTICAL_LAMBDA; ++cnt) {
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
                            ++next_pair[log_batch_sz];
                        }
                        // booked_permute[{permuter, logsz, veclen}].push_back({small_perm, session});
                    } else {
                        for (int cnt(0); cnt != STATISTICAL_LAMBDA; ++cnt) {
                            if (next_pair[log_batch_sz] == pairs[permuter][log_batch_sz].end()) {
                                std::cerr << "permute_session::perform : unexpected end of permute_pair." << std::endl;
                                throw std::runtime_error("permute_session::perform : unexpected end of permute_pair.");
                            }
                            vectors<ClearType> sum = next_pair[log_batch_sz]->a + local_val;
                            com.send(permuter, sum);
                            local_val = next_pair[log_batch_sz]->b;
                            ++next_pair[log_batch_sz];
                        }
                    }
                    for (int v : touched) {
                        for (size_t j(0); j != veclen; ++j) {
                            if (me == permuter) dum_val[v][j] = local_val[map[v]][j];
                            else val[v][j] = local_val[map[v]][j];
                        }
                    }
                }
            }
            // Add the permuted dummy value to result.
            result += dum_val;
        }
        if (me == permuter) {
            val = result;
        }
        destroy();
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
        perform(com, valMac);
        for (size_t i(0); i != val.num; ++i) {
            for (size_t j(0); j != val.len; ++j) {
                val[i][j].set_share(valMac[i][j * 2]);
                val[i][j].set_mac(valMac[i][j * 2 + 1]);
            }
        }
        com.mac_check(val);
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
        int sz = opvm.num;
        if (oblivious) {
            permutation inv = perm.inverse();
            delta.resize(sz, veclen);
            for (int i(0); i != sz; ++i) {
                for (int j(0); j != sz; ++j) {
                    if (perm[i] == j) continue;
                    if (veclen > 1) { // Extend seed to veclen.
                        vectors<ClearType> val(1, veclen);
                        arbitrary_prg(opvm[i][j], val);
                        for (int k(0); k != veclen; ++k) {
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
            for (int i(0); i != sz; ++i) {
                for (int j(0); j != sz; ++j) {
                    if (veclen > 1) { // Extend seed to veclen.
                        vectors<ClearType> val(1, veclen);
                        arbitrary_prg(opvm[i][j], val);
                        for (int k(0); k != veclen; ++k) {
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