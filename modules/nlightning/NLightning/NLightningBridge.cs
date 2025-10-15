using System.Runtime.InteropServices;

namespace NLightning.CppBridge;

using System.Text;
using NLightning.Bolt11.Models;

public static class Bridge
{
    [UnmanagedCallersOnly(EntryPoint = "nlightning_deserialize_invoice")]
    public static IntPtr DecodeInvoice(IntPtr invoiceStringPtr)
    {
        try
        {
            string? invoiceString = Marshal.PtrToStringUTF8(invoiceStringPtr);
            if (string.IsNullOrEmpty(invoiceString))
            {
                return CreateEmptyStringPtr();
            }

            Invoice invoice = Invoice.Decode(invoiceString);

            StringBuilder resultBuilder = new();

            resultBuilder.Append("HASH=").Append(invoice.PaymentHash);
            resultBuilder.Append(";AMOUNT=").Append(invoice.Amount.MilliSatoshi);
            resultBuilder.Append(";DESCRIPTION=").Append(invoice.Description);
            resultBuilder.Append(";RECIPIENT=").Append(invoice.PayeePubKey);
            resultBuilder.Append(";EXPIRY=").Append((int)(invoice.ExpiryDate - DateTimeOffset.FromUnixTimeSeconds(invoice.Timestamp)).TotalSeconds);
            resultBuilder.Append(";TIMESTAMP=").Append(invoice.Timestamp);
            resultBuilder.Append(";ROUTING_HINTS=").Append(invoice.RoutingInfos?.Count ?? 0);
            resultBuilder.Append(";MIN_CLTV=").Append(invoice.MinFinalCltvExpiry);

            string result = resultBuilder.ToString();

            // Manually allocate unmanaged memory for the UTF-8 string
            byte[] resultBytes = Encoding.UTF8.GetBytes(result);
            IntPtr resultPtr = Marshal.AllocHGlobal(resultBytes.Length + 1); // +1 for null terminator

            // Copy the UTF-8 bytes to unmanaged memory
            Marshal.Copy(resultBytes, 0, resultPtr, resultBytes.Length);

            // Null-terminate the string
            Marshal.WriteByte(resultPtr + resultBytes.Length, 0);

            return resultPtr;
        }
        catch
        {
            return CreateEmptyStringPtr();
        }
    }

    [UnmanagedCallersOnly(EntryPoint = "nlightning_free_string")]
    public static void FreeString(IntPtr ptr)
    {
        if (ptr != IntPtr.Zero)
        {
            Marshal.FreeHGlobal(ptr);
        }
    }

    private static IntPtr CreateEmptyStringPtr()
    {
        // Allocate memory for just a null terminator (empty string)
        IntPtr emptyStringPtr = Marshal.AllocHGlobal(1);
        Marshal.WriteByte(emptyStringPtr, 0);
        return emptyStringPtr;
    }
}