// C# mirror of include/bitcoinfuzz/ffi.h, linked into each NativeAOT bridge.

using System.Runtime.InteropServices;
using System.Text;

namespace BitcoinFuzz;

[StructLayout(LayoutKind.Sequential)]
public unsafe struct BfResult
{
    private const byte BfOk = 0;
    private const byte BfSkip = 1;
    private const byte BfFail = 2;

    private byte _status;
    private void* _data;
    private nuint _len;

    public static BfResult Ok(string value) => New(BfOk, value);

    public static BfResult Skip() => New(BfSkip, "");

    // The driver compares the reason, so it must match what the target's other
    // modules return on rejection (e.g. "INVALID").
    public static BfResult Fail(string reason = "") => New(BfFail, reason);

    private static BfResult New(byte status, string value)
    {
        byte[] bytes = Encoding.UTF8.GetBytes(value);
        void* data = null;
        if (bytes.Length > 0)
        {
            // NativeMemory.Alloc is malloc, which the harness pairs with free().
            data = NativeMemory.Alloc((nuint)bytes.Length);
            bytes.CopyTo(new Span<byte>(data, bytes.Length));
        }
        return new BfResult { _status = status, _data = data, _len = (nuint)bytes.Length };
    }
}
