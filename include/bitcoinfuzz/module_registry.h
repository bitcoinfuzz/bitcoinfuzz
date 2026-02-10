#pragma once

#include <cstdlib>
#include <set>
#include <string>
#include <vector>

namespace bitcoinfuzz {

// Singleton registry to track which modules are enabled at runtime
class ModuleRegistry {
public:
  static ModuleRegistry &instance() {
    static ModuleRegistry reg;
    return reg;
  }

  // Initialize from MODULES environment variable
  // Returns empty string on success, or error message on failure
  std::string initialize() {
    if (initialized_)
      return "";
    initialized_ = true;

    const char *modules_env = std::getenv("MODULES");
    std::vector<std::string> requested = parseModuleList(modules_env);

    if (requested.empty()) {
      all_enabled_ = true;
      return "";
    }

    all_enabled_ = false;
    enabled_ = std::set<std::string>(requested.begin(), requested.end());

    // Validate that all requested modules are compiled
    std::set<std::string> compiled = getCompiledModules();
    for (const auto &module : requested) {
      if (compiled.find(module) == compiled.end()) {
        std::string error = "ERROR: Module '" + module +
                            "' was requested but is not compiled.\n"
                            "Compiled modules: ";
        bool first = true;
        for (const auto &m : compiled) {
          if (!first)
            error += ", ";
          error += m;
          first = false;
        }
        if (compiled.empty()) {
          error += "(none)";
        }
        return error;
      }
    }

    return "";
  }

  // Check if a specific module is enabled at runtime
  bool isEnabled(const std::string &name) const {
    return all_enabled_ || enabled_.count(name) > 0;
  }

private:
  ModuleRegistry() = default;
  ModuleRegistry(const ModuleRegistry &) = delete;
  ModuleRegistry &operator=(const ModuleRegistry &) = delete;

  // Returns the list of all module names that were compiled into this binary
  static std::set<std::string> getCompiledModules() {
    std::set<std::string> compiled;

#ifdef BITCOIN_CORE
    compiled.insert("BITCOIN_CORE");
#endif
#ifdef RUST_BITCOIN
    compiled.insert("RUST_BITCOIN");
#endif
#ifdef RUST_MINISCRIPT
    compiled.insert("RUST_MINISCRIPT");
#endif
#ifdef TINY_MINISCRIPT
    compiled.insert("TINY_MINISCRIPT");
#endif
#ifdef BTCD
    compiled.insert("BTCD");
#endif
#ifdef GOCOIN
    compiled.insert("GOCOIN");
#endif
#ifdef NBITCOIN
    compiled.insert("NBITCOIN");
#endif
#ifdef LND
    compiled.insert("LND");
#endif
#ifdef LDK
    compiled.insert("LDK");
#endif
#ifdef NLIGHTNING
    compiled.insert("NLIGHTNING");
#endif
#ifdef EMBIT
    compiled.insert("EMBIT");
#endif
#ifdef PYBITCOINKERNEL
    compiled.insert("PYBITCOINKERNEL");
#endif
#ifdef RUSTBITCOINKERNEL
    compiled.insert("RUSTBITCOINKERNEL");
#endif
#ifdef CLIGHTNING
    compiled.insert("CLIGHTNING");
#endif
#ifdef ECLAIR
    compiled.insert("ECLAIR");
#endif
#ifdef LIGHTNING_KMP
    compiled.insert("LIGHTNING_KMP");
#endif
#ifdef BITCOINJ
    compiled.insert("BITCOINJ");
#endif
#ifdef DECRED_SECP256K1
    compiled.insert("DECRED_SECP256K1");
#endif
#ifdef SECP256K1
    compiled.insert("SECP256K1");
#endif
#ifdef NBITCOIN_SECP256K1
    compiled.insert("NBITCOIN_SECP256K1");
#endif
#ifdef RUST_K256
    compiled.insert("RUST_K256");
#endif

    return compiled;
  }

  // Parse comma-separated module names from environment variable
  static std::vector<std::string> parseModuleList(const char *modules_env) {
    std::vector<std::string> result;
    if (!modules_env || modules_env[0] == '\0') {
      return result;
    }

    std::string modules_str(modules_env);
    size_t start = 0;
    size_t end = 0;

    while ((end = modules_str.find(',', start)) != std::string::npos) {
      std::string module = modules_str.substr(start, end - start);
      // Trim whitespace
      size_t first = module.find_first_not_of(" \t");
      size_t last = module.find_last_not_of(" \t");
      if (first != std::string::npos) {
        result.push_back(module.substr(first, last - first + 1));
      }
      start = end + 1;
    }

    // Last token
    std::string module = modules_str.substr(start);
    size_t first = module.find_first_not_of(" \t");
    size_t last = module.find_last_not_of(" \t");
    if (first != std::string::npos) {
      result.push_back(module.substr(first, last - first + 1));
    }

    return result;
  }

  std::set<std::string> enabled_;
  bool all_enabled_ = true;
  bool initialized_ = false;
};

} // namespace bitcoinfuzz
