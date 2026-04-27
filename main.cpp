#include <bitcoinfuzz/module_loader.h>

static std::shared_ptr<bitcoinfuzz::Driver> driver = nullptr;
static bitcoinfuzz::ModuleLogger module_logger;
static const char *g_target = nullptr;

extern "C" int LLVMFuzzerInitialize([[maybe_unused]] int *argc,
                                    [[maybe_unused]] char ***argv) {
  bitcoinfuzz::InitializeRegistry();
  g_target = std::getenv("FUZZ");
  driver = std::make_shared<bitcoinfuzz::Driver>(module_logger);
  bitcoinfuzz::LoadModules(driver, module_logger);
  return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *Data, size_t Size) {
  driver->Run(Data, Size, g_target);
  return 0;
}
