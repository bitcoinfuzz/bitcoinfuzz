#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>

namespace bitcoinfuzz {
// Calls function(bytes) in modules/<module>/<module>_lib.py, which returns a
// bfresult.BfResult (include/bitcoinfuzz/bfresult.py) or, for bool targets, a
// bool. The interpreter starts on first use. A missing module or function, an
// exception escaping the wrapper, or a result of the wrong type aborts: it is
// a wrapper bug, not a fuzzing result.
std::optional<std::string> CallPython(const char *module, const char *function,
                                      std::span<const uint8_t> input);

bool CallPythonBool(const char *module, const char *function,
                    std::span<const uint8_t> input);
} // namespace bitcoinfuzz
