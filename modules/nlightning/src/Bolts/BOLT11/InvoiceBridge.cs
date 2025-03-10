using System.Runtime.InteropServices;

namespace NLightning.CppBridge.Bolts.BOLT11;

using NLightning.Bolts.BOLT11;
public static class InvoiceBridge
{
    [UnmanagedCallersOnly(EntryPoint = "DecodeInvoice")]
    public static bool DecodeInvoice(IntPtr invoiceStringPtr)
    {
        try
        {
            string? invoiceString = Marshal.PtrToStringUTF8(invoiceStringPtr);
            if (string.IsNullOrEmpty(invoiceString))
            {
                return false;
            }

            _ = Invoice.Decode(invoiceString);
            return true;
        }
        catch
        {
            return false;
        }
    }
}