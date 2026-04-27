import pbk


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
        return res
    except Exception as _:
        return "0"


def block_parse(data: bytes):
    try:
        block = pbk.Block(data)
        res = str(block.block_hash)
        for tx in block.transactions:
            res += "txid=" + str(tx.txid) + ";"
        return res
    except Exception as _:
        return "0"
