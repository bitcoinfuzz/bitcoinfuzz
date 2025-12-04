import pbk


def block_parse(data: bytes):
    try:
        block = pbk.Block(data)
        return str(block.block_hash)
    except Exception as _:
        return "0"
