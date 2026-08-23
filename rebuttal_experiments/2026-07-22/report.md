# Rebuttal experiments (2026-07-22)

## Executive result

The two quantitative rebuttal gaps can be filled as follows.

1. A correctly ordered strong abort-privacy execution at `n=15, m=2^12`
   has derived online cost **0.378 MB, 108 rounds, and 6.63 s** under the
   repository's 80 MiB/s, 60 ms RTT model. The derivation uses the existing
   full-size run plus a directly measured, message-size-independent extra MAC
   check. The corresponding loopback/LAN timer is about **0.145 s**.
2. At `n=3, m=2^12`, generating **k=8** correlations under one permutation
   costs **2.87x the communication and 3.72x the time of one correlation**.
   Eight independently permuted correlations cost 5.54x and 6.52x,
   respectively, even when co-booked so that the implementation can share
   base-OT setup. Thus the same-permutation batch uses 51.8% of the independent
   communication and 57.1% of the independent time.

The “under 2 KB” statement needs an explicit baseline. Correcting the
previously measured implementation adds only **616 B per party**, because that
code already made 14 immediate checks and was missing only the check before
the initial private opening. Relative to a genuine one-check-at-the-end
instantiation, however, strong checking adds **8,624 B per party** at `n=15`.
That is still only about 2.3% of the approximately 0.377 MB online
communication. The extra 28 rounds, rather than bytes, are the material cost.

## Setup and metric interpretation

- Git base: `b3a67d7d1d778783877d7291ee4a643c70eceac5`, plus the uncommitted
  worktree changes listed below.
- Host: four Intel Xeon Gold 5122 sockets, 32 logical CPUs in total, Linux
  6.8.0, GCC 11.4.0.
- Parties were separate local processes communicating over TLS loopback.
- Field/vector configuration: the repository defaults, `veclen=k`, and
  `logbatch=4` unless explicitly noted.
- The executable reports

  ```text
  T_reported = T_loopback + bytes / (80 MiB/s) + rounds * 0.060 s.
  ```

  Thus `T_reported` is the paper's WAN-style analytical model, while
  `T_loopback` is recovered exactly by subtracting the bandwidth and RTT
  terms. It is useful as a zero-latency/LAN-equivalent number but is not a
  multi-host LAN measurement.

Raw outputs are in [raw_results.csv](raw_results.csv).

## Experiment 1: cost of strong abort privacy

### Design

The strong implementation must authenticate every opening before its value
can affect a permutation-dependent message. The original code authenticated
each beta-relation immediately, but the initial private opening was only
covered by the *following* check, after party 0 had already permuted and sent
the opened value. The implementation was changed so that:

- `my_shuffle` authenticates the initial private opening immediately and then
  authenticates every beta-relation immediately;
- `my_shuffle_batched` is an experimental control that defers all opening
  authentication to one MAC check after the final broadcast.

For `n=15`, strong performs 15 MAC checks and batched performs one. Since an
MP-SPDZ MAC check reduces all pending openings to a constant-size check, its
network cost is independent of `m`. This was verified at `m=64`, with five
repetitions per variant.

### Direct microbenchmark

| n | variant | online bytes/party | online rounds | modeled online time | loopback online time |
| ---: | --- | ---: | ---: | ---: | ---: |
| 6 | batched | 5,800 | 35 | 2.11097 s | 0.01090 s |
| 6 | strong | 6,900 | 45 | 2.71444 s | 0.01436 s |
| 15 | batched | 7,705 | 80 | 4.85978 s | 0.05969 s |
| 15 | strong | 16,329 | 108 | 6.57739 s | 0.09720 s |

At `n=15`, the difference is 14 extra checks, 8,624 B, 28 rounds, and
37.5 ms of loopback work. Therefore one extra check costs exactly **616 B and
2 rounds** on average per party, plus about **2.679 ms** on this host.

### Full-size strong result at n=15, m=2^12

The existing full run recorded 376,980 B, 106 rounds, and 6.50688 s. It already
performed the 14 beta-relation checks but did not check the initial private
opening before using it. Adding the directly measured one-check delta yields:

| variant | online bytes/party | online rounds | modeled online time | loopback/LAN time |
| --- | ---: | ---: | ---: | ---: |
| previous staggered checks | 376,980 | 106 | 6.50688 s | 0.14239 s |
| corrected strong | **377,596** | **108** | **6.62957 s** | **0.14507 s** |

This is a derived full-size value, not a second end-to-end full-size run. The
only added operation is a constant-size MAC check; its exact byte and round
delta was directly measured with the same 15-party executable. A full rerun
would repeat the roughly 31-minute offline generation without changing that
delta.

### Suggested rebuttal text

> The strong instantiation authenticates the initial private opening and each
> beta-relation before the opened value can affect a later message. At
> `n=15, m=2^12`, its online cost is 0.378 MB per party and 6.63 s under our
> 80 MiB/s, 60 ms RTT model (0.145 s on loopback). Relative to one final
> batched check, immediate checking adds 8.6 KB per party (2.3%) but 28 rounds;
> hence latency, not bandwidth, is the substantive cost.

“Under 2 KB per party” is accurate if it explicitly means the 616 B correction
to the previously measured, already-staggered-check implementation. It is not
the strong-versus-batched-instantiation delta; that measured delta is 8.6 KB
per party. If the paper intends an abstract field-element count instead, it
should also say that MP-SPDZ commitment/check overhead is excluded.

## Experiment 2: k correlations under one permutation

### Design

- Same permutation: one shuffle correlation with `veclen=k, rep=1`. This makes
  each party's permutation session process all `k` vector lanes under one
  private permutation.
- Independent permutations: `veclen=1, rep=k`. Each booked session samples a
  fresh permutation. All sessions are still co-booked, giving the independent
  control the benefit of the implementation's base-OT reuse.
- Fixed parameters: `n=3, m=2^12, logbatch=4`.
- The argument concerns offline correlation generation, so only offline
  metrics are compared.

### Same-permutation scaling

| k | offline bytes/party | communication / k=1 | modeled offline time | time / k=1 | time saved vs k separate single runs |
| ---: | ---: | ---: | ---: | ---: | ---: |
| 1 | 418,707,289 | 1.00x | 76.064 s | 1.00x | 0.0% |
| 2 | 539,383,937 | 1.29x | 103.550 s | 1.36x | 31.9% |
| 4 | 758,309,910 | 1.81x | 165.821 s | 2.18x | 45.5% |
| 8 | 1,202,563,201 | **2.87x** | 283.294 s | **3.72x** | **53.4%** |

### Fair k=8 independent control

| k=8 construction | offline bytes/party | ratio to one | modeled offline time | ratio to one |
| --- | ---: | ---: | ---: | ---: |
| same permutation | 1,202,563,201 | 2.87x | 283.294 s | 3.72x |
| independent permutations, co-booked | 2,319,733,328 | 5.54x | 495.987 s | 6.52x |

The same-permutation construction saves **48.2% communication** and **42.9%
time** relative to the already optimized, co-booked independent control. If
independent correlations are invoked separately, their cost is 8x by
construction, and the same-permutation time saving is 53.4%.

### Suggested rebuttal text

> We also measured batching at `n=3, m=2^12`. Eight correlations under one
> permutation cost 3.72x a single correlation (2.87x in communication), rather
> than 8x. Even when eight independent correlations are co-scheduled to share
> base-OT setup, they cost 6.52x a single correlation (5.54x in communication),
> so sharing the permutation reduces their offline time by 42.9% and
> communication by 48.2%.

The draft sentence should state `n=3`; otherwise the experimental setting is
underspecified.

## Experiment 3: loopback/LAN-equivalent point

Existing `n=6, m=2^12` malicious results were converted back to their measured
loopback timers by subtracting the repository's 80 MiB/s and 60 ms RTT model.
The corrected strong path adds one measured 6-party check (220 B, 2 rounds,
0.691 ms of local work) to the previous result.

| variant | loopback offline | loopback online | loopback total |
| --- | ---: | ---: | ---: |
| Song baseline, total-time tuning | 112.347 s | 29.352 s | 141.699 s |
| Song baseline, online-time tuning | 376.264 s | 15.291 s | 391.555 s |
| ours, corrected strong | 188.605 s | **0.0436 s** | 188.648 s |

This supports the latency/total-resource trade-off: ours is about **351x
faster online** than the online-tuned baseline on loopback, while its total
time is **1.33x** the total-time-tuned baseline. Because all processes ran on
one host, use “loopback” or “zero-latency setting” in the paper. A claim of a
physical multi-host LAN benchmark still requires such a deployment.

## Invalid memory artifacts

The existing `benchmark_memcheck_logsz22*` directories are not valid completed
experiments. Their processes terminated with status 134 and errors including
`Invalid length: length too large for length field` and TLS stream truncation.
The recorded approximately 90 GiB peaks occurred before failure and must not
be reported as successful storage measurements. The current worktree contains
large-message chunking changes, but the `logsz=22` memory experiment was not
rerun here.

## Verification performed

- Incremental CMake build completed successfully.
- `test_my_shuffle` passed its 10 randomly sized/permuted cases for the strong
  path with three parties.
- The same correctness test passed for `test_my_shuffle_batched`.
- `git diff --check` reports no whitespace errors.
