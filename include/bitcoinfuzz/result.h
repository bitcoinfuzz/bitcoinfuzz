// The outcomes a module reports to the driver. The driver compares Ok values
// and Fail reasons alike and leaves skipped modules out of the comparison; the
// names make each return site say which outcome it means.
#pragma once

#include <optional>
#include <string>

namespace bitcoinfuzz {
inline std::optional<std::string> Ok(std::string value) { return value; }

// The reason is what the driver compares, so it must match what the target's
// other modules return on rejection (e.g. "INVALID").
inline std::optional<std::string> Fail(std::string reason = "") {
  return reason;
}

// Converts to any std::optional, so bool targets can skip too.
constexpr std::nullopt_t Skip() { return std::nullopt; }
} // namespace bitcoinfuzz
