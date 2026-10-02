using System.Runtime.InteropServices;

namespace NLightning.CppBridge;

using System.Text;
using BitcoinFuzz;
using NLightning.Bolt11.Models;

public static class Bridge
{
    [UnmanagedCallersOnly(EntryPoint = "nlightning_deserialize_invoice")]
    public static BfResult DecodeInvoice(IntPtr invoiceStringPtr)
    {
        try
        {
            string? invoiceString = Marshal.PtrToStringUTF8(invoiceStringPtr);
            if (string.IsNullOrEmpty(invoiceString))
            {
                return BfResult.Fail();
            }

            Invoice invoice = Invoice.Decode(invoiceString);

            StringBuilder resultBuilder = new();

            resultBuilder.Append("HASH=").Append(invoice.PaymentHash);
            resultBuilder.Append(";AMOUNT=").Append(invoice.Amount.MilliSatoshi);
            // Hex, the encoding every lightning module uses for free text.
            resultBuilder.Append(";DESCRIPTION=").Append(
                Convert.ToHexString(Encoding.UTF8.GetBytes(invoice.Description ?? string.Empty)).ToLowerInvariant());
            resultBuilder.Append(";RECIPIENT=").Append(invoice.PayeePubKey);
            resultBuilder.Append(";EXPIRY=").Append((int)(invoice.ExpiryDate - DateTimeOffset.FromUnixTimeSeconds(invoice.Timestamp)).TotalSeconds);
            resultBuilder.Append(";TIMESTAMP=").Append(invoice.Timestamp);
            resultBuilder.Append(";ROUTING_HINTS=").Append(invoice.RoutingInfos?.Count ?? 0);
            resultBuilder.Append(";MIN_CLTV=").Append(invoice.MinFinalCltvExpiry);

            return BfResult.Ok(resultBuilder.ToString());
        }
        catch
        {
            return BfResult.Fail();
        }
    }
}