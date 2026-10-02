"""Electrum adapter for bitcoinfuzz differential fuzzing."""

from bfresult import fail, ok, skip
from electrum.bip32 import BIP32Node


def bip32_master_keygen(data):
    try:
        node = BIP32Node.from_rootseed(data, xtype="standard")
        return ok(node.to_xprv())
    except Exception:
        return fail("INVALID")


def bip32_deserialize_extended_key(data):
    try:
        text = data.decode()
    except Exception:
        return fail("INVALID")
    # electrum pins a specific network and by default it's mainnet
    if not text.startswith(("xprv", "xpub")):
        return skip()
    try:
        node = BIP32Node.from_xkey(text, allow_custom_headers=False)
    except Exception:
        return fail("INVALID")
    if node.is_private():
        key = node.eckey.get_secret_bytes()  # 32-byte secret, matching embit
    else:
        key = node.eckey.get_public_key_bytes(compressed=True)
    try:
        return ok(
            f"depth={node.depth:02x};"
            f"fp={node.fingerprint.hex()};"
            f"child={node.child_number.hex()};"
            f"chaincode={node.chaincode.hex()};"
            f"key={key.hex()}"
        )
    except Exception:
        return fail("INVALID")
