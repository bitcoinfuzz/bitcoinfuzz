using System.Runtime.InteropServices;
using System.Text;
using NBitcoin.Secp256k1;
using NBitcoin.WalletPolicies;


namespace NBitcoin.CppBridge;

public static class Bridge
{
    // Minimal, self-contained CompactSize (Bitcoin's little-endian varint)
    // decoder operating directly on the raw PSBT bytes. Used only to
    // disambiguate PSBT_IN_SEQUENCE presence (see V2InputHasExplicitSequence
    // below); everything else about the parse is handled by NBitcoin itself.
    private static bool TryReadCompactSize(byte[] data, int offset, out ulong value, out int bytesRead)
    {
        value = 0;
        bytesRead = 0;
        if (offset >= data.Length)
        {
            return false;
        }

        byte first = data[offset];
        if (first < 253)
        {
            value = first;
            bytesRead = 1;
            return true;
        }
        if (first == 253)
        {
            if (offset + 3 > data.Length)
            {
                return false;
            }
            ushort v = (ushort)(data[offset + 1] | (data[offset + 2] << 8));
            if (v < 253)
            {
                return false; // non-canonical
            }
            value = v;
            bytesRead = 3;
            return true;
        }
        if (first == 254)
        {
            if (offset + 5 > data.Length)
            {
                return false;
            }
            uint v = (uint)(data[offset + 1] | (data[offset + 2] << 8) |
                            (data[offset + 3] << 16) | (data[offset + 4] << 24));
            if (v < 0x10000u)
            {
                return false; // non-canonical
            }
            value = v;
            bytesRead = 5;
            return true;
        }

        if (offset + 9 > data.Length)
        {
            return false;
        }
        ulong v64 = 0;
        for (int i = 0; i < 8; i++)
        {
            v64 |= (ulong)data[offset + 1 + i] << (8 * i);
        }
        if (v64 < 0x100000000UL)
        {
            return false; // non-canonical
        }
        value = v64;
        bytesRead = 9;
        return true;
    }

    // Advances `offset` past one PSBT key-value map (a run of records
    // terminated by a zero-length key). Returns false if the buffer is
    // malformed/truncated before reaching the terminator.
    private static bool TrySkipPsbtMap(byte[] data, ref int offset)
    {
        while (true)
        {
            if (offset > data.Length || !TryReadCompactSize(data, offset, out ulong keylen, out int keylenSize))
            {
                return false;
            }
            offset += keylenSize;
            if (keylen == 0)
            {
                return true; // separator
            }
            if ((ulong)(data.Length - offset) < keylen)
            {
                return false;
            }
            offset += (int)keylen;

            if (!TryReadCompactSize(data, offset, out ulong vallen, out int vallenSize))
            {
                return false;
            }
            offset += vallenSize;
            if ((ulong)(data.Length - offset) < vallen)
            {
                return false;
            }
            offset += (int)vallen;
        }
    }

    // Scans the map starting at `offset` (advancing it past the map) for a
    // record whose key is the single byte `keyType` with no key data.
    private static bool PsbtMapHasSingleByteKey(byte[] data, ref int offset, byte keyType)
    {
        bool found = false;
        while (true)
        {
            if (offset > data.Length || !TryReadCompactSize(data, offset, out ulong keylen, out int keylenSize))
            {
                return false;
            }
            offset += keylenSize;
            if (keylen == 0)
            {
                return found; // separator
            }
            if ((ulong)(data.Length - offset) < keylen)
            {
                return false;
            }
            if (keylen == 1 && data[offset] == keyType)
            {
                found = true;
            }
            offset += (int)keylen;

            if (!TryReadCompactSize(data, offset, out ulong vallen, out int vallenSize))
            {
                return false;
            }
            offset += vallenSize;
            if ((ulong)(data.Length - offset) < vallen)
            {
                return false;
            }
            offset += (int)vallen;
        }
    }

    private const byte PsbtInSequenceKey = 0x10;

    // NBitcoin's Sequence is a plain (non-nullable) value, so an omitted
    // PSBT_IN_SEQUENCE and an explicit one equal to Sequence.Final both
    // read back as the same concrete value. Bitcoin Core/rust-psbt/
    // libwallycore format an *omitted* sequence as an empty string, so
    // match that here only for the ambiguous case by re-scanning the raw
    // input map for an explicit PSBT_IN_SEQUENCE record.
    private static bool V2InputHasExplicitSequence(byte[] psbtBytes, int targetInputIndex)
    {
        byte[] magic = { 0x70, 0x73, 0x62, 0x74, 0xff };
        if (psbtBytes.Length < magic.Length)
        {
            return false;
        }
        for (int i = 0; i < magic.Length; i++)
        {
            if (psbtBytes[i] != magic[i])
            {
                return false;
            }
        }

        int offset = magic.Length;
        if (!TrySkipPsbtMap(psbtBytes, ref offset))
        {
            return false; // global map
        }
        for (int i = 0; i < targetInputIndex; i++)
        {
            if (!TrySkipPsbtMap(psbtBytes, ref offset))
            {
                return false;
            }
        }
        return PsbtMapHasSingleByteKey(psbtBytes, ref offset, PsbtInSequenceKey);
    }

    [UnmanagedCallersOnly(EntryPoint = "nbitcoin_psbt_parse")]
    public static IntPtr PsbtParse(IntPtr dataPtr, UIntPtr len)
    {
        if (dataPtr == IntPtr.Zero || (int)len <= 0)
        {
            return IntPtr.Zero;
        }

        try
        {
            byte[] psbtBytes = new byte[(int)len];
            Marshal.Copy(dataPtr, psbtBytes, 0, (int)len);

            PSBT psbt = PSBT.Load(psbtBytes, Network.Main);
            bool isV2 = psbt.Version == PSBTVersion.PSBTv2;

            Transaction tx;
            try
            {
                tx = psbt.GetGlobalTransaction();
            }
            catch (InvalidOperationException ex) when (ex.Message.Contains("conflicting", StringComparison.OrdinalIgnoreCase))
            {
                // Conflicting per-input lock time requirements (BIP-370) is a
                // well-defined "reject" outcome, not a generic parse failure.
                // Use a non-empty sentinel so it's actually compared across
                // modules (the driver's PSBTParseTarget skips empty results
                // from comparison entirely) rather than silently opted out,
                // mirroring the other PSBTv2-aware modules.
                return Marshal.StringToHGlobalAnsi("CONFLICTING_LOCKTIME");
            }
            if (tx == null)
            {
                return Marshal.StringToHGlobalAnsi("");
            }

            var result = new StringBuilder();
            result.Append($"lock_time={tx.LockTime.Value};");
            result.Append($"inputs={tx.Inputs.Count};");
            result.Append($"outputs={tx.Outputs.Count};");

            for (int i = 0; i < tx.Inputs.Count; i++)
            {
                var txIn = tx.Inputs[i];

                result.Append($"input{i}previous_output={txIn.PrevOut.Hash}:{txIn.PrevOut.N};");

                uint seqValue = txIn.Sequence.Value;
                if (!isV2 || seqValue != Sequence.Final.Value || V2InputHasExplicitSequence(psbtBytes, i))
                {
                    result.Append($"input{i}sequence={seqValue};");
                }
                else
                {
                    result.Append($"input{i}sequence=;");
                }

                if (i < psbt.Inputs.Count)
                {
                    var psbtInput = psbt.Inputs[i];
                    bool hasUtxo = psbtInput.WitnessUtxo != null || psbtInput.NonWitnessUtxo != null;

                    if (hasUtxo)
                    {
                        result.Append($"input{i}utxo=1;");
                    }

                    int partialSignatureCount = psbtInput.PartialSigs?.Count ?? 0;
                    result.Append($"input{i}partial_signatures={partialSignatureCount};");

                    result.Append($"input{i}redeem_script={psbtInput.RedeemScript?.ToHex() ?? ""};");
                    result.Append($"input{i}witness_script={psbtInput.WitnessScript?.ToHex() ?? ""};");

                    // Raw PSBT_IN_SIGHASH_TYPE byte value, or 0 if unset.
                    // SighashType/TaprootSighashType are mutually exclusive
                    // (legacy vs taproot input); both enums are backed by
                    // the same raw byte values as the wire format.
                    uint sighashType = 0;
                    if (psbtInput.SighashType.HasValue)
                    {
                        sighashType = (uint)psbtInput.SighashType.Value;
                    }
                    else if (psbtInput.TaprootSighashType.HasValue)
                    {
                        sighashType = (uint)psbtInput.TaprootSighashType.Value;
                    }
                    result.Append($"input{i}sighash_type={sighashType};");

                    result.Append($"input{i}bip32={psbtInput.HDKeyPaths.Count};");

                    // Report finalization on a *non-empty* final
                    // scriptSig/scriptWitness rather than mere presence, which
                    // is what PSBTInput.IsFinalized() checks. Bitcoin Core
                    // stores its final witness as a plain CScriptWitness whose
                    // IsNull() is just stack.empty(), so it cannot distinguish
                    // an absent PSBT_IN_FINAL_SCRIPTWITNESS key from one
                    // present with a zero-item stack; presence-based semantics
                    // would make this flag incomparable across modules for that
                    // degenerate input. An empty witness finalizes nothing.
                    bool finalized =
                        (psbtInput.FinalScriptSig != null && psbtInput.FinalScriptSig.Length > 0)
                        || !WitScript.IsNullOrEmpty(psbtInput.FinalScriptWitness);
                    if (finalized)
                    {
                        result.Append($"input{i}finalized=1;");
                    }
                }
            }

            for (int i = 0; i < tx.Outputs.Count; i++)
            {
                var txOut = tx.Outputs[i];

                long value = txOut.Value.Satoshi;
                result.Append($"output{i}val={value};");

                string scriptHex = txOut.ScriptPubKey.ToHex();
                result.Append($"output{i}script={scriptHex};");

                if (i < psbt.Outputs.Count)
                {
                    var psbtOutput = psbt.Outputs[i];
                    result.Append($"output{i}redeem_script={psbtOutput.RedeemScript?.ToHex() ?? ""};");
                    result.Append($"output{i}witness_script={psbtOutput.WitnessScript?.ToHex() ?? ""};");
                    result.Append($"output{i}bip32={psbtOutput.HDKeyPaths.Count};");
                }
            }

            return Marshal.StringToHGlobalAnsi(result.ToString());
        }
        catch
        {
            return Marshal.StringToHGlobalAnsi("INVALID");
        }
    }

    [UnmanagedCallersOnly(EntryPoint = "nbitcoin_verify_script")]
    public static bool VerifyScript(IntPtr scriptSigPtr, int scriptSigLength, IntPtr scriptPubKeyPtr, int scriptPubKeyLength)
    {
        if (scriptSigLength <= 0 || scriptPubKeyLength <= 0)
            return false;

        try
        {
            byte[] scriptSigBytes = new byte[scriptSigLength];
            Marshal.Copy(scriptSigPtr, scriptSigBytes, 0, scriptSigLength);

            byte[] scriptPubKeyBytes = new byte[scriptPubKeyLength];
            Marshal.Copy(scriptPubKeyPtr, scriptPubKeyBytes, 0, scriptPubKeyLength);

            Script scriptSig = new Script(scriptSigBytes);
            Script scriptPubKey = new Script(scriptPubKeyBytes);

            var tx = Network.Main.CreateTransaction();
            tx.Version = 1;

            var txIn = new TxIn
            {
                ScriptSig = Script.Empty,
                Sequence = Sequence.Final
            };

            tx.Inputs.Add(txIn);
            tx.Outputs.Add(new TxOut(Money.Zero, scriptPubKey));

            var context = new ScriptEvaluationContext
            {
                ScriptVerify = ScriptVerify.None
            };

            return context.VerifyScript(scriptSig, scriptPubKey, new TransactionChecker(tx, 0));
        }
        catch
        {
            return false;
        }
    }

    [UnmanagedCallersOnly(EntryPoint = "nbitcoin_script_eval")]
    public static bool ScriptEval(IntPtr inputDataPtr, int inputDataLength, uint flags, uint version)
    {
        if (inputDataPtr == IntPtr.Zero || inputDataLength <= 0)
            return false;

        try
        {
            // Marshal the input data from unmanaged memory
            byte[] scriptBytes = new byte[inputDataLength];
            Marshal.Copy(inputDataPtr, scriptBytes, 0, inputDataLength);

            // Create script from bytes
            Script script = new Script(scriptBytes);

            // Determine the script verification flags
            ScriptVerify scriptFlags = (ScriptVerify)flags;

            // Determine witness version
            var sigVersion = version == 0 ? HashVersion.Original : HashVersion.WitnessV0;

            // Evaluate the script
            var context = new ScriptEvaluationContext
            {
                ScriptVerify = scriptFlags
            };

            return context.EvalScript(script, new TransactionChecker(Network.Main.CreateTransaction(), 0), sigVersion);
        }
        catch
        {
            return false;
        }
    }

    [UnmanagedCallersOnly(EntryPoint = "nbitcoin_miniscript_parse")]
    public static bool MiniscriptParse(IntPtr miniscriptStringPtr)
    {
        if (miniscriptStringPtr == IntPtr.Zero)
            return false;

        string miniscriptString = Marshal.PtrToStringUTF8(miniscriptStringPtr) ?? "";
        if (string.IsNullOrEmpty(miniscriptString))
            return false;

        return TryParseMiniscript(miniscriptString, KeyType.Classic)
            || TryParseMiniscript(miniscriptString, KeyType.Taproot);
    }

    private static bool TryParseMiniscript(string miniscript, KeyType keyType)
    {
        try
        {
            _ = Miniscript.Parse(miniscript, new MiniscriptParsingSettings(Network.Main)
            {
                Dialect = MiniscriptDialect.Strict,
                KeyType = keyType,
                AllowedParameters = ParameterTypeFlags.None
            });
            return true;
        }
        catch
        {
            return false;
        }
    }

    [UnmanagedCallersOnly(EntryPoint = "nbitcoin_bip32_master_keygen")]
    public static IntPtr BIP32MasterKeygen(IntPtr dataPtr, UIntPtr len)
    {
        ulong seedLength = len.ToUInt64();
        if (seedLength < 16 || seedLength > 64)
        {
            return IntPtr.Zero;
        }

        var seed = new byte[(int)seedLength];
        Marshal.Copy(dataPtr, seed, 0, seed.Length);
        ExtKey sk = ExtKey.CreateFromSeed(seed);
        IntPtr strPtr = Marshal.StringToHGlobalAnsi(sk.GetWif(Network.Main).ToString());
        return strPtr;
    }

    [UnmanagedCallersOnly(EntryPoint = "nbitcoin_bip32_deserialize_extended_key")]
    public static IntPtr BIP32DeserializeExtendedKeyTarget(IntPtr inputPtr, int len)
    {
        if (inputPtr == IntPtr.Zero || len <= 0) return Marshal.StringToCoTaskMemUTF8("INVALID");

        string input = Marshal.PtrToStringUTF8(inputPtr, len) ?? "";

        if (TryParseXprv(input, Network.Main, out string? result) ||
            TryParseXprv(input, Network.TestNet, out result) ||
            TryParseXpub(input, Network.Main, out result) ||
            TryParseXpub(input, Network.TestNet, out result))
        {
            return Marshal.StringToCoTaskMemUTF8(result);
        }

        return Marshal.StringToCoTaskMemUTF8("INVALID");
    }

    // Helpers
    private static string Hex(byte[] data) => Convert.ToHexString(data).ToLower();

    private static bool TryParseXprv(string input, Network network, out string? result)
    {
        result = null;
        try
        {
            var ext = NBitcoin.ExtKey.Parse(input, network);
            result = $"depth={ext.Depth:x2};fp={Hex(ext.ParentFingerprint.ToBytes())};child={ext.Child:x8};chaincode={Hex(ext.ChainCode)};key={ext.PrivateKey.ToHex().ToLower()}";
            return true;
        }
        catch
        {
            return false;
        }
    }

    private static bool TryParseXpub(string input, Network network, out string? result)
    {
        result = null;
        try
        {
            var ext = NBitcoin.ExtPubKey.Parse(input, network);
            result = $"depth={ext.Depth:x2};fp={Hex(ext.ParentFingerprint.ToBytes())};child={ext.Child:x8};chaincode={Hex(ext.ChainCode)};key={ext.PubKey.ToHex().ToLower()}";
            return true;
        }
        catch
        {
            return false;
        }
    }

    [UnmanagedCallersOnly(EntryPoint = "nbitcoin_sign_schnorr")]
    public static IntPtr SignSchnorr(IntPtr privkeyPtr, IntPtr hashPtr, IntPtr auxPtr)
    {
        try
        {
            byte[] privkeyBytes = new byte[32];
            Marshal.Copy(privkeyPtr, privkeyBytes, 0, 32);

            byte[] hashBytes = new byte[32];
            Marshal.Copy(hashPtr, hashBytes, 0, 32);

            byte[] auxBytes = new byte[32];
            Marshal.Copy(auxPtr, auxBytes, 0, 32);

            // Validate private key before creating Key object
            // Return null pointer for invalid keys to match BTCD behavior
            Key key;
            try
            {
                key = new Key(privkeyBytes);
            }
            catch
            {
                return IntPtr.Zero;
            }

            var hash256 = new uint256(hashBytes);
            var aux256 = new uint256(auxBytes);

            TaprootSignature sig = key.SignTaprootScriptSpend(hash256, merkleRoot: null, aux: aux256, TaprootSigHash.Default);

            byte[] sigBytes = sig.SchnorrSignature.ToBytes();
            string hexSignature = Hex(sigBytes);

            return Marshal.StringToHGlobalAnsi(hexSignature);
        }
        catch
        {
            return Marshal.StringToCoTaskMemUTF8("");
        }
    }

    [UnmanagedCallersOnly(EntryPoint = "nbitcoin_bip32_derive_from_path")]
    public static IntPtr BIP32DeriveFromPath(IntPtr dataPtr, UIntPtr len)
    {
        var data = new byte[(int)len];
        Marshal.Copy(dataPtr, data, 0, (int)len);

        string pathStr;
        try
        {
            pathStr = Encoding.UTF8.GetString(data);
        }
        catch
        {
            return Marshal.StringToCoTaskMemUTF8("INVALID");
        }

        //filtering to overcome path parsing inconsistencies between modules
        if (!IsValidPathString(pathStr))
            return IntPtr.Zero;

        if (!IsValidIndexes(pathStr))
            return IntPtr.Zero;

        KeyPath path;
        try
        {
            path = KeyPath.Parse(pathStr);
        }
        catch
        {
            return Marshal.StringToCoTaskMemUTF8("INVALID");
        }

        if (path.Indexes.Length == 0)
            return Marshal.StringToCoTaskMemUTF8("INVALID");

        byte[] seed = new byte[32]
        {
            0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
            0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10,
            0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18,
            0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20
        };

        try
        {
            var masterKey = ExtKey.CreateFromSeed(seed);
            var derivedKey = masterKey.Derive(path);
            return Marshal.StringToCoTaskMemUTF8(derivedKey.ToString(Network.Main));
        }
        catch
        {
            return Marshal.StringToCoTaskMemUTF8("INVALID");
        }
    }

    private static bool IsValidPathString(string s)
    {
        if (string.IsNullOrEmpty(s))
            return false;

        if (s[0] == '/' || s[^1] == '/' || s.Contains("//"))
            return false;

        for (int i = 0; i < s.Length; i++)
        {
            char c = s[i];

            if (c == '\0' ||
                c == '+' ||
                c == '-' ||
                char.IsWhiteSpace(c))
            {
                return false;
            }

            if (c == 'm' && i != 0)
                return false;
        }

        return true;
    }

    private static bool IsValidIndexes(string s)
    {
        int start = 0;

        while (start < s.Length)
        {
            int end = start;
            while (end < s.Length && s[end] != '/')
                end++;

            var part = s.Substring(start, end - start);

            if (part.Length > 0)
            {
                if (ulong.TryParse(part, out var val))
                {
                    if (val > 0x7FFFFFFF)
                        return false;
                }
            }

            if (end == s.Length)
                break;

            start = end + 1;
        }

        return true;
    }

    [UnmanagedCallersOnly(EntryPoint = "nbitcoin_free_c_string")]
    public static void FreeString(IntPtr ptr)
    {
        if (ptr != IntPtr.Zero) Marshal.FreeHGlobal(ptr);
    }
}
