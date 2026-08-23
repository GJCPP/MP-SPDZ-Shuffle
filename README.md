# MP-SPDZ Shuffle

This is an implementation of secure multi-party shuffle protocol based on [MP-SPDZ project](https://github.com/data61/MP-SPDZ).

The shuffle implementation was started from MP-SPDZ commit
`bf38ddbc6bf164b67d3c4175921bd62c90e70308`. Third-party dependencies are
pinned through Git submodules and should be initialized with
`git submodule update --init --recursive`.

## Compilation

Please read [the instruction of MP-SPDZ](https://github.com/data61/MP-SPDZ) for setting up the MP-SPDZ part of the project.

Note that in MP-SPDZ, you need to run `Scripts/setup-ssl.sh <nparties>` for setting up communication channel before executing an n-party protocols.

Build the shuffle executable with CMake:

```
cmake -S . -B build
cmake --build build --target my_shuffle_main
```

This writes build artifacts under `build/`, including `build/my_shuffle_main.x`.
For compatibility, `make my_shuffle_main.x` runs the same CMake build.

In directory `MP-SPDZ-Shuffle`, use `make example` to run a 3-party example, which translates to


```
for i in 0 1 2; do ./build/my_shuffle_main.x my_shuffle $$i 3 6 1 1 10000 1 & true; done
```

The arguments are explained in later section.

## Benchmark

The dedicated benchmark entry points are:

- `make benchmark-semi`
- `make benchmark-mali`
- `make benchmark-strong`

Use `make benchmark-semi` to run the semi-honest benchmark suite:

```
Scripts/setup-ssl.sh 20
SHUFFLE_BENCHMARK_DIR=benchmark_results_bw80_rtt60/semi_size SHUFFLE_BENCHMARK_PARTIES=2 SHUFFLE_BENCHMARK_LOGSZ=14,16,18,20,22 python3 -u my_benchmark.py semi-parties 10000
SHUFFLE_BENCHMARK_DIR=benchmark_results_bw80_rtt60/semi_parties SHUFFLE_BENCHMARK_PARTIES=3,6,9,12,15 SHUFFLE_BENCHMARK_LOGSZ=16 python3 -u my_benchmark.py semi-parties 10000
SHUFFLE_BENCHMARK_BASE_DIR=benchmark_results_bw80_rtt60 python3 -u summarize_benchmarks.py semi --strict
```

Use `make benchmark-mali` to run the malicious benchmark suite:

```
Scripts/setup-ssl.sh 20
SHUFFLE_BENCHMARK_DIR=benchmark_results_bw80_rtt60/mali_size SHUFFLE_BENCHMARK_PARTIES=2 SHUFFLE_BENCHMARK_LOGSZ=10,12,14,16,18 python3 -u my_benchmark.py malicious 10000
SHUFFLE_BENCHMARK_DIR=benchmark_results_bw80_rtt60/mali_parties SHUFFLE_BENCHMARK_PARTIES=3,6,9,12,15 SHUFFLE_BENCHMARK_LOGSZ=12 python3 -u my_benchmark.py malicious 10000
SHUFFLE_BENCHMARK_BASE_DIR=benchmark_results_bw80_rtt60 python3 -u summarize_benchmarks.py mali --strict
```

Use `make benchmark-strong` to run only the strong abort-privacy variant of
`my_shuffle`, using the same size and party settings as `benchmark-mali`:

```
Scripts/setup-ssl.sh 20
SHUFFLE_BENCHMARK_DIR=benchmark_results_bw80_rtt60/strong_size SHUFFLE_BENCHMARK_PARTIES=2 SHUFFLE_BENCHMARK_LOGSZ=10,12,14,16,18 python3 -u my_benchmark.py my_shuffle_strong 10000
SHUFFLE_BENCHMARK_DIR=benchmark_results_bw80_rtt60/strong_parties SHUFFLE_BENCHMARK_PARTIES=3,6,9,12,15 SHUFFLE_BENCHMARK_LOGSZ=12 python3 -u my_benchmark.py my_shuffle_strong 10000
SHUFFLE_BENCHMARK_BASE_DIR=benchmark_results_bw80_rtt60 python3 -u summarize_benchmarks.py strong --strict
```

Use `make benchmark` to run both suites in sequence.

The four benchmark groups are:

- Semi-honest size scale: fixed `n = 2`, `logsz = 14, 16, 18, 20, 22`.
- Semi-honest party scale: fixed `logsz = 16`, `n = 3, 6, 9, 12, 15`.
- Malicious size scale: fixed `n = 2`, `logsz = 10, 12, 14, 16, 18`.
- Malicious party scale: fixed `logsz = 12`, `n = 3, 6, 9, 12, 15`.

The semi-honest and malicious summary tables have three rows per point:
baseline optimized for total time, baseline optimized for online time, and
ours. The strong summary has one `my_shuffle_strong` row per point.

Benchmark CSVs, temporary party-0 stdout, and summary tables are written under
`benchmark_results_bw80_rtt60/` by default.

Use `make my_shuffle_main.x` to build `build/my_shuffle_main.x`.

## Arguments

The arguments are: \<protocol name\>, \<party id\>, \<num of parties\>, \<log num of items\>, \<vector length\>, \<logbatch\>, \<port base\>, \<num of repetitions\>.

So the command `make example`, which translates to

```
for i in 0 1 2; do ./build/my_shuffle_main.x my_shuffle $$i 3 6 1 1 10000 1 & true; done
```

launches a shuffle protocol that repeats once, which shuffles $2^6$ many $1$-sized vectors among $3$ parties.

By default, at the end of the execution, each party outputs its offline communication (bytes), offline time (seconds), online communication and online time.

See also `MyShuffle/my_shuffle_main.cpp`.

`my_shuffle` defaults to the weak/batched-check ordering: all online openings
are authenticated together at the end of each shuffle. The strong
abort-privacy ordering remains available through the C++ API by passing
`strong_abort_privacy = true`; it authenticates every opening before its value
affects a later permutation-dependent message.

Use `make benchmark-strong` to benchmark only this strong mode. The target
selects the dedicated `my_shuffle_strong` protocol entry.

## Shuffle Protocols

| Protocol | Security | Party | Offline | Online | Framework |
| -------- | -------- | ----- | ------- | ------ | --------- |
| [1] [Chase et al.](https://link.springer.com/chapter/10.1007/978-3-030-64840-4_12) | Semi-honest | $2$ | $O(m\log m)$ | $O(m)$ | SPDZ |
| [2] [Song et al.](https://www.ndss-symposium.org/wp-content/uploads/2024-21-paper.pdf) | Malicious | $n$ | $O(Bn^2m\log m)$ | $O(Bn^2m)$ | SPDZ |
| [3] This paper | Malicious | $n$ | $O(Bn^2m\log m)$ | $O(nm)$ | Arbitrary |

$n$ is the number of parties, $m$ the number of items (to be shuffled), $B$ a parameter related to security parameter.

In context, [2] can be seen as an enhancement to [1], and [3] is instantiated by [2]. Note that [3] can also be implemented with other secret sharing scheme (e.g. Shamir SS) and other basic permutation protocol. In this project, [3] is instantiated by SPDZ and [2].

Protocol [1] includes an additional parameter $k$ ("logbatch" in code) for balancing communication and computation, which is inherited by [2] and [3]. Increasing $k$ reduces communication and increases computation.

The malicious benchmark suite compares [2] and the malicious instantiation
of [3]. The semi-honest benchmark suites compare [1] and the semi-honest
instantiation of [3], including an n-party Chase baseline formed by composing
one Chase permutation session per party.

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

  - `semi_my_shuffle.cpp/h` implements a semi-honest version of `my_shuffle`
    using `Chase_shuffle` as the permutation primitive.

MPC gadgets:

  - `mpc_communicator.cpp/h` defines interface for interactions between parties.

  - `mpc_gadgets.cpp/h` includes debugging tools.

  - `OPV.cpp/h`, `Benes_network.cpp`, `double_length_prg.cpp`, etc. define primitives required by shuffle protocols.

More details can be found in corresponding header files.

## Citation

TODO
