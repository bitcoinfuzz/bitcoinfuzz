from pycoin.symbols.btc import network as BTC
from pycoin.symbols.xtn import network as XTN


def bip32_master_keygen(data: bytes) -> str:
    try:
        root = BTC.keys.bip32_seed(data)
        return root.hwif(as_private=True)
    except Exception:
        return "INVALID"


def bip32_deserialize_extended_key(data: bytes) -> str:
    try:
        s = data.decode()
    except Exception:
        return "INVALID"

    key = None

    # TRY BTC PRV
    try:
        key = BTC.parse.bip32_prv(s)
    except Exception:
        key = None

    # TRY BTC PUB
    if key is None:
        try:
            key = BTC.parse.bip32_pub(s)
        except Exception:
            key = None

    # TRY TESTNET PRV (XTN)
    if key is None:
        try:
            key = XTN.parse.bip32_prv(s)
        except Exception:
            key = None

    # TRY TESTNET PUB (XTN)
    if key is None:
        try:
            key = XTN.parse.bip32_pub(s)
        except Exception:
            key = None

    if key is None:
        return "INVALID"

    try:
        depth = key.tree_depth()
        fp = key.parent_fingerprint()
        child_index = key.child_index()
        chaincode = key.chain_code()
        depth_hex = f"{depth:02x}"
        fp_hex = fp.hex().rjust(8, "0")
        child_hex = f"{child_index:08x}"
        chaincode_hex = chaincode.hex()

        if key.secret_exponent() is not None:
            se = key.secret_exponent()
            se_bytes = se.to_bytes(32, "big")
            key_bytes = se_bytes
        else:
            key_bytes = key.sec(is_compressed=True)

        key_hex = key_bytes.hex()
    except Exception:
        return "INVALID"

    return (
        f"depth={depth_hex};"
        f"fp={fp_hex};"
        f"child={child_hex};"
        f"chaincode={chaincode_hex};"
        f"key={key_hex}"
    )
