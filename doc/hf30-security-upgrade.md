# HF30 security upgrade draft

HF30 is scheduled for **Wednesday, 7 October 2026 12:00:00 UTC** (Unix time
`1791374400`). Nodes require the witness-voted time to match this compiled time
before applying HF30, so a vote cannot move it earlier. All block producers must
run this binary and vote (1.30, 1791374400) before that moment; the scaled
hardfork quorum (7 of 8, or the current equivalent) then activates it.

The witness schedule count repair applies as soon as nodes run this binary: it
counts enabled witnesses rather than every registered witness object. This is
necessary to keep a null-key registration from stopping schedule updates before
HF30 can activate. HF30 then requires each scheduled witness to receive at
least 1% of outstanding VESTS in witness approval. The floor only filters when
at least one enabled witness meets it; if none do (a bootstrapping or degenerate
chain with no real votes), scheduling falls back to pre-HF30 behavior and keeps
every enabled witness, so the schedule never collapses to a single producer.
Disabling the last enabled witness is rejected through both witness update APIs.

The hardfork-vote quorum repair also applies as soon as nodes run this binary
(not HF30-gated, mirroring HF29's own non-gated tally change). The quorum
denominator now counts only witnesses that have produced recently
(`last_confirmed_block_num` within two rounds), not every scheduled witness. A
never-producing witness keeps the default 0.0.0 hardfork vote that the stale-vote
filter ignores, so counting it would let an attacker register free, idle
witnesses (e.g. via the keyless `temp` account) to push the quorum above what the
honest producers can reach and permanently block HF30 - and every future
hardfork - by vote. The repair touches only `next_hardfork` / `next_hardfork_time`
(and, post-HF29, the API-only `majority_version`), none of which affect block
validity, so patched and unpatched nodes keep accepting each other's blocks until
the hardfork actually applies. Test: `non_producing_witnesses_do_not_raise_hardfork_quorum`.

HF30 also starts recording prior owner authorities immediately, even though the
inherited mainnet tracking block is 3,186,477. It cannot reconstruct owner
histories from changes before HF30; accounts compromised before activation may
still need a separate recovery procedure.

The one-percent approval floor makes the free zero-vote witness flood impossible.
One voter can still vote for multiple witnesses with the same stake. A large
stakeholder could therefore elect multiple controlled witnesses; HF30 does not
replace the chain's DPoS trust assumption or add a per-voter vote budget.

Verified locally in an offline Linux testnet build: `hf30_tests/*` (eleven cases,
1211 assertions), `hf30_activation_tests/*` (one case, 24 assertions), the existing
`hf29_tests/*` suite (six cases, 192 assertions) and `comments_in_external_storage/*`
(four cases, 197 assertions). A full chain_test regression shows no new failures
against the HF29 baseline. The activation test
uses an exact witness vote at a short testnet time and checks that a later owner
change creates a history entry. Mainnet replay and live deployment remain to be
verified after the activation time is chosen. No on-chain test or mainnet
transaction should be used for the null-key case.

## Economic corrections added to HF30

All three are gated on HF30 and change nothing before activation.

**Vote dust threshold.** Every vote loses a fixed number of rshares, and anything below it
becomes 0. The inherited 50,000,000 meant a full vote needed 2,500 VESTS, which on Pixagram's
1:1 VESTS price is 2,500 PIXA, so new accounts with a 100 VESTS delegation could not vote at all.
From HF30 the floor is `PIXA_HF30_VOTE_DUST_THRESHOLD` = 50,000 rshares: a full vote counts from
2.5 VESTS, about Hive's floor in PIXA terms. Resource credits still limit vote spam.

**Witness pay.** Per-block witness pay was weighted to spread 21 blocks' worth over the scheduled
witnesses, so with 9 witnesses each block paid 21/9 of the nominal 15% share and the chain issued
about 11.7% a year instead of 9.75%. From HF30, while fewer than 21 witnesses are scheduled, every
block pays exactly the nominal share; a full schedule keeps the upstream weighting. At today's
supply that is about 0.140 instead of 0.329 VESTS per block: each of 9 witnesses earns roughly
450 instead of 1,050 VESTS a day.

**DPF funding.** Each block's DPF share is converted to PXS and truncated to 0.001, which at a
51.833 PIXA feed paid 0.002 of the 0.0027 PXS due, about 11% of issuance instead of 15%. From
HF30 block n pays floor((n+1)x) - floor(nx) of the exact share x (`util::dhf_funding_without_truncation`),
which telescopes to the exact amount over consecutive blocks without storing a remainder.

Tests: `small_account_votes_count_from_hf30`, `partial_schedule_pays_the_nominal_witness_share`,
`dhf_funding_keeps_the_sub_milli_remainder`, `treasury_receives_the_full_dhf_share_after_hf30`.

**Transaction size** needs no consensus change: hived enforces `maximum_block_size - 256` bytes
(about 2 MiB at the current median), and `HIVE_MAX_TRANSACTION_SIZE` only sets the smallest block
size witnesses may vote. The public API's nginx accepts requests up to 1 MiB.


## Residual, not fixed in this release

**A single large stakeholder can still fill the schedule.** The one-percent
approval floor stops the zero-cost flood from anonymous accounts, but one account
may cast its full weight for up to 30 witnesses, so a holder with >=1% of VESTS
could clear the floor on several witnesses it controls. Live approvals on
2026-10-05: the lowest current witness sits at ~0.63% on a per-voter split basis
and ~5.4% on the raw tally, well clear of the floor. A per-voter vote budget
(dividing a voter's weight across the witnesses it approves) is the real fix, but
on today's votes it would drop three current witnesses (matus, chainguard,
initminer) below any floor that still blocks the attack, and matus is powering
down. It therefore waits until the witness set is more distributed, and is not
part of this activation. This is the standard DPoS trust assumption; the 1% floor
only removes the free variant.
