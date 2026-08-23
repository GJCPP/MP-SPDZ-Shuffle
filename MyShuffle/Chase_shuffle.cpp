#include "Chase_shuffle.h"

#include "math_gadget.h"

namespace chase2020 {
	using namespace myShuffle;
	shuffle_pair prepare_permute(mpc_comm& com, int sender, int permuter,
		const std::vector<int>& perm, int sz, int veclen, int batch)
	{
		int me = com.get_my_number();
		int logsz = math_gadget::log2(sz);
		shuffle_pair ret = {};
		ret.sender = sender, ret.permuter = permuter;
		ret.bat = batch, ret.logsz = logsz, ret.veclen = veclen;
		ret.a.resize(veclen), ret.b.resize(veclen), ret.delta.resize(veclen);
		if (me != sender && me != permuter) return ret;
		if (sz != (1 << logsz)) {
			std::cerr << "prepare_permute: sz must be power of two." << std::endl;
			throw std::runtime_error("prepare_permute: sz must be power of two.");
		}
		static osuCrypto::BitVector choose;
		static std::vector<prg_seed> msg0, msg1, hash_val, col_sum;
		static std::vector<prg_seed> opvm_hash;
		static std::vector<opv_2n> opvs;
		choose.reset();
		msg0.clear(), msg1.clear(), hash_val.clear(), col_sum.clear();
		opvm_hash.clear();
		opvs.clear();
		int next_opvs(0);
		std::vector<int> dest(sz);
		if (me == permuter) {
			for (int i(0); i != sz; ++i) dest[perm[i]] = i;
		} else {
			for (int i(0); i != sz; ++i) dest[i] = i;
		}
		const auto& all_tasks = BenesNetwork::task_decompose(logsz, batch);
		auto route = BenesNetwork::route(logsz, dest);
		auto sub_route = BenesNetwork::decompose(route, batch);
		BenesNetwork::desttask_to_permtask(sz, sub_route);
		std::vector<int> map(sz), inv_map(sz);
		for (size_t bat(0); bat != sub_route.size(); ++bat) { // sub_route[i][j] is where j is route to in current depth i
			const auto& task = all_tasks[bat];
			int next(0), batch_sz(task[0].size());
			int log_batch_sz = math_gadget::log2(batch_sz);
			for (const auto& touched : task) {
				next = 0;
				for (int v : touched) { // touched is the component specified by (depth, rank of task)
					inv_map[next] = v; // map the position back.
					map[v] = next++;
				}
				if (me == permuter) { // Receiver of OT
					for (int v : touched) { // Construct OPM
						receiver_append_OPV(choose, hash_val, log_batch_sz, map[sub_route[bat][v]]);
					}
					col_sum.resize(col_sum.size() + batch_sz); // preserve for receiving column sum
				} else {
					for ([[maybe_unused]] int v : touched) {
						sender_append_OPV(msg0, msg1, hash_val, log_batch_sz, opvs);
					}
					// Send sum along columns for check
					vectors<block_wrapper> left_opvm(batch_sz, batch_sz); // The left opvm is for integrity check
					const opv_2n *seed_opvm = &opvs[opvs.size() - batch_sz];
					Hash hash;
					block_wrapper hash_res[BLOCKS_FOR_HASH];
					// Reconstruct left OPVM and compute hash (first left to right, then top to bottom)
					for (int i(0); i != batch_sz; ++i) {
						for (int j(0); j != batch_sz; ++j) {
							left_opvm[i][j] = double_length_prg(seed_opvm[i][j])[0];
							hash.update(&left_opvm[i][j], sizeof(block_wrapper));
						}
					}
					hash.final(reinterpret_cast<octet *>(hash_res), sizeof(hash_res));
					for (int i(0); i != BLOCKS_FOR_HASH; ++i) opvm_hash.push_back(hash_res[i]);
					// Compute sum along columns 
					for (int j(0); j != batch_sz; ++j) {
						block_wrapper sum = {};
						for (int i(0); i != batch_sz; ++i) {
							sum += left_opvm[i][j];
						}
						col_sum.push_back(sum);
					}
				}
			}
		}

		static std::vector<prg_seed> msg; msg.clear();
		prg_seed* next_msg = 0, *next_hash = 0, *next_sum = 0;
		if (me == permuter) {
			msg.resize(choose.size());
			com.ext_ot_recv(sender, choose, msg);
			com.recv(sender, hash_val);
			com.recv(sender, col_sum);
			next_msg = msg.data();
			next_hash = hash_val.data();
			next_sum = col_sum.data();
		} else {
			com.ext_ot_send(permuter, msg0, msg1);
			com.send(permuter, hash_val);
			com.send(permuter, col_sum);
		}
		// Compute shuffle_pair
		for (size_t bat(0); bat != sub_route.size(); ++bat) {
			const auto& task = all_tasks[bat];
			int next(0), batch_sz(task[0].size());
			int log_batch_sz = math_gadget::log2(batch_sz);
			for (const auto& touched : task) {
				next = 0;
				std::vector<int> small_perm(batch_sz);
				for (int v : touched) {
					inv_map[next] = v; // map the position back.
					map[v] = next++;
				}
				for (int v : touched) {
					small_perm[map[v]] = map[sub_route[bat][v]];
				}

				// Left opvm is for checking, right is for permuting.
				std::vector<opv_2n> seed_opvm;
				vectors<block_wrapper> left_opvm(batch_sz, batch_sz), right_opvm(batch_sz, batch_sz);
				std::vector<int> oblivious_pos;
				if (me == permuter) { // Receiver of OT
					// Reconstruct opv matrix
					for (int v : touched) {
						seed_opvm.push_back(opv_2n(log_batch_sz, map[sub_route[bat][v]], next_msg, next_hash));
						oblivious_pos.push_back(map[sub_route[bat][v]]);
					}
				} else {
					for ([[maybe_unused]] int v : touched) {
						seed_opvm.push_back(opvs[next_opvs++]);
					}
				}
				// Expand to left/right opvm
				for (int i(0); i != batch_sz; ++i) {
					for (int j(0); j != batch_sz; ++j) {
						if (me == permuter && j == oblivious_pos[i]) continue; // Skip puntured position
						auto expand = double_length_prg(seed_opvm[i][j]);
						left_opvm[i][j] = expand[0];
						right_opvm[i][j] = expand[1];
					}
				}
				if (me == permuter) {
					// Reconstruct whole left opvm, and transmit hash value back.
					std::vector<block_wrapper> missing;
					for (int j(0); j != batch_sz; ++j) {
						block_wrapper sum(*next_sum++);
						for (int i(0); i != batch_sz; ++i) {
							sum -= left_opvm[i][j];
						}
						missing.push_back(sum);
					}
					for (int i(0); i != batch_sz; ++i) {
						left_opvm[i][oblivious_pos[i]] = missing[oblivious_pos[i]];
					}
					// Computing Hash
					Hash hash;
					block_wrapper hash_res[BLOCKS_FOR_HASH];
					for (int i(0); i != batch_sz; ++i) {
						for (int j(0); j != batch_sz; ++j) {
							hash.update(&left_opvm[i][j], sizeof(block_wrapper));
						}
					}
					hash.final(reinterpret_cast<octet *>(hash_res), sizeof(hash_res));
					for (int i(0); i != BLOCKS_FOR_HASH; ++i) opvm_hash.push_back(hash_res[i]);
				}

				std::vector<vectors<block_wrapper>> int_mat(veclen, vectors<block_wrapper>(batch_sz, batch_sz));
				for (int i(0); i != batch_sz; ++i) {
					for (int j(0); j != batch_sz; ++j) {
						if (me == permuter && j == oblivious_pos[i]) {
							if (right_opvm[i][j].is_nonzero()) {
								std::cerr << "chase2020::prepare_permute : Implementation error." << std::endl;
								throw std::runtime_error("chase2020::prepare_permute : Implementation error.");
							}
							continue; // This position is oblivious.
						}
						block_string ext = arbitrary_prg(right_opvm[i][j], veclen); // Extend to mask data vector
						for (int entry(0); entry != veclen; ++entry) {
							int_mat[entry][i][j] = ext[entry];
						}
					}
				}
				std::vector<std::vector<block_wrapper>> a(veclen, std::vector<block_wrapper>(batch_sz)),
					b(veclen, std::vector<block_wrapper>(batch_sz));

				for (int entry(0); entry != veclen; ++entry) {
					for (int i(0); i != batch_sz; ++i) {
						for (int j(0); j != batch_sz; ++j) {
							a[entry][j] += int_mat[entry][i][j]; // Sum along the column.
							b[entry][i] -= int_mat[entry][i][j]; // Sum of negations along the row.
						}
					}
				}
				if (me == permuter) {
					for (int entry(0); entry != veclen; ++entry) {
						for (int i(0); i != batch_sz; ++i) {
							ret.delta[entry].push_back(a[entry][small_perm[i]] + b[entry][i]);
						}
					}
				} else {
					for (int entry(0); entry != veclen; ++entry) {
						for (int i(0); i != batch_sz; ++i) {
							ret.a[entry].push_back(a[entry][i]);
							ret.b[entry].push_back(b[entry][i]);
						}
					}
				}
			}
		}
		if (me == permuter) {
			// Send Hash value
			com.send(sender, opvm_hash);
		} else {
			std::vector<block_wrapper> recv_hash(opvm_hash.size());
			com.recv(permuter, recv_hash);
			for (size_t i(0); i != opvm_hash.size(); ++i) {
				if (opvm_hash[i] != recv_hash[i]) {
					std::cerr << "Party " << me << " as sender in Chase::prepare_permute : incorrect OPVM Hash!" << std::endl;
					std::cerr << "Party " << me << " : abort!" << std::endl;
					std::abort();
				}
			}
		}
		ret.sender = sender;
		ret.permuter = permuter;
		ret.logsz = logsz, ret.bat = batch;
		ret.perm = perm;
		return ret;
	}

	void permute(mpc_comm& com, vectors<block_wrapper>& val, shuffle_pair& plan)
	{
		int sender = plan.sender, permuter = plan.permuter;
		int me = com.get_my_number();
		if (me != sender && me != permuter) return;
		int sz = (1 << plan.logsz), logsz = plan.logsz, veclen = plan.veclen, batch = plan.bat;
		int next_batch_ab(0);
		std::vector<int> map(sz), inv_map(sz);
		std::vector<int> dest(sz);
		if (me == plan.permuter) {
			for (int i(0); i != sz; ++i) dest[plan.perm[i]] = i;
		} else {
			for (int i(0); i != sz; ++i) dest[i] = i;
		}

		const auto& all_tasks = BenesNetwork::task_decompose(logsz, batch);
		auto route = BenesNetwork::route(logsz, dest);
		auto sub_route = BenesNetwork::decompose(route, batch);
		BenesNetwork::desttask_to_permtask(sz, sub_route);
		std::vector<block_wrapper> data;
		size_t cnt_send = sub_route.size() * sz * veclen;

		data.resize(cnt_send);
		int next_send(0);

		if (me == permuter) {
			com.recv(sender, data);
		}

		for (size_t bat(0); bat != sub_route.size(); ++bat) {
			const auto& task = all_tasks[bat];
			int next(0), batch_sz(task[0].size());
			for (const auto& touched : task) {
				std::vector<int> small_perm(batch_sz);
				next = 0;
				for (int v : touched) {
					inv_map[next] = v; // map the position back.
					map[v] = next++;
				}
				for (int v : touched) {
					small_perm[map[v]] = map[sub_route[bat][v]];
				}
				for (int entry(0); entry != veclen; ++entry) {
					std::vector<block_wrapper> vec, next_vec(batch_sz);
					if (me == sender) {
						vec.reserve(batch_sz);
						for (int i(0); i != batch_sz; ++i) {
							vec.push_back(val[touched[i]][entry] + plan.a[entry][next_batch_ab + i]);
						}
						for (auto i : vec) data[next_send++] = i;
						for (int i(0); i != batch_sz; ++i) next_vec[i] = (plan.b[entry][next_batch_ab + i]);
					} else {
						for (int i(0); i != batch_sz; ++i) {
							vec.push_back(val[touched[i]][entry]);
						}
						std::vector<block_wrapper> ay(batch_sz);
						for (int i(0); i != batch_sz; ++i) ay[i] = data[next_send++];

						for (int i(0); i != batch_sz; ++i) {
							next_vec[i] = vec[small_perm[i]] - plan.delta[entry][next_batch_ab + i] + ay[small_perm[i]];
						}
					}
					for (int i(0); i != batch_sz; ++i) {
						val[inv_map[i]][entry] = next_vec[i];
					}
				}
				next_batch_ab += batch_sz;
			}
		}
		if (me == sender) {
			com.send(permuter, data);
		}
	}
	
	void permute(mpc_comm& com, std::vector<block_wrapper>& val, shuffle_pair& plan)
	{
		if (plan.veclen != 1) {
			std::cerr << "Chase2020::permute(vec): plan.veclen != 1." << std::endl;
			throw std::runtime_error("Chase2020::permute(vec): plan.veclen != 1.");
		}
		if ((size_t(1) << plan.logsz) != val.size()) {
			std::cerr << "Chase2020::permute(vec): 2^plan.logsz != val.size()." << std::endl;
			throw std::runtime_error("Chase2020::permute(vec): 2^plan.logsz != val.size().");
		}
		vectors<block_wrapper> vec(val.size(), 1);
		for (size_t i(0); i != val.size(); ++i) {
			vec.at(i) = val[i];
		}
		permute(com, vec, plan);
		for (size_t i(0); i != val.size(); ++i) {
			val[i] = vec.at(i);
		}
	}

	clear_shuffle_pair prepare_permute_clear(mpc_comm& com, int sender, int permuter,
		const std::vector<int>& perm, int sz, int veclen, int batch)
	{
		int me = com.get_my_number();
		int logsz = math_gadget::log2(sz);
		clear_shuffle_pair ret = {};
		ret.sender = sender, ret.permuter = permuter;
		ret.bat = batch, ret.logsz = logsz, ret.veclen = veclen;
		ret.a.resize(veclen), ret.b.resize(veclen), ret.delta.resize(veclen);
		if (me != sender && me != permuter) return ret;
		if (sz != (1 << logsz)) {
			std::cerr << "prepare_permute_clear: sz must be power of two." << std::endl;
			throw std::runtime_error("prepare_permute_clear: sz must be power of two.");
		}
		static osuCrypto::BitVector choose;
		static std::vector<prg_seed> msg0, msg1, hash_val, col_sum;
		static std::vector<prg_seed> opvm_hash;
		static std::vector<opv_2n> opvs;
		choose.reset();
		msg0.clear(), msg1.clear(), hash_val.clear(), col_sum.clear();
		opvm_hash.clear();
		opvs.clear();
		int next_opvs(0);
		std::vector<int> dest(sz);
		if (me == permuter) {
			for (int i(0); i != sz; ++i) dest[perm[i]] = i;
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
				for (int v : touched) {
					inv_map[next] = v;
					map[v] = next++;
				}
				if (me == permuter) {
					for (int v : touched) {
						receiver_append_OPV(choose, hash_val, log_batch_sz, map[sub_route[bat][v]]);
					}
					col_sum.resize(col_sum.size() + batch_sz);
				} else {
					for ([[maybe_unused]] int v : touched) {
						sender_append_OPV(msg0, msg1, hash_val, log_batch_sz, opvs);
					}
					vectors<block_wrapper> left_opvm(batch_sz, batch_sz);
					const opv_2n *seed_opvm = &opvs[opvs.size() - batch_sz];
					Hash hash;
					block_wrapper hash_res[BLOCKS_FOR_HASH];
					for (int i(0); i != batch_sz; ++i) {
						for (int j(0); j != batch_sz; ++j) {
							left_opvm[i][j] = double_length_prg(seed_opvm[i][j])[0];
							hash.update(&left_opvm[i][j], sizeof(block_wrapper));
						}
					}
					hash.final(reinterpret_cast<octet *>(hash_res), sizeof(hash_res));
					for (int i(0); i != BLOCKS_FOR_HASH; ++i) opvm_hash.push_back(hash_res[i]);
					for (int j(0); j != batch_sz; ++j) {
						block_wrapper sum = {};
						for (int i(0); i != batch_sz; ++i) {
							sum += left_opvm[i][j];
						}
						col_sum.push_back(sum);
					}
				}
			}
		}

		static std::vector<prg_seed> msg; msg.clear();
		prg_seed* next_msg = 0, *next_hash = 0, *next_sum = 0;
		if (me == permuter) {
			msg.resize(choose.size());
			com.ext_ot_recv(sender, choose, msg);
			com.recv(sender, hash_val);
			com.recv(sender, col_sum);
			next_msg = msg.data();
			next_hash = hash_val.data();
			next_sum = col_sum.data();
		} else {
			com.ext_ot_send(permuter, msg0, msg1);
			com.send(permuter, hash_val);
			com.send(permuter, col_sum);
		}

		for (size_t bat(0); bat != sub_route.size(); ++bat) {
			const auto& task = all_tasks[bat];
			int next(0), batch_sz(task[0].size());
			int log_batch_sz = math_gadget::log2(batch_sz);
			for (const auto& touched : task) {
				next = 0;
				std::vector<int> small_perm(batch_sz);
				for (int v : touched) {
					inv_map[next] = v;
					map[v] = next++;
				}
				for (int v : touched) {
					small_perm[map[v]] = map[sub_route[bat][v]];
				}

				std::vector<opv_2n> seed_opvm;
				vectors<block_wrapper> left_opvm(batch_sz, batch_sz), right_opvm(batch_sz, batch_sz);
				std::vector<int> oblivious_pos;
				if (me == permuter) {
					for (int v : touched) {
						seed_opvm.push_back(opv_2n(log_batch_sz, map[sub_route[bat][v]], next_msg, next_hash));
						oblivious_pos.push_back(map[sub_route[bat][v]]);
					}
				} else {
					for ([[maybe_unused]] int v : touched) {
						seed_opvm.push_back(opvs[next_opvs++]);
					}
				}
				for (int i(0); i != batch_sz; ++i) {
					for (int j(0); j != batch_sz; ++j) {
						if (me == permuter && j == oblivious_pos[i]) continue;
						auto expand = double_length_prg(seed_opvm[i][j]);
						left_opvm[i][j] = expand[0];
						right_opvm[i][j] = expand[1];
					}
				}
				if (me == permuter) {
					std::vector<block_wrapper> missing;
					for (int j(0); j != batch_sz; ++j) {
						block_wrapper sum(*next_sum++);
						for (int i(0); i != batch_sz; ++i) {
							sum -= left_opvm[i][j];
						}
						missing.push_back(sum);
					}
					for (int i(0); i != batch_sz; ++i) {
						left_opvm[i][oblivious_pos[i]] = missing[oblivious_pos[i]];
					}
					Hash hash;
					block_wrapper hash_res[BLOCKS_FOR_HASH];
					for (int i(0); i != batch_sz; ++i) {
						for (int j(0); j != batch_sz; ++j) {
							hash.update(&left_opvm[i][j], sizeof(block_wrapper));
						}
					}
					hash.final(reinterpret_cast<octet *>(hash_res), sizeof(hash_res));
					for (int i(0); i != BLOCKS_FOR_HASH; ++i) opvm_hash.push_back(hash_res[i]);
				}

				std::vector<std::vector<ClearType>> a(veclen, std::vector<ClearType>(batch_sz)),
					b(veclen, std::vector<ClearType>(batch_sz));
				for (int i(0); i != batch_sz; ++i) {
					for (int j(0); j != batch_sz; ++j) {
						if (me == permuter && j == oblivious_pos[i]) {
							if (right_opvm[i][j].is_nonzero()) {
								std::cerr << "chase2020::prepare_permute_clear : Implementation error." << std::endl;
								throw std::runtime_error("chase2020::prepare_permute_clear : Implementation error.");
							}
							continue;
						}
						vectors<ClearType> ext(1, veclen);
						arbitrary_prg(right_opvm[i][j], ext);
						for (int entry(0); entry != veclen; ++entry) {
							a[entry][j] += ext[0][entry];
							b[entry][i] -= ext[0][entry];
						}
					}
				}
				if (me == permuter) {
					for (int entry(0); entry != veclen; ++entry) {
						for (int i(0); i != batch_sz; ++i) {
							ret.delta[entry].push_back(a[entry][small_perm[i]] + b[entry][i]);
						}
					}
				} else {
					for (int entry(0); entry != veclen; ++entry) {
						for (int i(0); i != batch_sz; ++i) {
							ret.a[entry].push_back(a[entry][i]);
							ret.b[entry].push_back(b[entry][i]);
						}
					}
				}
			}
		}
		if (me == permuter) {
			com.send(sender, opvm_hash);
		} else {
			std::vector<block_wrapper> recv_hash(opvm_hash.size());
			com.recv(permuter, recv_hash);
			for (size_t i(0); i != opvm_hash.size(); ++i) {
				if (opvm_hash[i] != recv_hash[i]) {
					std::cerr << "Party " << me << " as sender in Chase::prepare_permute_clear : incorrect OPVM Hash!" << std::endl;
					std::abort();
				}
			}
		}
		ret.sender = sender;
		ret.permuter = permuter;
		ret.logsz = logsz, ret.bat = batch;
		ret.perm = perm;
		return ret;
	}

	void permute(mpc_comm& com, vectors<ClearType>& val, clear_shuffle_pair& plan)
	{
		int sender = plan.sender, permuter = plan.permuter;
		int me = com.get_my_number();
		if (me != sender && me != permuter) return;
		int sz = (1 << plan.logsz), logsz = plan.logsz, veclen = plan.veclen, batch = plan.bat;
		int next_batch_ab(0);
		std::vector<int> map(sz), inv_map(sz);
		std::vector<int> dest(sz);
		if (me == plan.permuter) {
			for (int i(0); i != sz; ++i) dest[plan.perm[i]] = i;
		} else {
			for (int i(0); i != sz; ++i) dest[i] = i;
		}

		const auto& all_tasks = BenesNetwork::task_decompose(logsz, batch);
		auto route = BenesNetwork::route(logsz, dest);
		auto sub_route = BenesNetwork::decompose(route, batch);
		BenesNetwork::desttask_to_permtask(sz, sub_route);
		std::vector<ClearType> data;
		size_t cnt_send = sub_route.size() * sz * veclen;

		data.resize(cnt_send);
		int next_send(0);

		if (me == permuter) {
			com.recv(sender, data);
		}

		for (size_t bat(0); bat != sub_route.size(); ++bat) {
			const auto& task = all_tasks[bat];
			int next(0), batch_sz(task[0].size());
			for (const auto& touched : task) {
				std::vector<int> small_perm(batch_sz);
				next = 0;
				for (int v : touched) {
					inv_map[next] = v;
					map[v] = next++;
				}
				for (int v : touched) {
					small_perm[map[v]] = map[sub_route[bat][v]];
				}
				for (int entry(0); entry != veclen; ++entry) {
					std::vector<ClearType> vec, next_vec(batch_sz);
					if (me == sender) {
						vec.reserve(batch_sz);
						for (int i(0); i != batch_sz; ++i) {
							vec.push_back(val[touched[i]][entry] + plan.a[entry][next_batch_ab + i]);
						}
						for (auto i : vec) data[next_send++] = i;
						for (int i(0); i != batch_sz; ++i) next_vec[i] = plan.b[entry][next_batch_ab + i];
					} else {
						for (int i(0); i != batch_sz; ++i) {
							vec.push_back(val[touched[i]][entry]);
						}
						std::vector<ClearType> ay(batch_sz);
						for (int i(0); i != batch_sz; ++i) ay[i] = data[next_send++];

						for (int i(0); i != batch_sz; ++i) {
							next_vec[i] = vec[small_perm[i]] - plan.delta[entry][next_batch_ab + i] + ay[small_perm[i]];
						}
					}
					for (int i(0); i != batch_sz; ++i) {
						val[inv_map[i]][entry] = next_vec[i];
					}
				}
				next_batch_ab += batch_sz;
			}
		}
		if (me == sender) {
			com.send(permuter, data);
		}
	}
}
