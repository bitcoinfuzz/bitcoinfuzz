#include "driver.h"
#include <algorithm>
#include <bitcoinfuzz/basemodule.h>
#include <cstring>
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

#ifdef GOCOIN
#include <modules/gocoin/module.h>
#endif

#ifdef LIBBITCOIN
#include <modules/libbitcoin/module.h>
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

#ifdef RUST_K256
#include <modules/rustk256/module.h>
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

#ifdef CUSTOM_MUTATOR_EXTENDED_KEY
#include <custommutator/mutators/extended_key.h>
#endif

std::shared_ptr<bitcoinfuzz::Driver> driver = nullptr;

static bitcoinfuzz::ModuleLogger module_logger;

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *Data, size_t Size) {
  const char *target = std::getenv("FUZZ");
  driver = std::make_shared<bitcoinfuzz::Driver>(module_logger);
#ifdef BITCOIN_CORE
  driver->LoadModule(std::make_shared<bitcoinfuzz::module::Bitcoin>());
#endif
#ifdef RUST_BITCOIN
  driver->LoadModule(std::make_shared<bitcoinfuzz::module::Rustbitcoin>());
#endif
#ifdef RUST_MINISCRIPT
  driver->LoadModule(std::make_shared<bitcoinfuzz::module::Rustminiscript>());
#endif
#ifdef TINY_MINISCRIPT
  driver->LoadModule(std::make_shared<bitcoinfuzz::module::Tinyminiscript>());
#endif
#ifdef LIBBITCOIN
  driver->LoadModule(std::make_shared<bitcoinfuzz::module::Libbitcoin>());
#endif
#ifdef BTCD
  driver->LoadModule(std::make_shared<bitcoinfuzz::module::Btcd>());
#endif
#ifdef GOCOIN
  driver->LoadModule(std::make_shared<bitcoinfuzz::module::Gocoin>());
#endif
#ifdef NBITCOIN
  driver->LoadModule(std::make_shared<bitcoinfuzz::module::NBitcoin>());
#endif
#ifdef LND
  driver->LoadModule(std::make_shared<bitcoinfuzz::module::Lnd>());
#endif
#ifdef LDK
  driver->LoadModule(std::make_shared<bitcoinfuzz::module::Ldk>());
#endif
#ifdef NLIGHTNING
  driver->LoadModule(std::make_shared<bitcoinfuzz::module::NLightning>());
#endif
#ifdef EMBIT
  driver->LoadModule(std::make_shared<bitcoinfuzz::module::Embit>());
#endif
#ifdef PYBITCOINKERNEL
  driver->LoadModule(std::make_shared<bitcoinfuzz::module::Pybitcoinkernel>());
#endif
#ifdef RUSTBITCOINKERNEL
  driver->LoadModule(
      std::make_shared<bitcoinfuzz::module::Rustbitcoinkernel>());
#endif
#ifdef CLIGHTNING
  driver->LoadModule(std::make_shared<bitcoinfuzz::module::CLightning>());
#endif
#ifdef ECLAIR
  driver->LoadModule(std::make_shared<bitcoinfuzz::module::Eclair>());
#endif
#ifdef LIGHTNING_KMP
  driver->LoadModule(std::make_shared<bitcoinfuzz::module::LightningKmp>());
#endif
#ifdef BITCOINJ
  driver->LoadModule(std::make_shared<bitcoinfuzz::module::BitcoinJ>());
#endif
#ifdef DECRED_SECP256K1
  driver->LoadModule(std::make_shared<bitcoinfuzz::module::Decred_secp256k1>());
#endif
#ifdef SECP256K1
  driver->LoadModule(std::make_shared<bitcoinfuzz::module::Secp256k1>());
#endif
#ifdef NBITCOIN_SECP256K1
  driver->LoadModule(
      std::make_shared<bitcoinfuzz::module::NBitcoin_secp256k1>());
#endif
#ifdef RUST_K256
  driver->LoadModule(std::make_shared<bitcoinfuzz::module::K256>());
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

#ifdef CUSTOM_MUTATOR_EXTENDED_KEY
  module_logger.addCustomMutator("Extended Key Base58 Custom Mutator");
#endif

  module_logger.logModules();
  driver->Run(Data, Size, target);
  return 0;
}
