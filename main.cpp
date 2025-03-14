#include <memory>
#include "driver.h"
#include <bitcoinfuzz/basemodule.h>

#ifdef BITCOIN_CORE
#include <modules/bitcoin/module.h>
#endif

#ifdef MAKO
#include <modules/mako/module.h>
#endif

#ifdef BTCD
#include <modules/btcd/module.h>
#endif

#ifdef LND
#include <modules/lnd/module.h>
#endif

#ifdef NLIGHTNING
#include <modules/nlightning/module.h>
#endif

#ifdef RUST_MODULES
#include <modules/rust/module.h>
#endif

std::shared_ptr<bitcoinfuzz::Driver> driver = nullptr;

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *Data, size_t Size)
{
  const char* target = std::getenv("FUZZ");
  driver = std::make_shared<bitcoinfuzz::Driver>();
#ifdef BITCOIN_CORE
  driver->LoadModule(std::make_shared<bitcoinfuzz::module::Bitcoin>());
#endif
#ifdef MAKO
  driver->LoadModule(std::make_shared<bitcoinfuzz::module::Mako>());
#endif
#ifdef BTCD
  driver->LoadModule(std::make_shared<bitcoinfuzz::module::Btcd>());
#endif
#ifdef LND
  driver->LoadModule(std::make_shared<bitcoinfuzz::module::Lnd>());
#endif
#ifdef NLIGHTNING
  driver->LoadModule(std::make_shared<bitcoinfuzz::module::NLightning>());
#endif
#ifdef RUST_MODULES
  driver->LoadModule(std::make_shared<bitcoinfuzz::module::RustModules>());
#endif

  driver->Run(Data, Size, target);
  return 0;
}