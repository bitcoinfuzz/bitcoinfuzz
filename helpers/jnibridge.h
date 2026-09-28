#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>

namespace bitcoinfuzz {
// Calls a static method of a JVM wrapper class that returns
// bitcoinfuzz.BfResult (include/bitcoinfuzz/BfResult.java), on the JVM
// JvmLoader starts. A missing class or method, an exception escaping the
// wrapper, or a null result aborts: it is a wrapper bug, not a fuzzing result.

// `static BfResult method(byte[])`.
std::optional<std::string> CallJvm(const char *class_name, const char *method,
                                   std::span<const uint8_t> input);

// `static BfResult method(String)`. The input goes through NewStringUTF, so it
// stops at the first NUL.
std::optional<std::string> CallJvm(const char *class_name, const char *method,
                                   const std::string &input);
} // namespace bitcoinfuzz
