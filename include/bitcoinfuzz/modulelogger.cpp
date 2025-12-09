#include "modulelogger.h"
#include <iostream>
#include <string>
#include <vector>

namespace bitcoinfuzz {
void ModuleLogger::addModule(const std::string &module_name) {
  if (logged)
    return;
  loaded_modules.push_back(module_name);
}

void ModuleLogger::addCustomMutator(const std::string &mutator_name) {
  if (logged)
    return;
  custom_mutators.push_back(mutator_name);
}

void ModuleLogger::logModules() {
  if (logged)
    return;
  logged = true;

  std::cout << "=== BitcoinFuzz Module Activation Log ===" << std::endl;
  std::cout << "Target: "
            << (std::getenv("FUZZ") ? std::getenv("FUZZ") : "default")
            << std::endl;

  if (!loaded_modules.empty()) {
    std::cout << "Loaded Modules (" << loaded_modules.size()
              << "):" << std::endl;
    for (size_t i = 0; i < loaded_modules.size(); ++i) {
      std::cout << "  [" << (i + 1) << "] " << loaded_modules[i] << std::endl;
    }
  } else {
    std::cout << "No modules loaded!" << std::endl;
  }

  if (!custom_mutators.empty()) {
    std::cout << "Active Custom Mutators:" << std::endl;
    for (const auto &mutator : custom_mutators) {
      std::cout << "  - " << mutator << std::endl;
    }
  }

  std::cout << "=========================================" << std::endl;
}
} // namespace bitcoinfuzz