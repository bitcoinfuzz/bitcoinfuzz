import pbk


def block_parse(data: bytes):
    try:
        block = pbk.Block(data)
        res = str(block.block_hash)
        for tx in block.transactions:
            res += "txid=" + str(tx.txid) + ";"
        return res
    except Exception as _:
        return "0"
