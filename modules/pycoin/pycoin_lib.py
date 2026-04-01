from pycoin.symbols.btc import network as BTC


def bip32_master_keygen(data: bytes) -> str:
    try:
        root = BTC.keys.bip32_seed(data)
        return root.hwif(as_private=True)
    except Exception:
        return "INVALID"
