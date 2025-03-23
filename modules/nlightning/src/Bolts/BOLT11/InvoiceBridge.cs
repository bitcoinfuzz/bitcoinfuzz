using System;
using System.Runtime.InteropServices;

namespace NLightning.CppBridge.Bolts.BOLT11
{
    public static class InvoiceBridge
    {
        private const string LdkLib = "ldk_lib"; // Ensure this matches the Rust library file

        [DllImport(LdkLib, EntryPoint = "ldk_des_invoice", CallingConvention = CallingConvention.Cdecl)]
        private static extern bool LdkDecodeInvoice(IntPtr invoiceStringPtr);

        [UnmanagedCallersOnly(EntryPoint = "DecodeInvoice")]
        public static bool DecodeInvoice(IntPtr invoiceStringPtr)
        {
            if (invoiceStringPtr == IntPtr.Zero)
            {
                Console.WriteLine("Error: Received null pointer for invoice string.");
                return false;
            }

            bool result = LdkDecodeInvoice(invoiceStringPtr);
            Console.WriteLine($"DecodeInvoice Result: {result}");
            return result;
        }
    }
}