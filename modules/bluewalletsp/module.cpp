#include "module.h"
#include <bitcoinfuzz/result.h>
#include <cstdio>
#include <cstdlib>
#include <spawn.h>
#include <unistd.h>

extern char **environ;

namespace {

// The library needs a real Node runtime: its secp256k1 backend is a WebAssembly
// build and its dependencies use Node builtins. Rather than shim any of that,
// the module drives an unmodified Node process over a pipe. One runner is
// started lazily and kept for the whole session, so process startup and the
// WASM instantiation are paid once instead of per input.
FILE *runner_in = nullptr;
FILE *runner_out = nullptr;

[[noreturn]] void die(const std::string &message) {
  std::fprintf(stderr, "BlueWalletSp: %s\n", message.c_str());
  std::abort();
}

bool start_runner() {
  if (runner_in != nullptr && runner_out != nullptr)
    return true;

  int to_runner[2];
  int from_runner[2];
  if (pipe(to_runner) != 0 || pipe(from_runner) != 0)
    return false;

  posix_spawn_file_actions_t actions;
  posix_spawn_file_actions_init(&actions);
  posix_spawn_file_actions_adddup2(&actions, to_runner[0], STDIN_FILENO);
  posix_spawn_file_actions_adddup2(&actions, from_runner[1], STDOUT_FILENO);
  posix_spawn_file_actions_addclose(&actions, to_runner[1]);
  posix_spawn_file_actions_addclose(&actions, from_runner[0]);

  // The build bakes in the runner's absolute path. The environment overrides it
  // so an image can copy the runner somewhere other than where it was built.
  const char *env_path = std::getenv("BLUEWALLET_SP_RUNNER");
  std::string script{env_path != nullptr ? env_path : BLUEWALLET_SP_RUNNER};

  char program[] = "node";
  char *argv[] = {program, script.data(), nullptr};

  pid_t pid;
  const int rc = posix_spawnp(&pid, program, &actions, nullptr, argv, environ);
  posix_spawn_file_actions_destroy(&actions);
  close(to_runner[0]);
  close(from_runner[1]);
  if (rc != 0) {
    close(to_runner[1]);
    close(from_runner[0]);
    return false;
  }

  runner_in = fdopen(to_runner[1], "w");
  runner_out = fdopen(from_runner[0], "r");
  return runner_in != nullptr && runner_out != nullptr;
}

void append_hex(std::string &out, const uint8_t *data, size_t len) {
  static const char digits[] = "0123456789abcdef";
  for (size_t i = 0; i < len; ++i) {
    out += digits[data[i] >> 4];
    out += digits[data[i] & 0x0f];
  }
}

} // namespace

namespace bitcoinfuzz {
namespace module {
BlueWalletSp::BlueWalletSp(void) : BaseModule("BlueWalletSp") {}

std::optional<std::string> BlueWalletSp::silentpayments_create_outputs(
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

  // The library has no label API: it derives no label tweak and its address
  // type carries no label, so there is nothing here to compare against the
  // modules that do. Labeled recipients are dropped rather than answered with
  // the unlabeled outputs, which would be a mismatch of this wrapper's making.
  for (const uint8_t is_labeled : input.recipient_is_labeled) {
    if (is_labeled)
      return Skip();
  }

  if (!start_runner())
    die("could not start the node runner; is node on PATH?");

  std::string request;
  append_hex(request, input.outpoint_smallest.data(),
             input.outpoint_smallest.size());
  request += ' ';
  append_hex(request, input.input_seckeys.data(), input.input_seckeys.size());
  request += ' ';
  append_hex(request, input.input_is_taproot.data(),
             input.input_is_taproot.size());
  request += ' ';
  append_hex(request, input.scan_seckeys.data(), input.scan_seckeys.size());
  request += ' ';
  append_hex(request, input.spend_seckeys.data(), input.spend_seckeys.size());
  request += '\n';

  if (std::fwrite(request.data(), 1, request.size(), runner_in) !=
          request.size() ||
      std::fflush(runner_in) != 0) {
    die("the node runner closed its input");
  }

  std::string response;
  for (int c = std::fgetc(runner_out); c != '\n'; c = std::fgetc(runner_out)) {
    if (c == EOF)
      die("the node runner closed its output");
    response += static_cast<char>(c);
  }

  // The runner mirrors BfResult: "OK <outputs>", "FAIL <reason>" or "SKIP".
  // Anything else, such as its answer to a malformed request, is a bug here.
  if (response.starts_with("OK "))
    return Ok(response.substr(3));
  if (response.starts_with("FAIL "))
    return Fail(response.substr(5));
  if (response == "SKIP")
    return Skip();
  die("unexpected response from the node runner: " + response);
}
} // namespace module
} // namespace bitcoinfuzz
