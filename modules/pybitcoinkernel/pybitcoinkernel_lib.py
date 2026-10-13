import pbk
from bfresult import fail, ok

_CHAIN_TYPES = [
    pbk.ChainType.MAINNET,
    pbk.ChainType.TESTNET,
    pbk.ChainType.TESTNET_4,
    pbk.ChainType.SIGNET,
    pbk.ChainType.REGTEST,
]

_CHAIN_TYPE_NAMES = {
    pbk.ChainType.MAINNET: "mainnet",
    pbk.ChainType.TESTNET: "testnet",
    pbk.ChainType.TESTNET_4: "testnet4",
    pbk.ChainType.SIGNET: "signet",
    pbk.ChainType.REGTEST: "regtest",
}


def transaction_parse(data: bytes):
    try:
        tx = pbk.Transaction(data)
        res = "txid=" + str(tx.txid) + ";"
        for txin in tx.inputs:
            res += f"index={str(txin.out_point.index)}"
            res += f"txid={str(txin.out_point.txid)};"
        for txout in tx.outputs:
            res += f"amount={str(txout.amount)}"
            res += f"script_pubkey={str(txout.script_pubkey)};"
        return ok(res)
    except Exception:
        return fail("0")


def block_parse(data: bytes):
    try:
        block = pbk.Block(data)
        res = str(block.block_hash)
        for tx in block.transactions:
            res += "txid=" + str(tx.txid) + ";"
        return ok(res)
    except Exception:
        return fail("0")


def block_check(data: bytes):
    chain_selector = data[0] if len(data) > 0 else 0
    flag_selector = data[1] if len(data) > 1 else 0
    raw_block = data[2:] if len(data) > 2 else b""

    chain_type = _CHAIN_TYPES[chain_selector % len(_CHAIN_TYPES)]

    flags = pbk.BlockCheckFlags.BASE
    if flag_selector & 0x01:
        flags |= pbk.BlockCheckFlags.POW
    if flag_selector & 0x02:
        flags |= pbk.BlockCheckFlags.MERKLE

    res = f"chain={_CHAIN_TYPE_NAMES[chain_type]};flags={flag_selector & 0x03};"

    try:
        block = pbk.Block(raw_block)
        consensus_params = pbk.ChainParameters(chain_type).consensus_params
        state = block.check(consensus_params, flags)
        valid = state.validation_mode == pbk.ValidationMode.VALID
        res += f"ok={1 if valid else 0}"
        res += f";mode={int(state.validation_mode)}"
        res += f";result={int(state.block_validation_result)}"
        res += f";hash={str(block.block_hash)}"
        res += f";txs={len(block.transactions)}"
        res += ";"
        return ok(res)
    except Exception:
        res += "err=exception;"
        return fail(res)
