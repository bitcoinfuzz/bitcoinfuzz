from embit.descriptor.miniscript import Miniscript
from embit.descriptor import Descriptor
from embit.psbt import PSBT


def miniscript_parse(input):
    try:
        ms = Miniscript.from_string(input, taproot=False)
        ms.verify()
        return True
    except Exception as _:
        try:
            ms = Miniscript.from_string(input, taproot=True)
            ms.verify()
            return True
        except Exception as _:
            return False


def descriptor_parse(input):
    try:
        desc = Descriptor.from_string(input)
        return True
    except Exception as _:
        return False


def psbt_parse(data):
    try:
        psbt_obj = PSBT.parse(data)

        result = []  # format similar to rustbitcoin implementation

        tx = psbt_obj.tx
        # result.append(f"v={tx.version}")
        result.append(f"lt={tx.locktime}")
        result.append(f"in={len(tx.vin)}")
        result.append(f"out={len(tx.vout)}")

        # ip details
        for i, vin in enumerate(tx.vin):
            result.append(f"in{i}prev={vin.txid.hex()}:{vin.vout}")
            result.append(f"in{i}seq={vin.sequence}")

            # check utxo
            psbt_input = psbt_obj.inputs[i]
            has_utxo = (
                1
                if (
                    hasattr(psbt_input, "witness_utxo")
                    and psbt_input.witness_utxo is not None
                    or hasattr(psbt_input, "non_witness_utxo")
                    and psbt_input.non_witness_utxo is not None
                )
                else 0
            )
            result.append(f"in{i}utxo={has_utxo}")

            # count sig
            sig_count = (
                len(psbt_input.partial_sigs)
                if hasattr(psbt_input, "partial_sigs")
                else 0
            )
            result.append(f"in{i}sigs={sig_count}")

        for i, vout in enumerate(tx.vout):
            result.append(f"out{i}val={vout.value}")
            result.append(f"out{i}script={vout.scriptpubkey.hex()}")

        return ";".join(result) + ";"
    except Exception as _:
        return None
