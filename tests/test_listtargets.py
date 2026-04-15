import json
import subprocess
import sys
from pathlib import Path

from tools.listtargets import extract_targets


REPO_ROOT = Path(__file__).resolve().parents[1]
SCRIPT_PATH = REPO_ROOT / "tools" / "listtargets.py"
DRIVER_PATH = REPO_ROOT / "driver.cpp"
EXPECTED_TARGETS = [
    "script",
    "deserialize_block",
    "script_eval",
    "verify_script",
    "descriptor_parse",
    "miniscript_parse",
    "deserialize_invoice",
    "address_parse",
    "psbt_parse",
    "addrv2",
    "deserialize_offer",
    "cmpctblocks_parse",
    "parse_p2p_message",
    "parse_p2p_lightning_message",
    "transaction_eval",
    "bip32_master_keygen",
    "kernel_block",
    "kernel_transaction",
    "private_to_public_key",
    "sign_compact",
    "sign_der",
    "sign_verify",
    "ecdh",
    "sign_schnorr",
    "bip32_deserialize_extended_key",
    "decode_ellswift",
    "schnorr_verify",
    "decode_onion",
    "stump_modify_add",
]


def test_extract_targets_matches_driver_dispatch():
    assert extract_targets(DRIVER_PATH) == EXPECTED_TARGETS


def test_cli_json_output():
    result = subprocess.run(
        [sys.executable, str(SCRIPT_PATH), "--format=json"],
        check=True,
        capture_output=True,
        text=True,
        cwd=REPO_ROOT,
    )

    assert json.loads(result.stdout) == EXPECTED_TARGETS
