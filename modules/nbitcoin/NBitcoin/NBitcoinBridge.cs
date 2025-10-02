using System.Runtime.InteropServices;
using NBitcoin.Scripting;
using NBitcoin.WalletPolicies;

namespace NBitcoin.CppBridge;

public static class Bridge
{
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

    [UnmanagedCallersOnly(EntryPoint = "nbitcoin_free_c_string")]
    public static void FreeString(IntPtr ptr)
    {
        if (ptr != IntPtr.Zero) Marshal.FreeHGlobal(ptr);
    }
}
