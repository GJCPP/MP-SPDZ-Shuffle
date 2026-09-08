#include <iostream>
#include <queue>
#include <map>
#include <random>

#include "Benes_network.h"


namespace BenesNetwork
{
	layer_dest::layer_dest(int n)
		: dest(1 << n) {
		;
	}

	int& layer_dest::operator[](int pos) {
		return dest[pos];
	}

	int layer_dest::operator[](int pos) const {
		return dest[pos];
	}
	Benes_network::Benes_network(int _n)
		: n(_n), layers(2 * _n - 1, layer_dest(_n)) {
		for (int i = 0; i < 2 * _n - 1; i++) {
			layers[i] = layer_dest(_n);
		}
	}
	layer_dest& Benes_network::operator[](int pos) {
		return layers[pos];
	}

	const layer_dest& Benes_network::operator[](int pos) const {
		return layers[pos];
	}

	int Benes_network::depth() const {
		return 2 * n - 1;
	}

	int Benes_network::width() const {
		return 1 << n;
	}

	int Benes_network::half_width() const {
		return 1 << (n - 1);
	}

	Benes_network_switches::Benes_network_switches(int _n) : n(_n) {
		switches.resize(depth() + 1, std::vector<std::vector<std::pair<int, int>>>(width()));
		for (int i(0); i != n; ++i) {
			for (int j(0); j != width(); ++j) {
				switches[i][j].push_back({ i + 1, j });
				switches[i + 1][j].push_back({ i, j });

				switches[i][j].push_back({ i + 1, j ^ (1 << (n - i - 1)) });
				switches[i + 1][j ^ (1 << (n - i - 1))].push_back({ i, j });
			}
		}
		for (int i(n); i != depth(); ++i) {
			for (int j(0); j != width(); ++j) {
				switches[i][j].push_back({ i + 1, j });
				switches[i + 1][j].push_back({ i, j });

				switches[i][j].push_back({ i + 1, j ^ (1 << (i - n + 1)) });
				switches[i + 1][j ^ (1 << (i - n + 1))].push_back({ i, j });
			}
		}
	}

	Benes_network route(int n, const std::vector<int>& dest) {
#define SUB_ROUTE(ind) (ind < bn.half_width() ? ind : ind - bn.half_width())
		if (dest.size() != (size_t(1) << n)) {
			std::cerr << "Benes_network: Invalid destination vector size" << std::endl;
			throw std::runtime_error("Benes_network: Invalid destination vector size");
		}
		Benes_network bn(n);
		if (n == 1) {
			if (dest[0] == 0) {
				bn[0][0] = 0;
				bn[0][1] = 1;
			} else {
				bn[0][0] = 1;
				bn[0][1] = 0;
			}
			return bn;
		}
		std::vector<int> ori(bn.width());
		for (int i(0); i != bn.width(); ++i) {
			ori[dest[i]] = i;
		}
		// Those who are conflict with each other cannot be routed to same sub-network.
		std::vector<std::vector<int>> conflict(bn.width());
		std::vector<int> solution(bn.width());
		for (int i(0); i != (bn.half_width()); ++i) {
			// Restriction posed by input network.
			conflict[i].push_back(i + (bn.half_width()));
			conflict[i + (bn.half_width())].push_back(i);
			// Restriction posed by input network.
			conflict[ori[i]].push_back(ori[i + (bn.half_width())]);
			conflict[ori[i + (bn.half_width())]].push_back(ori[i]);
		}
		for (int i(0); i != bn.width(); ++i) {
			std::queue<int> que;
			if (solution[i] == 0) {
				que.push(i);
				solution[i] = 1;
			}
			while (!que.empty()) {
				int now(que.front());
				que.pop();
				for (size_t j(0); j != conflict[now].size(); ++j) {
					if (solution[conflict[now][j]] == 0) {
						solution[conflict[now][j]] = -solution[now];
						que.push(conflict[now][j]);
					} else if (solution[conflict[now][j]] == solution[now]) {
						std::cerr << "Benes_network::route Error: cannot fix conflict." << std::endl;
						throw "Benes_network::route Error: cannot fix conflict.";
					}
				}
			}
		}
		std::vector<int> upper_dest(bn.half_width()), lower_dest(bn.half_width());
		for (int i(0); i != (1 << n); ++i) {
			int sub_route = SUB_ROUTE(i);
			if (solution[i] == 1) {
				bn[0][i] = sub_route; // Upper
				upper_dest[sub_route] = SUB_ROUTE(dest[i]);
				bn[2 * n - 2][upper_dest[sub_route]] = dest[i];
			} else {
				bn[0][i] = sub_route + bn.half_width(); // Lower
				lower_dest[sub_route] = SUB_ROUTE(dest[i]);
				bn[2 * n - 2][lower_dest[sub_route] + bn.half_width()] = dest[i];
			}
		}
		Benes_network upper = route(n - 1, upper_dest);
		Benes_network lower = route(n - 1, lower_dest);
		for (int i(1); i != 2 * n - 2; ++i) {
			for (int j(0); j != bn.half_width(); ++j) {
				bn[i][j] = upper[i - 1][j];
				bn[i][j + bn.half_width()] = lower[i - 1][j] + bn.half_width();
			}
		}
		return bn;
	}
	
	int Benes_network_switches::depth() const {
		return 2 * n - 1;
	}

	int Benes_network_switches::width() const {
		return 1 << n;
	}

	int Benes_network_switches::half_width() const {
		return 1 << (n - 1);
	}

	const benes_tasks& task_decompose(int logn, int batch, bool clear)
	{
		static std::unordered_map<int, Benes_network_switches> switches;
		static std::map<std::pair<int, int>, benes_tasks> mem;
		if (clear) {
			static benes_tasks null_ret;
			switches.clear();
			mem.clear();
			return null_ret;
		}
		if (mem.find({ logn, batch }) != mem.end()) {
			return mem[{logn, batch}];
		}
		benes_tasks ret;
		if (switches.find(logn) == switches.end()) {
			switches[logn] =  Benes_network_switches(logn);
		}
		Benes_network_switches& bn = switches[logn];
		std::vector<std::vector<bool>> vis(bn.depth() + 1, std::vector<bool>(bn.width()));
		for (int left(0); left < bn.depth(); left += batch) {
			int right = (left + batch < bn.depth() ? left + batch : bn.depth());
			ret.push_back({});
			for (int top(0); top != bn.width(); ++top) vis[left][top] = false;
			for (int top(0); top != bn.width(); ++top) {
				if (vis[left][top]) continue;
				std::queue<std::pair<int, int>> que; // dep / wid
				std::set<int> new_task;
				new_task.insert(top);
				que.push({ left, top });
				vis[left][top] = true;
				while (!que.empty()) {
					auto now = que.front();
					que.pop();
					for (auto to : bn.switches[now.first][now.second]) {
						if (to.first >= left && to.first <= right && !vis[to.first][to.second]) {
							vis[to.first][to.second] = true;
							que.push(to);
							new_task.insert(to.second);
						}
					}
				}
				ret.back().push_back(std::vector<int>(new_task.begin(), new_task.end()));
			}
		}
		mem[{logn, batch}] = ret;
		return mem[{logn, batch}];
	}

	std::vector<permutation> decompose(const Benes_network& bn, int batch) {
		std::vector<permutation> ret(bn.depth() / batch + (bn.depth() % batch != 0),
			permutation(bn.width()));
		int now(0);
		for (int i(0); i != bn.depth(); ++i) {
			permutation next(bn.width());
			for (int j(0); j != bn.width(); ++j) {
				next[bn[i][j]] = ret[now][j];
			}
			ret[now] = next;
			if ((i + 1) % batch == 0) {
				++now;
			}
		}
		for (size_t i(0); i != ret.size(); ++i) {
			permutation tmp(bn.width());
			for (int j(0); j != bn.width(); ++j) {
				tmp[ret[i][j]] = j;
			}
			ret[i] = tmp;
		}
		return ret;
	}

	permutation::permutation(int n)
		: dest(n) {
		for (int i(0); i != n; ++i) {
			dest[i] = i;
		}
	}

	permutation permutation::operator*(const permutation& other) const {
		permutation ret(dest.size());
		for (size_t i(0); i != dest.size(); ++i) {
			ret[dest[i]] = other[i];
		}
		return ret;
	}

	size_t permutation::size() const
	{
		return dest.size();
	}

	int& permutation::operator[](int pos) {
		return dest[pos];
	}

	int permutation::operator[](int pos) const{
		return dest[pos];
	}

	void dest_to_perm(permutation& dest)
	{
		int n = dest.dest.size();
		permutation perm(n);
		for (int i(0); i != n; ++i) perm[dest[i]] = i;
		dest = perm;
	}


	void desttask_to_permtask(int sz, std::vector<permutation>& dest_task)
	{
		permutation map(sz);
		for (auto& dest : dest_task) {
			for (size_t i(0); i != dest.size(); ++i) {
				map[dest[i]] = i;
			}
			dest = map;
		}
	}
}