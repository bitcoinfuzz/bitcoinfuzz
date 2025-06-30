using System.Runtime.InteropServices;
using NBitcoin.Scripting;
using NBitcoin.WalletPolicies;

namespace NBitcoin.CppBridge;

public static class Bridge
{
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
}