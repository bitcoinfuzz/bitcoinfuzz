#include <bitcoinfuzz/module_loader.h>
#include <cstdint>

static std::shared_ptr<bitcoinfuzz::Driver> driver = nullptr;
static bitcoinfuzz::ModuleLogger module_logger;
static std::string g_target;

// Go c-archives (-d=libfuzzer) initialize the Go runtime on a background
// thread, and that is where they register their coverage counters and PC
// tables with libFuzzer. That races with libFuzzer's own startup: a process
// that loses it sees missing or half-registered regions and dies with a SEGV in
// TracePC, "size of coverage PC tables does not match", or a Go fatal error.
// Fork children and merge subprocesses re-run the race on every start. Block
// here (libFuzzer calls LLVMFuzzerInitialize before it touches coverage) until
// every linked Go runtime is up. Needed even when the module is not in MODULES,
// since its runtime is linked in anyway. The names follow each module's
// SYMBOL_PREFIX from rename_cgo_symbols.sh; utreexo's archive is not renamed.
extern "C" {
#ifdef BTCD
uintptr_t btcd__cgo_wait_runtime_init_done(void);
#endif
#ifdef GOCOIN
uintptr_t gocoin__cgo_wait_runtime_init_done(void);
#endif
#ifdef LND
uintptr_t lnd__cgo_wait_runtime_init_done(void);
#endif
#ifdef DECRED_SECP256K1
uintptr_t decredsecp256k1__cgo_wait_runtime_init_done(void);
#endif
#ifdef UTREEXO
uintptr_t _cgo_wait_runtime_init_done(void);
#endif
}

static void WaitForGoRuntimes() {
#ifdef BTCD
  btcd__cgo_wait_runtime_init_done();
#endif
#ifdef GOCOIN
  gocoin__cgo_wait_runtime_init_done();
#endif
#ifdef LND
  lnd__cgo_wait_runtime_init_done();
#endif
#ifdef DECRED_SECP256K1
  decredsecp256k1__cgo_wait_runtime_init_done();
#endif
#ifdef UTREEXO
  _cgo_wait_runtime_init_done();
#endif
}

extern "C" int LLVMFuzzerInitialize([[maybe_unused]] int *argc,
                                    [[maybe_unused]] char ***argv) {
  WaitForGoRuntimes();
  bitcoinfuzz::InitializeRegistry();
  const char *target = std::getenv("FUZZ");
  if (target == nullptr || target[0] == '\0') {
    std::cerr << "FUZZ environment variable must be set" << std::endl;
    std::exit(1);
  }
  g_target = target;
  const char *log_outputs_env = std::getenv("LOG_OUTPUTS");
  const bool log_outputs = log_outputs_env != nullptr &&
                           log_outputs_env[0] != '\0' &&
                           std::string_view(log_outputs_env) != "0";
  driver = std::make_shared<bitcoinfuzz::Driver>(module_logger, log_outputs);
  bitcoinfuzz::LoadModules(driver, module_logger);
  return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *Data, size_t Size) {
  driver->Run(Data, Size, g_target);
  return 0;
}
