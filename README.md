# MP-SPDZ Shuffle

This is an implementation of secure multi-party shuffle protocol based on [MP-SPDZ project](https://github.com/data61/MP-SPDZ).

It accompanies **Free Linear Online Phase for Secure Multiparty Shuffle** by
Jiacheng Gao, Yuan Zhang, Sheng Zhong, and Changyu Dong (ASIACRYPT 2026).
The [paper](https://eprint.iacr.org/2024/1936) describes the protocols and experiments.
License terms are in [LICENSE](LICENSE); OT submodules retain their own licenses.

## Installation

Run from the repository root on Ubuntu 24.04 x86-64 with AVX2:

The artifact ZIP includes the OT dependency sources. For a Git checkout, first
run `git submodule update --init --recursive deps/SimpleOT deps/SimplestOT_C deps/libOTe`.

```sh
sudo apt-get update
sudo apt-get install -y automake build-essential cmake curl git libboost-all-dev \
    libgmp-dev libsodium-dev libssl-dev libtool openssl python3-venv
python3 -m venv .venv
source .venv/bin/activate
python -m pip install -r requirements-runtime.txt
make my_shuffle_main.x CXX=g++ BUILD_JOBS=4
```

The Makefile builds the OT dependencies and `build/my_shuffle_main.x`. Use the
Ubuntu CMake package for the initial build. Binaries use `-march=native`;
rebuild on the machine that will run the experiments.

Validated dependencies:

| Component | Versions |
| --- | --- |
| Build tools | G++ 13.3.0, CMake 3.28.3, Make 4.3, Automake 1.16.5, Libtool 2.4.7 |
| Python | Python 3.12.3, tqdm 4.67.1 |
| Libraries | Boost 1.83.0, GMP 6.3.0, libsodium 1.0.18, OpenSSL 3.0.13 |
| SimpleOT | Unversioned snapshot, 2020-07-11 |
| SimplestOT_C | Unversioned snapshot, 2024-11-29 |
| libOTe | 1.5.0 (SoftSpoken fork, snapshot 2024-07-05) |
| cryptoTools | 1.9.0 (SoftSpoken fork, snapshot 2024-07-05) |

OT version numbers come from the source; snapshot dates are commit dates.
Exact revisions are pinned by Git submodules.

## Benchmark

The paper used Ubuntu 24.04, two Intel Xeon Gold 6326 processors at 2.90 GHz
(32 physical cores / 64 threads), and one process per party over loopback.
The validation host has 188 GiB RAM. Large configurations need substantial time
and memory; reduce the party/size lists below when resources are limited.

Each point compares `Song_shuffle` optimized for total time (Song1),
`Song_shuffle` optimized for online time (Song2), and `my_shuffle` (ours).
For a small initial run:

```sh
SHUFFLE_BENCHMARK_DIR=benchmark_results_quick SHUFFLE_BENCHMARK_PARTIES=3 SHUFFLE_BENCHMARK_LOGSZ=6 SHUFFLE_BENCHMARK_LOGBATCH=4 python3 -u my_benchmark.py malicious 15000
```

For the paper's full benchmark, run
`make benchmark BENCHMARK_BASE_DIR=benchmark_results_review`, or run the groups
and summary separately:

```sh
SHUFFLE_BENCHMARK_DIR=benchmark_results_review/mali_size SHUFFLE_BENCHMARK_PARTIES=2 SHUFFLE_BENCHMARK_LOGSZ=10,12,14,16,18 python3 -u my_benchmark.py malicious 10000
SHUFFLE_BENCHMARK_DIR=benchmark_results_review/mali_parties SHUFFLE_BENCHMARK_PARTIES=3,6,9,12,15 SHUFFLE_BENCHMARK_LOGSZ=12 python3 -u my_benchmark.py malicious 10000
SHUFFLE_BENCHMARK_BASE_DIR=benchmark_results_review python3 -u summarize_benchmarks.py mali --strict
```

| Command / output | Paper data |
| --- | --- |
| `mali_parties` benchmark | Section 7.2, Table 2 |
| `mali_size` benchmark | Section 7.3, Table 3 |
| `summarize_benchmarks.py mali --strict` | Both tables' communication and modeled running time |
| `mali_size/network_sweep.csv`, `n=2, logsz=18` | Section 7.4, Figures 3/4: size bandwidth/RTT panels |
| `mali_parties/network_sweep.csv`, `n=15, logsz=12` | Section 7.4, Figures 3/4: party bandwidth/RTT panels |

TLS certificates are prepared automatically. Existing results are resumed;
use a new output directory after changing the code. `SHUFFLE_BENCHMARK_LOGBATCH`
sets the candidate list (default `4,5,6,7,8,9,10`, excluding `10` at `logsz=18`).
`SHUFFLE_BENCHMARK_TIMEOUT` sets the per-candidate timeout in seconds (default
3600). Failures are recorded and cause a nonzero exit status.
For semi-honest comparisons, use `my_benchmark.py semi-parties` or
`make benchmark-semi` to compare `Chase_shuffle` and `semi_my_shuffle`.

## Output

`raw_measurements_v2.csv` records measurements and failures. Per-protocol CSVs
contain selected results; `mali_summary.md` summarizes the paper's table data.
`network_sweep.csv` is generated automatically. To regenerate
it from measurements:

```sh
python3 derive_network_sweeps.py benchmark_results_review/mali_size/raw_measurements_v2.csv --strict
python3 derive_network_sweeps.py benchmark_results_review/mali_parties/raw_measurements_v2.csv --strict
```

The executable outputs six values: offline bytes, rounds, local seconds, then
online bytes, rounds, local seconds. Bytes and local time are averaged across
parties; rounds represent global protocol depth. The benchmark uses one
repetition; multiple repetitions report amortized values. Divide bytes by
`1,000,000` for the paper's decimal MB.

Modeled time is `local_seconds + bytes / (bandwidth_MBps * 1,000,000) + rounds * RTT_ms / 1,000`. Table timings use 80 MB/s and 60 ms RTT. Bandwidth
sweeps fix RTT at 0.5 ms; RTT sweeps fix bandwidth at 80 MB/s. Each setting
selects its decomposition parameter for the stated optimization target.
MPC-backend preprocessing contributes time and bytes; its internal rounds are
excluded from the protocol-depth counter.

Round accounting has been corrected since the paper measurements, so modeled
timings can differ. Local times also depend on hardware and load.

## Source organization and changes to MP-SPDZ

`MyShuffle/` adds the shuffle protocols (`my_shuffle`, `Song_shuffle`,
`semi_my_shuffle`, and `Chase_shuffle`), MPC/OT helpers, and the executable entry
`my_shuffle_main.cpp`. `mpc_communicator.cpp` handles communication;
`my_benchmark.cpp` measures protocol execution.

The added benchmark scripts run experiments (`my_benchmark.py`), derive network
sweeps (`derive_network_sweeps.py`), and produce summaries
(`summarize_benchmarks.py`). CMake/Make, TLS setup, the local launcher
(`Scripts/run-shuffle.py`), and CI integrate these additions with MP-SPDZ.

## AI acknowledgement

The main body of the code, including MPC building blocks and the shuffle protocols, are written manually by Jiacheng Gao in 2024 and 2025.

OpenAI Codex later assisted with artifact documentation, writing benchmark scripts, and packaging the artifact for evaluations. The main body of this README is also written by Codex.
