# MP-SPDZ Shuffle

This is an implementation of secure multi-party shuffle protocol based on [MP-SPDZ project](https://github.com/data61/MP-SPDZ).

## Compilation

Please read [the instruction of MP-SPDZ](https://github.com/data61/MP-SPDZ) for setting up the MP-SPDZ part of the project.

Note that in MP-SPDZ, you need to run `Scripts/setup-ssl.sh <nparties>` for setting up communication channel before executing an n-party protocols.

Build the shuffle executable with CMake:

```
cmake -S . -B build
cmake --build build --target my_shuffle_main
```

This writes build artifacts under `build/`, including `build/my_shuffle_main.x`.

In directory `MP-SPDZ-Shuffle`, use `make example` to run a 3-party example, which translates to

```
for i in 0 1 2; do ./build/my_shuffle_main.x my_shuffle $$i 3 6 1 1 10000 1 & true; done
```

The arguments are explained in later section.

## Benchmark

Use `make benchmark` to run only the benchmark. Every point includes three variants: `Song_shuffle` optimized for total time (Song1), `Song_shuffle` optimized for online time (Song2), and the unified malicious `my_shuffle` protocol:

```
Scripts/setup-ssl.sh 20
SHUFFLE_BENCHMARK_DIR=benchmark_results_network_sweeps/mali_size SHUFFLE_BENCHMARK_PARTIES=2 SHUFFLE_BENCHMARK_LOGSZ=10,12,14,16,18 python3 -u my_benchmark.py malicious 10000
SHUFFLE_BENCHMARK_DIR=benchmark_results_network_sweeps/mali_parties SHUFFLE_BENCHMARK_PARTIES=3,6,9,12,15 SHUFFLE_BENCHMARK_LOGSZ=12 python3 -u my_benchmark.py malicious 10000
SHUFFLE_BENCHMARK_BASE_DIR=benchmark_results_network_sweeps python3 -u summarize_benchmarks.py mali --strict
```

The two benchmark groups are:

- Malicious size scale: fixed `n = 2`, `logsz = 10, 12, 14, 16, 18`.
- Malicious party scale: fixed `logsz = 12`, `n = 3, 6, 9, 12, 15`.

## Arguments

The arguments are: \<protocol name\>, \<party id\>, \<num of parties\>, \<log num of items\>, \<vector length\>, \<logbatch\>, \<port base\>, \<num of repetitions\>.

So the command `make example`, which translates to

```
for i in 0 1 2; do ./build/my_shuffle_main.x my_shuffle $$i 3 6 1 1 10000 1 & true; done
```

launches a shuffle protocol that repeats once, which shuffles $2^6$ many $1$-sized vectors among $3$ parties.

## Shuffle Protocols

| Protocol | Security | Party | Offline | Online | Framework |
| -------- | -------- | ----- | ------- | ------ | --------- |
| [1] [Chase et al.](https://link.springer.com/chapter/10.1007/978-3-030-64840-4_12) | Semi-honest | $2$ | $O(m\log m)$ | $O(m)$ | SPDZ |
| [2] [Song et al.](https://www.ndss-symposium.org/wp-content/uploads/2024-21-paper.pdf) | Malicious | $n$ | $O(Bn^2m\log m)$ | $O(Bn^2m)$ | SPDZ |
| [3] This paper | Malicious | $n$ | $O(Bn^2m\log m)$ | $O(nm)$ | Arbitrary |

$n$ is the number of parties, $m$ the number of items (to be shuffled), $B$ a parameter related to security parameter.

In context, [2] can be seen as an enhancement to [1], and [3] is instantiated by [2]. Note that [3] can also be implemented with other secret sharing scheme (e.g. Shamir SS) and other basic permutation protocol. In this project, [3] is instantiated by SPDZ and [2].

Protocol [1] includes an additional parameter $k$ ("logbatch" in code) for balancing communication and computation, which is inherited by [2] and [3]. Increasing $k$ reduces communication and increases computation.

The malicious benchmark suite compares [2] and the malicious instantiation of [3].

## File Structure

Directory `MyShuffle` includes all codes for implementing shuffle protocols. All files mentioned below are in this directory.

Entries:

  - `my_shuffle_main.cpp` defines the entry of the program.

  - `my_benchmark.cpp/h` helps execute the corresponding shuffle protocol and records test outcomes, including offline/online communication and time.

  - `test_shuffle.cpp/h` defines the (correctness) test for shuffle protocols.

  - `unit_test.cpp/h` defines unit test for gadgets.

Shuffle protocols:

  - `Chase_shuffle.cpp/h` implements the shuffle protocol by [Chase et al.](https://link.springer.com/chapter/10.1007/978-3-030-64840-4_12).

  - `Song_shuffle.cpp/h` implements the shuffle protocol by [Song et al.](https://www.ndss-symposium.org/wp-content/uploads/2024-21-paper.pdf).

  - `my_shuffle.cpp/h` implements the shuffle protocol by this paper.

MPC gadgets:

  - `mpc_communicator.cpp/h` defines interface for interactions between parties.

  - `mpc_gadgets.cpp/h` includes debugging tools.

  - `OPV.cpp/h`, `Benes_network.cpp`, `double_length_prg.cpp`, etc. define primitives required by shuffle protocols.

More details can be found in corresponding header files.

## Citation

TODO
