use bitcoinkernel::core::{ScriptPubkeyExt, TransactionExt, TxInExt, TxOutExt, TxOutPointExt};
use bitcoinkernel::prelude::BlockValidationStateExt;
use bitcoinkernel::{
    Block, BlockCheckFlags, BlockCheckResult, ChainParams, ChainType, Transaction,
    BLOCK_CHECK_BASE, BLOCK_CHECK_MERKLE, BLOCK_CHECK_POW,
};
use std::slice;

extern crate bitcoinkernel;

#[path = "../../../../include/bitcoinfuzz/ffi.rs"]
mod ffi;
use ffi::BfResult;

fn decode_chain_type(value: u8) -> ChainType {
    const CHAIN_TYPES: [ChainType; 5] = [
        ChainType::Mainnet,
        ChainType::Testnet,
        ChainType::Testnet4,
        ChainType::Signet,
        ChainType::Regtest,
    ];
    CHAIN_TYPES[(value as usize) % CHAIN_TYPES.len()]
}

fn chain_type_to_str(chain_type: ChainType) -> &'static str {
    match chain_type {
        ChainType::Mainnet => "mainnet",
        ChainType::Testnet => "testnet",
        ChainType::Testnet4 => "testnet4",
        ChainType::Signet => "signet",
        ChainType::Regtest => "regtest",
    }
}

fn decode_block_check_flags(value: u8) -> BlockCheckFlags {
    let mut flags = BLOCK_CHECK_BASE;
    if value & 0x01 != 0 {
        flags |= BLOCK_CHECK_POW;
    }
    if value & 0x02 != 0 {
        flags |= BLOCK_CHECK_MERKLE;
    }
    flags
}

#[no_mangle]
pub unsafe extern "C" fn rustbitcoinkernel_transaction(data: *const u8, len: usize) -> BfResult {
    // Safety: Ensure that the data pointer is valid for the given length
    let data_slice = slice::from_raw_parts(data, len);
    let Ok(tx) = Transaction::new(data_slice) else {
        return BfResult::fail_with("0");
    };

    let mut result = String::new();
    result.push_str(&format!("txid={};", tx.txid().to_string()));

    for input in tx.inputs() {
        result.push_str(&format!("index={}", input.outpoint().index().to_string()));
        result.push_str(&format!("txid={};", input.outpoint().txid().to_string()));
    }

    for output in tx.outputs() {
        result.push_str(&format!("amount={}", output.value().to_string()));
        let script_hex = output
            .script_pubkey()
            .to_bytes()
            .iter()
            .map(|b| format!("{:02x}", b))
            .collect::<String>();
        result.push_str(&format!("script_pubkey={};", script_hex));
    }

    BfResult::ok(result)
}

#[no_mangle]
pub unsafe extern "C" fn rustbitcoinkernel_block(data: *const u8, len: usize) -> BfResult {
    // Safety: Ensure that the data pointer is valid for the given length
    let data_slice = slice::from_raw_parts(data, len);
    let Ok(block) = Block::new(data_slice) else {
        return BfResult::fail_with("0");
    };

    let mut result = String::new();
    result.push_str(&block.hash().to_string());

    for tx in block.transactions() {
        result.push_str(&format!("txid={};", tx.txid().to_string()));
    }

    BfResult::ok(result)
}

#[no_mangle]
pub unsafe extern "C" fn rustbitcoinkernel_block_check(data: *const u8, len: usize) -> BfResult {
    // Safety: Ensure that the data pointer is valid for the given length
    let data_slice = slice::from_raw_parts(data, len);
    let chain_selector = data_slice.first().copied().unwrap_or(0);
    let flag_selector = data_slice.get(1).copied().unwrap_or(0);
    let chain_type = decode_chain_type(chain_selector);
    let flags = decode_block_check_flags(flag_selector);
    let raw_block = if data_slice.len() > 2 {
        &data_slice[2..]
    } else {
        &[]
    };

    let mut result = String::new();
    result.push_str("chain=");
    result.push_str(chain_type_to_str(chain_type));
    result.push_str(";flags=");
    result.push_str(&(flag_selector & 0x03).to_string());
    result.push(';');

    let Ok(block) = Block::new(raw_block) else {
        result.push_str("err=exception;");
        return BfResult::fail_with(result);
    };

    let chain_params = ChainParams::new(chain_type);
    match block.check(&chain_params, flags) {
        BlockCheckResult::Valid => {
            result.push_str("ok=1;mode=0;result=0;");
        }
        BlockCheckResult::Invalid(state) => {
            result.push_str("ok=0;mode=");
            result.push_str(&(state.mode() as u8).to_string());
            result.push_str(";result=");
            result.push_str(&(state.result() as u32).to_string());
            result.push(';');
        }
    }
    result.push_str("hash=");
    result.push_str(&block.hash().to_string());
    result.push_str(";txs=");
    result.push_str(&block.transaction_count().to_string());
    result.push(';');

    BfResult::ok(result)
}
