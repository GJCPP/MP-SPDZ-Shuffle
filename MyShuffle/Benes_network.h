#pragma once
#include <vector>
#include <set>
#include <unordered_map>
#include <random>

namespace BenesNetwork
{
	class layer_dest {
	public:
		layer_dest(int n);

		int& operator[](int pos);
		int operator[](int pos) const;
		std::vector<int> dest;
	};

	class Benes_network {
	public:
		Benes_network(int n);

		layer_dest& operator[](int pos);
		const layer_dest& operator[](int pos) const;

		int depth() const;
		int width() const;
		int half_width() const;

		int n;
		std::vector<layer_dest> layers;
	};

	class Benes_network_switches {
	public:
		Benes_network_switches() = default;
		Benes_network_switches(int n);

		int depth() const;
		int width() const;
		int half_width() const;

		int n;
		std::vector<std::vector<std::vector<std::pair<int, int>>>> switches; // Adjacent list
	};

	class permutation {
	public:
		permutation() = default;
		permutation(int n);

		permutation operator*(const permutation& other) const;

		size_t size() const;

		int operator[](int pos) const;
		int& operator[](int pos);

		std::vector<int> dest;
	};

	/*
	* Parameter dest should be a permutation over [0, 2^n-1].
	* This function returns a Benes network that routes according to dest.
	*/
	Benes_network route(int n, const std::vector<int>& dest);

	/*
	* Structure: task[depth][task] = { all touched switches }
	*/
	typedef std::vector<std::vector<std::vector<int>>> benes_tasks;


	const benes_tasks& task_decompose(int logn, int batch, bool clear = false);

	void dest_to_perm(permutation& perm);
	void desttask_to_permtask(int sz, std::vector<permutation>& perm);

	std::vector<permutation> decompose(const Benes_network& bn, int batch);
}
