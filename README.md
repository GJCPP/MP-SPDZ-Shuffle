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
SHUFFLE_BENCHMARK_DIR=benchmark_results_network_sweeps/semi_size SHUFFLE_BENCHMARK_PARTIES=2 SHUFFLE_BENCHMARK_LOGSZ=14,16,18,20,22 python3 -u my_benchmark.py semi-parties 10000
SHUFFLE_BENCHMARK_DIR=benchmark_results_network_sweeps/semi_parties SHUFFLE_BENCHMARK_PARTIES=3,6,9,12,15 SHUFFLE_BENCHMARK_LOGSZ=16 python3 -u my_benchmark.py semi-parties 10000
SHUFFLE_BENCHMARK_BASE_DIR=benchmark_results_network_sweeps python3 -u summarize_benchmarks.py semi --strict
```

Use `make benchmark-mali` to run the malicious benchmark suite. Every point
includes four variants: `Song_shuffle` optimized for total time (Song1),
`Song_shuffle` optimized for online time (Song2), weak `my_shuffle`, and strong
`my_shuffle_strong`. Both Song variants and `my_shuffle_strong` authenticate
every permutation-dependent intermediate state before returning; weak
`my_shuffle` deliberately retains its deferred online MAC-check mode:

```
Scripts/setup-ssl.sh 20
SHUFFLE_BENCHMARK_DIR=benchmark_results_network_sweeps/mali_size SHUFFLE_BENCHMARK_PARTIES=2 SHUFFLE_BENCHMARK_LOGSZ=10,12,14,16,18 python3 -u my_benchmark.py malicious 10000
SHUFFLE_BENCHMARK_DIR=benchmark_results_network_sweeps/mali_parties SHUFFLE_BENCHMARK_PARTIES=3,6,9,12,15 SHUFFLE_BENCHMARK_LOGSZ=12 python3 -u my_benchmark.py malicious 10000
SHUFFLE_BENCHMARK_BASE_DIR=benchmark_results_network_sweeps python3 -u summarize_benchmarks.py mali --strict
```

Use `make benchmark-strong` to run only the strong abort-privacy variant of
`my_shuffle`, using the same size and party settings as `benchmark-mali`:

```
Scripts/setup-ssl.sh 20
SHUFFLE_BENCHMARK_DIR=benchmark_results_network_sweeps/strong_size SHUFFLE_BENCHMARK_PARTIES=2 SHUFFLE_BENCHMARK_LOGSZ=10,12,14,16,18 python3 -u my_benchmark.py my_shuffle_strong 10000
SHUFFLE_BENCHMARK_DIR=benchmark_results_network_sweeps/strong_parties SHUFFLE_BENCHMARK_PARTIES=3,6,9,12,15 SHUFFLE_BENCHMARK_LOGSZ=12 python3 -u my_benchmark.py my_shuffle_strong 10000
SHUFFLE_BENCHMARK_BASE_DIR=benchmark_results_network_sweeps python3 -u summarize_benchmarks.py strong --strict
```

Use `make benchmark` to run both suites in sequence.

The four benchmark groups are:

- Semi-honest size scale: fixed `n = 2`, `logsz = 14, 16, 18, 20, 22`.
- Semi-honest party scale: fixed `logsz = 16`, `n = 3, 6, 9, 12, 15`.
- Malicious size scale: fixed `n = 2`, `logsz = 10, 12, 14, 16, 18`.
- Malicious party scale: fixed `logsz = 12`, `n = 3, 6, 9, 12, 15`.

The semi-honest summary has three rows per point: baseline optimized for total
time, baseline optimized for online time, and ours. The malicious summary has
four rows per point: Song1, Song2, weak `my_shuffle`, and strong
`my_shuffle_strong`. The dedicated strong summary has one
`my_shuffle_strong` row per point.

Benchmark CSVs, temporary party-0 stdout, and summary tables are written under
`benchmark_results_network_sweeps/` by default. This separate directory avoids
mixing the v2 compute-time measurements with legacy modeled-time CSVs.

Each protocol execution records network-independent compute time,
communication bytes, and rounds in `raw_measurements_v2.csv`. The benchmark
driver keeps the existing result CSVs at the default decimal `80 MB/s` and
`60 ms` RTT, and also rewrites `network_sweep.csv` after every successful
candidate run. The sweep file independently selects the best `logbatch` for
each network setting and contains both:

- fixed `80 MB/s`, RTT `0.5, 1, 5, 20, 60, 100 ms`;
- fixed `0.5 ms` RTT, bandwidth `12.5, 80, 125, 312.5, 1250 MB/s`.

Modeled phase time is `compute_seconds + comm_bytes / (MBps * 1,000,000) +
rounds * RTT_ms / 1,000`. Thus `MB/s` is decimal, not MiB/s. The sweep can be
regenerated without running the MPC protocols:

```
python3 -u derive_network_sweeps.py \
    benchmark_results_network_sweeps/mali_size/raw_measurements_v2.csv \
    --strict
```

For plots, use `rtt_ms` or `bandwidth_MBps` from `network_sweep.csv` as a
log-scale x-axis. The CSV stores untransformed numeric values.

Use `make my_shuffle_main.x` to build `build/my_shuffle_main.x`.

## Arguments

The arguments are: \<protocol name\>, \<party id\>, \<num of parties\>, \<log num of items\>, \<vector length\>, \<logbatch\>, \<port base\>, \<num of repetitions\>.

So the command `make example`, which translates to

```
for i in 0 1 2; do ./build/my_shuffle_main.x my_shuffle $$i 3 6 1 1 10000 1 & true; done
```

launches a shuffle protocol that repeats once, which shuffles $2^6$ many $1$-sized vectors among $3$ parties.

At the end of an execution, party 0 outputs offline communication (bytes),
rounds, and measured compute time (seconds), followed by the corresponding
online values. Network-model time is derived by `my_benchmark.py`.

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
