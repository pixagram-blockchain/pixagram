# Merging upstream Hive 1.28.8

Pixagram's `main` forked from upstream Hive at tag **1.28.7** (`1584099c3`). This merge brings in
upstream `master` at `7daad482d`, which is release **1.28.8** plus one `setup_ubuntu.sh` commit:
219 upstream commits in total. Upstream's 1.29.0 release candidates live on `develop`, which split
from the 1.28 line long before 1.28.7; they are not part of this merge.

The merge keeps Pixagram's history intact (a merge commit, not a rebase), because mainnet is live
and the `v1.29.0` and `v1.30.0` release tags point into `main`.

## Versions

| Build | `HIVE_BLOCKCHAIN_VERSION` | `HIVE_NUM_HARDFORKS` |
|---|---|---|
| mainnet | 1.30.1 | 30 |
| testnet, mirrornet | 1.31.0 | 31 |

Mainnet gets a patch version: this release changes no consensus rule that is active on mainnet.

## Upstream hardfork 29 is Pixagram hardfork 31

Upstream 1.28.8 ships its own hardfork 29. Pixagram already used 29 and 30 (active since blocks
402205 and 949330), so gating upstream's features on `HIVE_HARDFORK_1_29` would switch them on
retroactively from block 402205 and break replay. They are renumbered to hardfork 31 in
`libraries/protocol/hardfork.d/1_31.hf`:

| Feature macro | What it changes |
|---|---|
| `HIVE_HARDFORK_1_31_ALLOW_MODIFY_LAST_RECURRENT_TRANSFER` | a recurrent transfer can be modified down to one remaining execution |
| `HIVE_HARDFORK_1_31_FIX_ENFORCEMENT_OF_AUTHORITY_SIZE_ON_RECOVERY_REQUEST` | account recovery can no longer bypass the authority size limit |
| `HIVE_HARDFORK_1_31_FIX_LIMIT_ORDER_ASSETS` | limit orders can no longer name nonexistent assets |
| `HIVE_HARDFORK_1_31_RC_IS_CONSENSUS` | resource credits become part of consensus |
| `HIVE_HARDFORK_1_31_VALIDATE_ALL_PROPOSAL_IDS` | proposal removal validates every id |

HF31 uses upstream's far-future default time (2100-01-01). Mainnet keeps `HIVE_NUM_HARDFORKS` at
30, so none of these features can activate there, and a 1.30.1 witness does not vote for HF31.
Scheduling HF31 later means raising mainnet's hardfork count, setting a real time, and shipping
it as 1.31.0. The renamed upstream tests are in `tests/unit/tests/hf31_tests.cpp`.

## Pixagram code carried onto the new upstream APIs

Upstream encapsulated the chain objects and replaced raw assets with balance classes. Pixagram's
own consensus code was ported without changing its effect:

- **Genesis**: the same accounts in the same order (account ids are unchanged), the 3-of-3
  authorities, the ICO, team and treasury allocations, and the 1 PXS = 102 PIXA feed seed. The
  allocations are issued through the balance API with the same totals as before.
- **HF21 and HF25** keep Pixagram's reward settings: 40% curation, convergent curves,
  `content_constant` 2500, and zero vesting reward.
- **HF29** reset of `recent_claims`, through the new reward-fund accessors.
- **HF30 reward conversion** (`award_hbd`, formerly `create_hbd`): the unconverted remainder goes
  back to the recipient before upstream's conversion runs, which mints exactly the same PXS.
- **HF30 DPF funding**: a new `convert_HIVE_to_exact_HBD` on the global properties object mints the
  carried amount and leaves virtual supply alone, as the pre-merge code did.
- **Interest** stays pinned to zero, and the restricted VESTS transfer from `pixa.rex` and
  `pixa.team` moves stake through a temporary balance.

## Upstream regression fixed here

Upstream's `account_authority_object` refactor made `lock_account()` stamp
`previous_owner_update` and `last_owner_update`. The old code left them alone. On Pixagram that
changed the stored state of `pixa.omnibus`, locked by HF21 in block 1. No consensus check reads
those fields for a keyless account, but the state no longer matched the live chain.
`lock_account()` now uses `replace_owner_without_update()` and leaves the timestamps untouched.

## Behaviour that changes without a hardfork

- `author_reward_operation.curators_vesting_payout` now reports the curation actually paid
  (upstream fix `2a7a25d33`). A replay produces slightly different values for some historical
  operations, typically by one VESTS unit. HAF and Hivemind data indexed by 1.30.0 keeps the old
  values until it is rebuilt. State is not affected.
- `database_api.get_dynamic_global_properties` no longer returns `total_reward_fund_hive` and
  `total_reward_shares2`. Both have been zero since HF17.
- p2p handshakes require protocol version 106 or later and the current chain id. Nodes running
  1.30.0 report 107 and the Pixagram chain id, so they stay compatible.
- Test builds raise the minimum account creation fee from 0 to 0.001. Mainnet is unchanged at 1.

## Upgrading a node

The shared memory layout and the exported blockchain configuration both change, so moving a
node from 1.30.0 needs `--force-replay`, as for every previous Pixagram release. Builds now use
C++20 and Boost 1.90, and runtime images are based on Ubuntu 26.04 (`ci-base-image:pypa_2_28-pg18-5`).

The same release moves HAF to upstream 1.28.8 (pixagram-blockchain/haf, PostgreSQL stays at 17)
and Hivemind to upstream 1.28.8 (pixagram-blockchain/hivemind). The new Hivemind needs the new HAF,
so API nodes wipe `haf_db_store` and resync HAF and Hivemind together, as on every upgrade.

The 1.28.8 account-history fix matters on live nodes: 1.30.0's `account_history_rocksdb` keeps every
SST file open, and a node accumulated about 14,000 files a day (128,000 open descriptors after
nine days, against a limit of 1,048,576). After the replay the same node holds 25 files.

## Building without gitlab.syncad.com

Submodules come from the openhive-network mirrors on GitHub, so a recursive clone of
pixagram-blockchain/pixagram works as is. The Dockerfile's base images are unchanged copies of
upstream's on Docker Hub, under the same tags: `pixadock/ci-base-image:pypa_2_28-pg18-5` (build)
and `pixadock/minimal-runtime:ubuntu26.04-1` (runtime). When a future merge bumps either tag,
mirror the new image before building.

## Verification

A mainnet node built from this tree synced Pixagram mainnet over p2p from genesis to block
1,163,732, peering with 1.30.0 nodes throughout. It was then compared with api.pixagram.com at the
same head block.

- **State**: global properties, the reward fund, the witness schedule, hardfork properties, feed
  history, all witnesses, all proposals and all 117 accounts are identical. The only differences
  are the two global-properties fields upstream removed from the API, and account metadata, which
  comes from the separate `metadata` plugin that the test node did not load.
- **Virtual operations**: every operation in blocks 1 to 1,160,000 was compared, about 1.17 million
  in all. All match except 119 `author_reward_operation` values of `curators_vesting_payout`,
  which upstream fix `2a7a25d33` now reports as actually paid.
- **Unit tests**: the Pixagram HF29 and HF30 suites, both activation suites and `hf31_tests` pass
  (29 cases, 1,930 assertions). Run suite by suite against `main`, `chain_test` has no test that
  passes on `main` and fails here, and 17 that failed on `main` now pass. `plugin_test` fails 45
  cases against 51 on `main`, with none new. Both suites still have many failures inherited from
  `main`; see below.
- **Release candidate on a live API node**: 173.212.245.49 (blockforge.lol) has run this hived
  since 2026-10-15 07:14 UTC, and the new HAF and Hivemind since 12:16 UTC. The fresh HAF and
  Hivemind resync took about 30 minutes without a Hivemind restart. Compared with api.pixagram.com
  (1.30.0) at the same head block, the following were identical:
  - global properties, the reward fund, the feed, the witness schedule and all 18 witnesses,
  - all 130 accounts, with their RC delegations,
  - 45 sampled blocks with their operations,
  - every account's profile, posts and follow counts, plus ranked posts and communities.

  Account history differed only in 12 `curators_vesting_payout` values. Notifications differed in 7
  of the 130 accounts' lists, all produced by the resync itself: a fresh sync keeps only the final
  state of a changed vote, and creates no mention notification for a comment that was deleted.

## Unit-test builds

Upstream removed the `allow_not_enough_rc` switch that let test transactions through without
enough resource credits. Resource credit prices scale with total VESTS, and Pixagram's genesis
puts 100M VESTS into `pixa.rex` and `pixa.team`, so upstream's test accounts could no longer
afford anything. `IS_TEST_NET` builds therefore skip the genesis funding unless a test calls
`configuration_data.set_pixa_genesis_allocations( true )`, as the Pixagram genesis tests do.
Mainnet and mirrornet always fund.

The deeper cause of the unit-test failures, here and on `main` before this merge, is the test vest
price. Test builds start at upstream's `VEST_price( 1800, 1000 )`, about 0.0018 VESTS per PIXA,
and rely on HF1's 1,000,000x vesting share split to reach a realistic price. Pixagram's HF1 splits
by 1 (`perform_vesting_share_split( 1 )`), which is right for mainnet, whose genesis price is already
1 VESTS per PIXA. In test builds it leaves every account with a millionth of the stake upstream's
tests expect: votes fall below the dust threshold, and transactions run out of resource credits.
`hf28_tests2/vote_stabilization` then divides by a zero first vote and crashes, and in a single
`chain_test` process every later suite fails after it. Raising the test-build initial vesting price
by the split factor would fix this at the root, but it changes every test's economics, so it is
left as a follow-up.
