# bitcoin-rs

[bitcoin-rs](https://github.com/gosuda/bitcoin-rs) is an independent Bitcoin
full node written in Rust. This module links its `bitcoin-rs-primitives` crate
(consensus encoding/decoding) and does not link libbitcoinkernel, so it stays
independent of Bitcoin Core.

Enable it with `-DBITCOIN_RS` (registry name `BITCOIN_RS`).

Supported targets: `deserialize_block`.
