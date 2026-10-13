#include "module.h"
#include "sp_lib/sp_lib.h"

namespace bitcoinfuzz {
namespace module {

BdkSp::BdkSp(void) : BaseModule("BdkSp") {}

std::optional<std::string> BdkSp::silentpayments_create_outputs(
    const SilentPaymentsCreateOutputsInput &input) const {
  const size_t num_inputs = input.input_seckeys.size() / 32;
  const size_t num_recipients = input.scan_seckeys.size() / 32;
  if (num_inputs == 0 || num_recipients == 0 ||
      input.input_seckeys.size() != num_inputs * 32 ||
      input.input_is_taproot.size() != num_inputs ||
      input.scan_seckeys.size() != num_recipients * 32 ||
      input.spend_seckeys.size() != num_recipients * 32 ||
      input.recipient_is_labeled.size() != num_recipients ||
      input.recipient_labels.size() != num_recipients) {
    return Skip();
  }

  // Returns the recipient ordered outputs as hex, or fails with
  // "INVALID_SECKEY" / "CREATE_FAIL" / "LABEL_FAIL", matching the secp256k1
  // module. It skips on malformed arguments, which the checks above already
  // rule out, and on a known upstream divergence.
  return TakeResult(::bdk_sp_create_outputs(
      input.outpoint_smallest.data(), input.input_seckeys.data(),
      input.input_is_taproot.data(), num_inputs, input.scan_seckeys.data(),
      input.spend_seckeys.data(), input.recipient_is_labeled.data(),
      input.recipient_labels.data(), num_recipients));
}

} // namespace module
} // namespace bitcoinfuzz
