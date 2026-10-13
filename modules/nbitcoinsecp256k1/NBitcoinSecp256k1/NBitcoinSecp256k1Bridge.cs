using System.Runtime.InteropServices;
using BitcoinFuzz;
using NBitcoin.Secp256k1;
namespace NBitcoinSecp256k1.CppBridge;

public static unsafe class Bridge
{
    private const int BufferLength = 32;

    // Disposable struct that forces GC cleanup when disposed.
    private readonly struct GCScope : IDisposable
    {
        public void Dispose()
        {
            // We force a GC here to clean up managed wrappers holding native resources.
            // Normally the GC's finalizer thread takes care of this, but during fuzzing
            // the GC may not run often enough and the leaks start to accumulate.
            //
            // This does add GC pressure and will slow fuzzing down.
            GC.Collect();
            GC.WaitForPendingFinalizers();
        }
    }

    [UnmanagedCallersOnly(EntryPoint = "nbitcoinsecp256k1_private_to_public_key")]
    public static BfResult PrivateToPublicKey(IntPtr bufferPtr)
    {
        using var gcScope = new GCScope();
        try
        {
            Context ctx = Context.Instance;
            var privKeySpan = new ReadOnlySpan<byte>((void*)bufferPtr, BufferLength);
            if (!ctx.TryCreateECPrivKey(privKeySpan, out var privKey))
            {
                return BfResult.Skip();
            }

            var pubKey = privKey.CreatePubKey().ToBytes();

            return BfResult.Ok(HexEncode(pubKey));
        }
        catch
        {
            return BfResult.Fail();
        }
    }

    [UnmanagedCallersOnly(EntryPoint = "nbitcoinsecp256k1_sign_compact")]
    public static BfResult SignCompact(IntPtr bufferPtr, IntPtr hashPtr)
    {
        using var gcScope = new GCScope();
        try
        {
            Context ctx = Context.Instance;
            var privKeySpan = new ReadOnlySpan<byte>((void*)bufferPtr, BufferLength);
            var hashSpan = NormalizeHashToCurveOrder(new ReadOnlySpan<byte>((void*)hashPtr, BufferLength));
            if (!ctx.TryCreateECPrivKey(privKeySpan, out var privKey))
            {
                return BfResult.Skip();
            }

            var signature = privKey.SignECDSARFC6979(hashSpan);
            var compactSign = new byte[64];
            signature.WriteCompactToSpan(compactSign);

            return BfResult.Ok(HexEncode(compactSign));
        }
        catch
        {
            return BfResult.Fail();
        }
    }

    [UnmanagedCallersOnly(EntryPoint = "nbitcoinsecp256k1_sign_der")]
    public static BfResult SignDER(IntPtr bufferPtr, IntPtr hashPtr)
    {
        using var gcScope = new GCScope();
        try
        {
            Context ctx = Context.Instance;
            var privKeySpan = new ReadOnlySpan<byte>((void*)bufferPtr, BufferLength);
            var hashSpan = NormalizeHashToCurveOrder(new ReadOnlySpan<byte>((void*)hashPtr, BufferLength));
            if (!ctx.TryCreateECPrivKey(privKeySpan, out var privKey))
            {
                return BfResult.Skip();
            }

            var derSign = privKey.SignECDSARFC6979(hashSpan).ToDER();

            return BfResult.Ok(HexEncode(derSign));
        }
        catch
        {
            return BfResult.Fail();
        }
    }

    [UnmanagedCallersOnly(EntryPoint = "nbitcoinsecp256k1_sign_verify")]
    public static bool SignVerify(IntPtr bufferPtr, IntPtr hashPtr, IntPtr signPtr, uint signLen)
    {
        using var gcScope = new GCScope();
        try
        {
            Context ctx = Context.Instance;
            var privKeySpan = new ReadOnlySpan<byte>((void*)bufferPtr, BufferLength);
            var hashSpan = new ReadOnlySpan<byte>((void*)hashPtr, BufferLength);
            var signSpan = new ReadOnlySpan<byte>((void*)signPtr, (int)signLen);
            if (!ctx.TryCreateECPrivKey(privKeySpan, out var privKey))
            {
                return false;
            }

            if (!SecpECDSASignature.TryCreateFromDer(signSpan, out var signature))
            {
                return false;
            }

            var pubKey = privKey.CreatePubKey();

            return pubKey.SigVerify(signature, hashSpan);
        }
        catch
        {
            return false;
        }
    }

    [UnmanagedCallersOnly(EntryPoint = "nbitcoinsecp256k1_ecdh")]
    public static BfResult ECDH(IntPtr bufferPtr, IntPtr pubKeyPtr)
    {
        using var gcScope = new GCScope();
        try
        {
            Context ctx = Context.Instance;
            var privKeySpan = new ReadOnlySpan<byte>((void*)bufferPtr, BufferLength);
            var pubKeySpan = new ReadOnlySpan<byte>((void*)pubKeyPtr, 33);
            if (!ctx.TryCreateECPrivKey(privKeySpan, out var privKey))
            {
                return BfResult.Skip();
            }

            if (!ctx.TryCreatePubKey(pubKeySpan, out var pubKey))
            {
                return BfResult.Skip();
            }

            var sharedSecret = pubKey.GetSharedPubkey(privKey);
            var hashedSecret = System.Security.Cryptography.SHA256.HashData(sharedSecret.ToBytes());

            return BfResult.Ok(HexEncode(hashedSecret));
        }
        catch
        {
            return BfResult.Fail();
        }
    }

    [UnmanagedCallersOnly(EntryPoint = "nbitcoinsecp256k1_schnorr_verify")]
    public static unsafe BfResult SchnorrVerify(
        byte* privateKey32,
        byte* hash32,
        byte* sig64)
    {
        var privateKeyBytes = new ReadOnlySpan<byte>(privateKey32, 32);
        var hashBytes = new ReadOnlySpan<byte>(hash32, 32);
        var sigBytes = new ReadOnlySpan<byte>(sig64, 64);
        try
        {
            Context ctx = Context.Instance;
            if (!ctx.TryCreateECPrivKey(privateKeyBytes, out var privKey))
            {
                return BfResult.Ok("INVALID");
            }

            if (!SecpSchnorrSignature.TryCreate(sigBytes.ToArray(), out var sig))
            {
                return BfResult.Ok("INVALID");
            }

            ECXOnlyPubKey pubkey = privKey.CreateXOnlyPubKey();

            if (pubkey.SigVerifyBIP340(sig, hashBytes) == false)
            {
                return BfResult.Ok("INVALID");
            }

            return BfResult.Ok("VALID");
        }
        catch
        {
            return BfResult.Ok("INVALID");
        }
    }

    private static string HexEncode(byte[] data) => Convert.ToHexString(data).ToLowerInvariant();

    private static ReadOnlySpan<byte> NormalizeHashToCurveOrder(ReadOnlySpan<byte> hashSpan)
    {
        // Normalizes a hash to the secp256k1 curve order.
        // Ensures compatibility with libsecp256k1, which automatically performs
        // modular reduction on message hashes before deterministic nonce generation.
        return new Scalar(hashSpan).ToBytes();
    }
}