// Java mirror of include/bitcoinfuzz/ffi.h, compiled into each JVM module and
// read back by helpers/jnibridge.cpp.
package bitcoinfuzz;

import java.nio.charset.StandardCharsets;

public final class BfResult {
  public static final byte OK = 0;
  public static final byte SKIP = 1;
  public static final byte FAIL = 2;

  public final byte status;
  public final byte[] value;

  private BfResult(byte status, String value) {
    this.status = status;
    this.value = value.getBytes(StandardCharsets.UTF_8);
  }

  public static BfResult ok(String value) {
    return new BfResult(OK, value);
  }

  public static BfResult skip() {
    return new BfResult(SKIP, "");
  }

  public static BfResult fail() {
    return new BfResult(FAIL, "");
  }

  // The driver compares the reason, so it must match what the target's other
  // modules return on rejection (e.g. "INVALID").
  public static BfResult fail(String reason) {
    return new BfResult(FAIL, reason);
  }
}
