# Module Matrix

Ground-truth audit of all modules in the bitcoinfuzz harness.
Derived from `modules/*/module.h`, `auto_build.py`, the root `Makefile`,
and `docker-compose.yml` as of the `v2` branch.

## How to read this table

- **Flag** — the `-D<FLAG>` value passed in `CXXFLAGS` to enable the module
- **Directory** — path relative to repo root (follows `get_module_dir()` rules in `auto_build.py`)
- **Language** — runtime language of the wrapped library
- **FFI** — mechanism used to call into that runtime from C++
- **Implemented targets** — `BaseModule` virtual methods overridden by the module
- **docker-compose services** — services in `docker-compose.yml` whose `CXXFLAGS` include this module's flag

---

## Module Table

| Flag | Directory | Language | FFI | Implemented targets | docker-compose services |
|------|-----------|----------|-----|---------------------|-------------------------|
| `BITCOIN_CORE` | `modules/bitcoin/` | C++ | Direct (git submodule) | `script_parse`, `deserialize_block`, `descriptor_parse`, `miniscript_parse`, `address_parse`, `addrv2_parse`, `psbt_parse`, `cmpctblocks_parse`, `transaction_eval`, `bip32_master_keygen`, `bip32_deserialize_extended_key`, `bip32_derive_from_path` | `script`, `deserialize_block`, `script_eval`, `verify_script`, `descriptor_parse`, `cmpctblocks_parse`, `miniscript_parse`, `address_parse`, `addrv2`, `transaction_eval`, `psbt_parse`, `bip32_master_keygen`, `bip32_deserialize_extended_key`, `bip32_derive_from_path` |
| `RUST_BITCOIN` | `modules/rustbitcoin/` | Rust | cbindgen C FFI | `script_parse`, `deserialize_block`, `address_parse`, `psbt_parse`, `addrv2_parse`, `cmpctblocks_parse`, `parse_p2p_message`, `bip32_master_keygen`, `bip32_deserialize_extended_key`, `bip32_derive_from_path` | `script`, `deserialize_block`, `cmpctblocks_parse`, `address_parse`, `addrv2`, `parse_p2p_message`, `psbt_parse`, `bip32_master_keygen`, `bip32_deserialize_extended_key`, `bip32_derive_from_path` |
| `RUST_PSBT` | `modules/rustpsbt/` | Rust | cbindgen C FFI | `psbt_parse` | `psbt_parse` |
| `RUST_MINISCRIPT` | `modules/rustminiscript/` | Rust | cbindgen C FFI | `descriptor_parse`, `miniscript_parse` | `descriptor_parse`, `miniscript_parse` |
| `TINY_MINISCRIPT` | `modules/tinyminiscript/` | Rust | Shared lib (`.so`/`.dylib`) | `descriptor_parse` | — |
| `LDK` | `modules/ldk/` | Rust | cbindgen C FFI | `deserialize_invoice`, `deserialize_offer`, `parse_p2p_lightning_message`, `decode_onion` | `deserialize_invoice`, `deserialize_offer`, `parse_p2p_lightning_message`, `decode_onion` |
| `RUSTBITCOINKERNEL` | `modules/rustbitcoinkernel/` | Rust | cbindgen C FFI | `kernel_block`, `kernel_transaction` | `kernel_block`, `kernel_transaction` |
| `RUST_K256` | `modules/rustk256/` | Rust | cbindgen C FFI | `private_to_public_key`, `sign_compact`, `sign_der`, `sign_verify`, `ecdh`, `sign_schnorr` | `private_to_public_key`, `sign_compact`, `sign_der`, `sign_verify`, `ecdh`, `sign_schnorr` |
| `RUSTREEXO` | `modules/rustreexo/` | Rust | cbindgen C FFI | `stump_modify_add` | `stump_modify_add` |
| `BTCD` | `modules/btcd/` | Go | CGo | `deserialize_block`, `address_parse`, `psbt_parse`, `addrv2_parse`, `parse_p2p_message`, `transaction_eval`, `bip32_master_keygen`, `decode_ellswift` | `deserialize_block`, `verify_script`, `addrv2`, `parse_p2p_message`, `transaction_eval`, `psbt_parse`, `bip32_master_keygen`, `decode_ellswift`, `sign_schnorr`, `schnorr_verify` |
| `GOCOIN` | `modules/gocoin/` | Go | CGo | `verify_script`, `script_eval` | `script_eval`, `verify_script` |
| `LND` | `modules/lnd/` | Go | CGo | `deserialize_invoice`, `parse_p2p_lightning_message`, `decode_onion` | `deserialize_invoice`, `parse_p2p_lightning_message`, `decode_onion` |
| `DECRED_SECP256K1` | `modules/decredsecp256k1/` | Go | CGo | `private_to_public_key`, `sign_compact`, `sign_der`, `sign_verify`, `ecdh` | `private_to_public_key`, `sign_compact`, `sign_der`, `sign_verify`, `ecdh` |
| `UTREEXO` | `modules/utreexo/` | Go | CGo | `stump_modify_add` | `stump_modify_add` |
| `BITCOINJ` | `modules/bitcoinj/` | Java | JNI | `bip32_master_keygen`, `bip32_deserialize_extended_key` | `bip32_master_keygen`, `bip32_deserialize_extended_key` |
| `ECLAIR` | `modules/eclair/` | Scala (JVM) | JNI | `deserialize_invoice`, `deserialize_offer` | `deserialize_invoice`, `deserialize_offer` |
| `LIGHTNING_KMP` | `modules/lightningkmp/` | Kotlin (JVM) | JNI | `deserialize_invoice`, `deserialize_offer` | `deserialize_invoice` |
| `NBITCOIN` | `modules/nbitcoin/` | C# (.NET) | P/Invoke | `descriptor_parse`, `miniscript_parse`, `bip32_master_keygen`, `bip32_deserialize_extended_key`, `psbt_parse`, `sign_schnorr` | `script_eval`, `verify_script`, `descriptor_parse`, `miniscript_parse`, `psbt_parse`, `bip32_master_keygen`, `sign_schnorr`, `bip32_deserialize_extended_key` |
| `NLIGHTNING` | `modules/nlightning/` | C# (.NET) | P/Invoke | `deserialize_invoice` | `deserialize_invoice` |
| `NBITCOIN_SECP256K1` | `modules/nbitcoinsecp256k1/` | C# (.NET) | P/Invoke | `private_to_public_key`, `sign_compact`, `sign_der`, `sign_verify`, `ecdh`, `schnorr_verify` | `private_to_public_key`, `sign_compact`, `sign_der`, `sign_verify`, `ecdh`, `schnorr_verify` |
| `EMBIT` | `modules/embit/` | Python | Python C API | `descriptor_parse`, `miniscript_parse`, `psbt_parse`, `bip32_master_keygen`, `bip32_deserialize_extended_key` | `descriptor_parse`, `miniscript_parse`, `psbt_parse`, `bip32_master_keygen`, `bip32_deserialize_extended_key` |
| `PYBITCOINKERNEL` | `modules/pybitcoinkernel/` | Python | Python C API | `kernel_block`, `kernel_transaction` | `kernel_block`, `kernel_transaction` |
| `PYCOIN` | `modules/pycoin/` | Python | Python C API | `bip32_master_keygen`, `bip32_deserialize_extended_key` | `bip32_master_keygen`, `bip32_deserialize_extended_key` |
| `SECP256K1` | `modules/secp256k1/` | C | Direct (git submodule) | `private_to_public_key`, `sign_compact`, `sign_der`, `sign_verify`, `ecdh`, `sign_schnorr`, `decode_ellswift`, `schnorr_verify` | `private_to_public_key`, `sign_compact`, `sign_der`, `sign_verify`, `ecdh`, `sign_schnorr`, `decode_ellswift`, `schnorr_verify` |
| `LIBWALLY_CORE` | `modules/libwallycore/` | C | Direct (git submodule) | `psbt_parse`, `bip32_master_keygen`, `bip32_deserialize_extended_key` | `psbt_parse`, `bip32_master_keygen`, `bip32_deserialize_extended_key` |
| `CLIGHTNING` | `modules/clightning/` | C | Direct (git submodule) | `deserialize_invoice`, `deserialize_offer`, `parse_p2p_lightning_message`, `decode_onion` | `deserialize_invoice`, `deserialize_offer`, `parse_p2p_lightning_message`, `decode_onion` |
| `BITCOINKERNEL` | `modules/bitcoinkernel/` | C++ | C API wrapper | `kernel_block`, `kernel_transaction` | `kernel_block`, `kernel_transaction` |
| `BITCOINKERNEL_VARIANT` | `modules/bitcoinkernelvariant/` | C++ | C API wrapper | `kernel_block`, `kernel_transaction` | `kernel_block`, `kernel_transaction` |

---

## Module Count by Language

| Language | Count | Modules |
|----------|-------|---------|
| Rust | 9 | `rustbitcoin`, `rustpsbt`, `rustminiscript`, `tinyminiscript`, `ldk`, `rustbitcoinkernel`, `rustk256`, `rustreexo` + (kernel variant uses rust kernel) |
| Go | 4 | `btcd`, `gocoin`, `lnd`, `decredsecp256k1`, `utreexo` |
| C / C++ | 5 | `bitcoin`, `secp256k1`, `libwallycore`, `clightning`, `bitcoinkernel`, `bitcoinkernelvariant` |
| Java / JVM | 3 | `bitcoinj`, `eclair`, `lightningkmp` |
| C# (.NET) | 3 | `nbitcoin`, `nlightning`, `nbitcoinsecp256k1` |
| Python | 3 | `embit`, `pybitcoinkernel`, `pycoin` |

---

## Targets Coverage Matrix

Which modules implement each fuzz target (✓ = implements, — = not implemented):

| Target | BITCOIN_CORE | RUST_BITCOIN | BTCD | GOCOIN | NBITCOIN | EMBIT | RUST_MINISCRIPT | LDK | LND | CLIGHTNING | ECLAIR | LIGHTNING_KMP | NLIGHTNING | SECP256K1 | DECRED_SECP256K1 | NBITCOIN_SECP256K1 | RUST_K256 | BITCOINJ | BITCOINKERNEL | BITCOINKERNEL_VARIANT | PYBITCOINKERNEL | RUSTBITCOINKERNEL | RUST_PSBT | LIBWALLY_CORE | PYCOIN | RUSTREEXO | UTREEXO | TINY_MINISCRIPT |
|--------|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|
| `script_parse` | ✓ | ✓ | — | — | — | — | — | — | — | — | — | — | — | — | — | — | — | — | — | — | — | — | — | — | — | — | — | — |
| `deserialize_block` | ✓ | ✓ | ✓ | — | — | — | — | — | — | — | — | — | — | — | — | — | — | — | — | — | — | — | — | — | — | — | — | — |
| `script_eval` | — | — | — | ✓ | — | — | — | — | — | — | — | — | — | — | — | — | — | — | — | — | — | — | — | — | — | — | — | — |
| `verify_script` | — | — | ✓ | ✓ | — | — | — | — | — | — | — | — | — | — | — | — | — | — | — | — | — | — | — | — | — | — | — | — |
| `descriptor_parse` | ✓ | — | — | — | ✓ | ✓ | ✓ | — | — | — | — | — | — | — | — | — | — | — | — | — | — | — | — | — | — | — | — | ✓ |
| `miniscript_parse` | ✓ | — | — | — | ✓ | ✓ | ✓ | — | — | — | — | — | — | — | — | — | — | — | — | — | — | — | — | — | — | — | — | — |
| `address_parse` | ✓ | ✓ | ✓ | — | — | — | — | — | — | — | — | — | — | — | — | — | — | — | — | — | — | — | — | — | — | — | — | — |
| `addrv2_parse` | ✓ | ✓ | ✓ | — | — | — | — | — | — | — | — | — | — | — | — | — | — | — | — | — | — | — | — | — | — | — | — | — |
| `psbt_parse` | ✓ | ✓ | ✓ | — | ✓ | ✓ | — | — | — | — | — | — | — | — | — | — | — | — | — | — | — | — | ✓ | ✓ | — | — | — | — |
| `cmpctblocks_parse` | ✓ | ✓ | — | — | — | — | — | — | — | — | — | — | — | — | — | — | — | — | — | — | — | — | — | — | — | — | — | — |
| `transaction_eval` | ✓ | — | ✓ | — | — | — | — | — | — | — | — | — | — | — | — | — | — | — | — | — | — | — | — | — | — | — | — | — |
| `deserialize_invoice` | — | — | — | — | — | — | — | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | — | — | — | — | — | — | — | — | — | — | — | — | — | — | — |
| `deserialize_offer` | — | — | — | — | — | — | — | ✓ | — | ✓ | ✓ | ✓ | — | — | — | — | — | — | — | — | — | — | — | — | — | — | — | — |
| `parse_p2p_message` | — | ✓ | ✓ | — | — | — | — | — | — | — | — | — | — | — | — | — | — | — | — | — | — | — | — | — | — | — | — | — |
| `parse_p2p_lightning_message` | — | — | — | — | — | — | — | ✓ | ✓ | ✓ | — | — | — | — | — | — | — | — | — | — | — | — | — | — | — | — | — | — |
| `bip32_master_keygen` | ✓ | ✓ | ✓ | — | ✓ | ✓ | — | — | — | — | — | — | — | — | — | — | — | ✓ | — | — | — | — | — | ✓ | ✓ | — | — | — |
| `bip32_deserialize_extended_key` | ✓ | ✓ | — | — | ✓ | ✓ | — | — | — | — | — | — | — | — | — | — | — | ✓ | — | — | — | — | — | ✓ | ✓ | — | — | — |
| `bip32_derive_from_path` | ✓ | ✓ | — | — | — | — | — | — | — | — | — | — | — | — | — | — | — | — | — | — | — | — | — | — | — | — | — | — |
| `kernel_block` | — | — | — | — | — | — | — | — | — | — | — | — | — | — | — | — | — | — | ✓ | ✓ | ✓ | ✓ | — | — | — | — | — | — |
| `kernel_transaction` | — | — | — | — | — | — | — | — | — | — | — | — | — | — | — | — | — | — | ✓ | ✓ | ✓ | ✓ | — | — | — | — | — | — |
| `private_to_public_key` | — | — | — | — | — | — | — | — | — | — | — | — | — | ✓ | ✓ | ✓ | ✓ | — | — | — | — | — | — | — | — | — | — | — |
| `sign_compact` | — | — | — | — | — | — | — | — | — | — | — | — | — | ✓ | ✓ | ✓ | ✓ | — | — | — | — | — | — | — | — | — | — | — |
| `sign_der` | — | — | — | — | — | — | — | — | — | — | — | — | — | ✓ | ✓ | ✓ | ✓ | — | — | — | — | — | — | — | — | — | — | — |
| `sign_verify` | — | — | — | — | — | — | — | — | — | — | — | — | — | ✓ | ✓ | ✓ | ✓ | — | — | — | — | — | — | — | — | — | — | — |
| `ecdh` | — | — | — | — | — | — | — | — | — | — | — | — | — | ✓ | ✓ | ✓ | ✓ | — | — | — | — | — | — | — | — | — | — | — |
| `sign_schnorr` | — | — | ✓ | — | ✓ | — | — | — | — | — | — | — | — | ✓ | — | — | ✓ | — | — | — | — | — | — | — | — | — | — | — |
| `schnorr_verify` | — | — | ✓ | — | — | — | — | — | — | — | — | — | — | ✓ | — | ✓ | — | — | — | — | — | — | — | — | — | — | — | — |
| `decode_ellswift` | — | — | ✓ | — | — | — | — | — | — | — | — | — | — | ✓ | — | — | — | — | — | — | — | — | — | — | — | — | — | — |
| `decode_onion` | — | — | — | — | — | — | — | ✓ | ✓ | ✓ | — | — | — | — | — | — | — | — | — | — | — | — | — | — | — | — | — | — |
| `stump_modify_add` | — | — | — | — | — | — | — | — | — | — | — | — | — | — | — | — | — | — | — | — | — | — | — | — | — | ✓ | ✓ | — |

---

## Notes

- `TINY_MINISCRIPT` appears in no docker-compose service directly because it is a
  dependency pulled in via the `tinyminiscript` shared library at runtime, not
  via a dedicated compose flag.
- `LIGHTNING_KMP` implements `deserialize_offer` in its header but the only
  compose service that includes it (`deserialize_invoice`) does not exercise
  that path — a gap worth filling in a future PR.
- Modules that require Rust nightly: `RUST_BITCOIN`, `RUST_PSBT`,
  `RUST_MINISCRIPT`, `LDK`, `TINY_MINISCRIPT`, `RUSTBITCOINKERNEL`,
  `RUST_K256`, `RUSTREEXO` — handled automatically by `auto_build.py`.
- Modules that require sequential builds (JVM init or heavy submodule):
  `SECP256K1`, `BITCOINJ`, `LIGHTNING_KMP` — handled by `should_build_sequentially()`.
- Git submodules are required for: `CLIGHTNING` (`external/lightning`),
  `SECP256K1` (`external/secp256k1`), `ECLAIR` (`external/eclair`),
  `LIBWALLY_CORE` (`external/libwally-core`), `BITCOIN_CORE`
  (`external/bitcoin-core`) — see `SUBMODULES_BY_FLAG` in `auto_build.py`.
