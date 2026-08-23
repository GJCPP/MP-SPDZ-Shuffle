# Semi-Honest `my_shuffle` Design Notes

This note records the intended direction for implementing a semi-honest version
of `my_shuffle` using `Chase_shuffle` as the underlying permutation primitive.
It is a design record only; it does not describe code that already exists.

## Goal

Implement a new semi-honest shuffle protocol that follows the same high-level
offline/online shape as the current malicious `my_shuffle`, but replaces the
Song-based malicious permutation primitive with the Chase semi-honest primitive.

The semi-honest version should avoid all malicious-only machinery:

- no SPDZ MAC permutation
- no `beta`, `beta_r`, or `rp`
- no MAC verification checks
- no multiplication triples for MAC equations
- no malicious OPV bucket/check amplification from `Song_shuffle`

The protocol should still support `n` parties, each contributing one private
permutation, and should output additive shares of the final shuffled vector.

## Protocol Shape

Let party `i` hold a private permutation `pi_i`. Let the input be additively
shared across parties.

Offline:

1. For every party `i`, generate a random additive share vector `r_i`.
2. Precompute `pi_i(r_i)` using a Chase-based permutation session.
3. For every party `i > 0`, privately give party `i` the clear correction
   value:

   ```text
   z_i = pi_{i-1}(r_{i-1}) - r_i
   ```

Online:

1. Party 0 privately opens `x - r_0` to itself.
2. Party 0 applies `pi_0` locally and sends the clear masked value to party 1.
3. Party `i > 0` receives the clear masked value, adds `z_i`, applies `pi_i`,
   and forwards it to party `i + 1`.
4. The last party broadcasts the final clear masked value.
5. Each party outputs its share of `pi_{n-1}(r_{n-1})`; one designated party
   also adds the broadcast clear masked value.

This mirrors the current malicious `my_shuffle` flow, but the second MAC lane
is removed.

## Code Implications

The current malicious implementation lives in:

- `MyShuffle/my_shuffle.h`
- `MyShuffle/my_shuffle.cpp`

The current Song permutation primitive lives in:

- `MyShuffle/Song_shuffle.h`
- `MyShuffle/Song_shuffle.cpp`

The Chase semi-honest two-party permutation primitive lives in:

- `MyShuffle/Chase_shuffle.h`
- `MyShuffle/Chase_shuffle.cpp`

The semi-honest implementation should be added separately rather than changing
the malicious implementation in place. Suggested names:

- `MyShuffle/semi_my_shuffle.h`
- `MyShuffle/semi_my_shuffle.cpp`

or a similarly explicit namespace/file pair.

## Main Design Decisions

### Data Representation

The existing malicious `my_shuffle` uses `vectors<ShareType>`, where `ShareType`
is an SPDZ share with a MAC. For the semi-honest version, the cleaner target is
to use `vectors<ClearType>` as local additive shares.

This avoids accidental use of:

- `ShareType::get_mac()`
- `ShareType::set_mac()`
- `ShareType::get_mac_key()`
- `com.mac_check()`

### Chase Primitive Adaptation

`Chase_shuffle` currently works over `vectors<block_wrapper>`. To integrate
cleanly with field-sharing tests and benchmarks, it should either be generalized
or wrapped to work over `vectors<ClearType>`.

The preferred route is to adapt the Chase permutation primitive so its masks are
expanded into `ClearType` values, similar to how `Song_shuffle` uses
`arbitrary_prg(..., vectors<ClearType>&)`.

### N-Party Permutation Session

`Chase_shuffle` is a two-party sender/permuter primitive. The semi-honest
shuffle needs an n-party permutation session analogous to the clear-value path
in `song2023::permute_session::perform()`:

1. Fix one `permuter`.
2. For each `sender != permuter`, prepare a Chase `shuffle_pair`.
3. During online permutation, have each sender's additive share transformed
   under the permuter's private permutation.
4. The permuter combines the dummy/permuted contributions so all local shares
   represent the globally permuted vector.

This should be much simpler than `Song_shuffle` because semi-honest security
does not require malicious bucket repetition or MAC checks.

## Implementation Plan

1. Add a separate semi-honest module and namespace.
2. Decide and implement the `ClearType` Chase adapter/generalization.
3. Implement a Chase-based n-party `permute_session`.
4. Implement semi-honest `shuffle_session` with only `r`, `permuted_r`, and
   `z`.
5. Add a protocol name such as `semi_my_shuffle` to `my_shuffle_main.cpp`.
6. Add correctness tests mirroring `test_my_shuffle`, but using additive
   `ClearType` shares and no MAC checks.
7. Add benchmark support, keeping semi-honest results separate from malicious
   `my_shuffle` and `Song_shuffle`.

## Risks

- The largest engineering risk is the current type mismatch between
  `Chase_shuffle` (`block_wrapper`) and the rest of the shuffle benchmark
  (`ClearType`/field shares).
- Existing `mpc_comm` helper methods are centered around SPDZ preprocessing.
  The semi-honest code should avoid calling malicious-only helpers where
  possible.
- The implementation must keep the offline/online separation explicit. Chase
  preparation belongs in offline; online should only consume prepared
  correlations and communicate masked values.
