#include "driver.h"
#include <bitcoinfuzz/basemodule.h>
#include <bitcoinfuzz/module_registry.h>
#include <cstdlib>
#include <iostream>
#include <memory>

#ifdef BITCOIN_CORE
#include <modules/bitcoin/module.h>
#endif

#ifdef RUST_BITCOIN
#include <modules/rustbitcoin/module.h>
#endif

#ifdef RUST_MINISCRIPT
#include <modules/rustminiscript/module.h>
#endif

#ifdef TINY_MINISCRIPT
#include <modules/tinyminiscript/module.h>
#endif

#ifdef BTCD
#include <modules/btcd/module.h>
#endif

#ifdef NBITCOIN
#include <modules/nbitcoin/module.h>
#endif

#ifdef LND
#include <modules/lnd/module.h>
#endif

#ifdef LDK
#include <modules/ldk/module.h>
#endif

#ifdef ECLAIR
#include <modules/eclair/module.h>
#endif

#ifdef NLIGHTNING
#include <modules/nlightning/module.h>
#endif

#ifdef PYBITCOINKERNEL
#include <modules/pybitcoinkernel/module.h>
#endif

#ifdef RUSTBITCOINKERNEL
#include <modules/rustbitcoinkernel/module.h>
#endif

#ifdef EMBIT
#include <modules/embit/module.h>
#endif

#ifdef CLIGHTNING
#include <modules/clightning/module.h>
#endif

#ifdef LIGHTNING_KMP
#include <modules/lightningkmp/module.h>
#endif

#ifdef BITCOINJ
#include <modules/bitcoinj/module.h>
#endif

#ifdef DECRED_SECP256K1
#include <modules/decredsecp256k1/module.h>
#endif

#ifdef SECP256K1
#include <modules/secp256k1/module.h>
#endif

#ifdef NBITCOIN_SECP256K1
#include <modules/nbitcoinsecp256k1/module.h>
#endif

#ifdef CUSTOM_MUTATOR_BOLT12_OFFER
#include <custommutator/mutators/bolt12_offer.h>
#endif

#ifdef CUSTOM_MUTATOR_P2P_MESSAGE
#include <custommutator/mutators/p2p_message.h>
#endif

#ifdef CUSTOM_MUTATOR_P2P_LIGHTNING_MESSAGE
#include <custommutator/mutators/p2p_lightning_message.h>
#endif

#ifdef CUSTOM_MUTATOR_BOLT11
#include <custommutator/mutators/bolt11.h>
#endif

#ifdef CUSTOM_MUTATOR_SECP256K1_SIGNATURE
#include <custommutator/mutators/secp256k1_signature.h>
#endif

std::shared_ptr<bitcoinfuzz::Driver> driver = nullptr;

static bitcoinfuzz::ModuleLogger module_logger;

static void InitializeRegistryOnce() {
  std::string error = bitcoinfuzz::ModuleRegistry::instance().initialize();
  if (!error.empty()) {
    std::cerr << error << std::endl;
    std::abort();
  }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *Data, size_t Size) {
  InitializeRegistryOnce();

  const char *target = std::getenv("FUZZ");
  driver = std::make_shared<bitcoinfuzz::Driver>(module_logger);

  auto &registry = bitcoinfuzz::ModuleRegistry::instance();

#ifdef BITCOIN_CORE
  if (registry.isEnabled("BITCOIN_CORE"))
    driver->LoadModule(std::make_shared<bitcoinfuzz::module::Bitcoin>());
#endif
#ifdef RUST_BITCOIN
  if (registry.isEnabled("RUST_BITCOIN"))
    driver->LoadModule(std::make_shared<bitcoinfuzz::module::Rustbitcoin>());
#endif
#ifdef RUST_MINISCRIPT
  if (registry.isEnabled("RUST_MINISCRIPT"))
    driver->LoadModule(std::make_shared<bitcoinfuzz::module::Rustminiscript>());
#endif
#ifdef TINY_MINISCRIPT
  if (registry.isEnabled("TINY_MINISCRIPT"))
    driver->LoadModule(std::make_shared<bitcoinfuzz::module::Tinyminiscript>());
#endif
#ifdef BTCD
  if (registry.isEnabled("BTCD"))
    driver->LoadModule(std::make_shared<bitcoinfuzz::module::Btcd>());
#endif
#ifdef NBITCOIN
  if (registry.isEnabled("NBITCOIN"))
    driver->LoadModule(std::make_shared<bitcoinfuzz::module::NBitcoin>());
#endif
#ifdef LND
  if (registry.isEnabled("LND"))
    driver->LoadModule(std::make_shared<bitcoinfuzz::module::Lnd>());
#endif
#ifdef LDK
  if (registry.isEnabled("LDK"))
    driver->LoadModule(std::make_shared<bitcoinfuzz::module::Ldk>());
#endif
#ifdef NLIGHTNING
  if (registry.isEnabled("NLIGHTNING"))
    driver->LoadModule(std::make_shared<bitcoinfuzz::module::NLightning>());
#endif
#ifdef EMBIT
  if (registry.isEnabled("EMBIT"))
    driver->LoadModule(std::make_shared<bitcoinfuzz::module::Embit>());
#endif
#ifdef PYBITCOINKERNEL
  if (registry.isEnabled("PYBITCOINKERNEL"))
    driver->LoadModule(
        std::make_shared<bitcoinfuzz::module::Pybitcoinkernel>());
#endif
#ifdef RUSTBITCOINKERNEL
  if (registry.isEnabled("RUSTBITCOINKERNEL"))
    driver->LoadModule(
        std::make_shared<bitcoinfuzz::module::Rustbitcoinkernel>());
#endif
#ifdef CLIGHTNING
  if (registry.isEnabled("CLIGHTNING"))
    driver->LoadModule(std::make_shared<bitcoinfuzz::module::CLightning>());
#endif
#ifdef ECLAIR
  if (registry.isEnabled("ECLAIR"))
    driver->LoadModule(std::make_shared<bitcoinfuzz::module::Eclair>());
#endif
#ifdef LIGHTNING_KMP
  if (registry.isEnabled("LIGHTNING_KMP"))
    driver->LoadModule(std::make_shared<bitcoinfuzz::module::LightningKmp>());
#endif
#ifdef BITCOINJ
  if (registry.isEnabled("BITCOINJ"))
    driver->LoadModule(std::make_shared<bitcoinfuzz::module::BitcoinJ>());
#endif
#ifdef DECRED_SECP256K1
  if (registry.isEnabled("DECRED_SECP256K1"))
    driver->LoadModule(
        std::make_shared<bitcoinfuzz::module::Decred_secp256k1>());
#endif
#ifdef SECP256K1
  if (registry.isEnabled("SECP256K1"))
    driver->LoadModule(std::make_shared<bitcoinfuzz::module::Secp256k1>());
#endif
#ifdef NBITCOIN_SECP256K1
  if (registry.isEnabled("NBITCOIN_SECP256K1"))
    driver->LoadModule(
        std::make_shared<bitcoinfuzz::module::NBitcoin_secp256k1>());
#endif

#ifdef CUSTOM_MUTATOR_BOLT11
  module_logger.addCustomMutator("BOLT11 Bech32 Custom Mutator");
#endif

#ifdef CUSTOM_MUTATOR_SECP256K1_SIGNATURE
  module_logger.addCustomMutator("SECP256K1 Signature Custom Mutator");
#endif

#ifdef CUSTOM_MUTATOR_BOLT12_OFFER
  module_logger.addCustomMutator("BOLT12 Offer/Invoice Custom Mutator");
#endif

#ifdef CUSTOM_MUTATOR_P2P_MESSAGE
  module_logger.addCustomMutator("Bitcoin P2P Message Custom Mutator");
#endif

#ifdef CUSTOM_MUTATOR_P2P_LIGHTNING_MESSAGE
  module_logger.addCustomMutator("Lightning P2P Message Custom Mutator");
#endif

  module_logger.logModules();
  driver->Run(Data, Size, target);
  return 0;
}
