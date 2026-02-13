using System.Runtime.InteropServices;
using System.Text;
using NBitcoin.Scripting;
using NBitcoin.Secp256k1;
using NBitcoin.WalletPolicies;


namespace NBitcoin.CppBridge;

public static class Bridge
{
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

            var tx = psbt.GetGlobalTransaction();
            if (tx == null)
            {
                return Marshal.StringToHGlobalAnsi("");
            }

            var result = new StringBuilder();
            result.Append($"lt={tx.LockTime.Value};");
            result.Append($"in={tx.Inputs.Count};");
            result.Append($"out={tx.Outputs.Count};");

            for (int i = 0; i < tx.Inputs.Count; i++)
            {
                var txIn = tx.Inputs[i];

                result.Append($"in{i}prev={txIn.PrevOut.Hash}:{txIn.PrevOut.N};");
                result.Append($"in{i}seq={txIn.Sequence.Value};");

                if (i < psbt.Inputs.Count)
                {
                    var psbtInput = psbt.Inputs[i];
                    bool hasUtxo = psbtInput.WitnessUtxo != null || psbtInput.NonWitnessUtxo != null;

                    if (hasUtxo)
                    {
                        result.Append($"in{i}utxo=1;");
                    }

                    int sigCount = psbtInput.PartialSigs?.Count ?? 0;
                    result.Append($"in{i}sigs={sigCount};");
                }
            }

            for (int i = 0; i < tx.Outputs.Count; i++)
            {
                var txOut = tx.Outputs[i];

                long value = txOut.Value.Satoshi;
                result.Append($"out{i}val={value};");

                string scriptHex = txOut.ScriptPubKey.ToHex();
                result.Append($"out{i}script={scriptHex};");
            }

            return Marshal.StringToHGlobalAnsi(result.ToString());
        }
        catch
        {
            return Marshal.StringToHGlobalAnsi("");
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

    [UnmanagedCallersOnly(EntryPoint = "nbitcoin_descriptor_parse")]
    public static bool DescriptorParse(IntPtr descriptorStringPtr)
    {
        if (descriptorStringPtr == IntPtr.Zero)
            return false;

        string descriptorString = Marshal.PtrToStringUTF8(descriptorStringPtr) ?? "";
        if (string.IsNullOrEmpty(descriptorString))
            return false;

        try
        {
            _ = OutputDescriptor.Parse(descriptorString, Network.Main);
            return true;
        }
        catch
        {
            return false;
        }
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
        var seed = new byte[(int)len];
        Marshal.Copy(dataPtr, seed, 0, (int)len);
        ExtKey sk = ExtKey.CreateFromSeed(seed);
        IntPtr strPtr = Marshal.StringToHGlobalAnsi(sk.GetWif(Network.Main).ToString());
        return strPtr;
    }

    [UnmanagedCallersOnly(EntryPoint = "nbitcoin_bip32_deserialize_extended_key")]
    public static IntPtr BIP32DeserializeExtendedKeyTarget(IntPtr inputPtr, int len)
    {
        if (inputPtr == IntPtr.Zero || len <= 0) return Marshal.StringToCoTaskMemUTF8("INVALID");

        string input = Marshal.PtrToStringUTF8(inputPtr, len) ?? "";

        if (TryParseXprv(input, Network.Main, out string result) ||
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

    private static bool TryParseXprv(string input, Network network, out string result)
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

    private static bool TryParseXpub(string input, Network network, out string result)
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

    [UnmanagedCallersOnly(EntryPoint = "nbitcoin_free_c_string")]
    public static void FreeString(IntPtr ptr)
    {
        if (ptr != IntPtr.Zero) Marshal.FreeHGlobal(ptr);
    }
}
