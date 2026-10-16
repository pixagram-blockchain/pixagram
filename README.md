# Pixagram

Pixagram is the blockchain behind [pixagram.com](https://pixagram.com), a fork of [Hive](https://hive.io).
This repository holds its node software, `hived`, and the `cli_wallet`.

- Mainnet RPC: https://api.pixagram.com
- Docs: https://pixa.org

## Differences from Hive

| | Hive | Pixagram |
|---|---|---|
| Liquid token | HIVE | **PIXA** |
| Stable token | HBD | **PXS** |
| Staked token | VESTS | VESTS |
| Public key prefix | `STM` | **`PIX`** |
| Inflation to content / proposals / witnesses / stakers | 65 / 10 / 10 / 15 % | **70 / 15 / 15 / 0 %** |
| PXS savings interest | set by witnesses | **fixed at 0** |
| Proposal fund account | `hive.fund` | **`pixa.omnibus`** |
| Max transaction size | 64 KiB | **128 KiB** |
| Max `custom_json` payload | 8 KiB | **64 KiB** |

Holding PIXA, PXS or VESTS earns nothing by itself; stake is rewarded only through curation, for voting.
Everything else works as on Hive: 3-second blocks, delegated proof of stake, Resource Credits instead
of fees, and the same APIs. The [Hive developer docs](https://developers.hive.io/) apply.

## Run a node

| You want | Use |
|---|---|
| A witness (block producer) | [pixagram-blockchain/witness](https://github.com/pixagram-blockchain/witness) |
| A public API node | [pixagram-blockchain/pixagram-node](https://github.com/pixagram-blockchain/pixagram-node) |
| A witness price feed | [pixagram-blockchain/bigmac-feed](https://github.com/pixagram-blockchain/bigmac-feed) |

Docker images are published as `pixadock/pixagram:<version>`, for example `pixadock/pixagram:1.30.1`.
The `mainnet` tag follows the current mainnet release.

To join the network with your own config, add a seed node to `config.ini`:

```ini
p2p-seed-node = api.pixagram.com:2001
```

## Build

```bash
git clone --recursive https://github.com/pixagram-blockchain/pixagram.git
cd pixagram
docker build --target=instance -t pixagram:local .
```

For native builds on Ubuntu 24.04 see [doc/building.md](doc/building.md).

## CLI wallet

The image includes `/home/hived/bin/cli_wallet`, which defaults to the Pixagram chain ID:

```bash
docker run --rm -it -v "$PWD/wallet:/wallet" -w /wallet \
  --entrypoint /home/hived/bin/cli_wallet pixadock/pixagram:1.30.1 -o
```

`-o` runs it offline, for signing. To broadcast, point it at a node with
`--server-rpc-endpoint=ws://<node>:<port>`.

## License

Based on Hive and Steem. Provided as is, without warranty. See [LICENSE.md](LICENSE.md).
