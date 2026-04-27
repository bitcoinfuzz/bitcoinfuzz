#pragma once

#include <string>
#include <vector>

namespace bitcoinfuzz {
class ModuleLogger {
private:
  std::vector<std::string> loaded_modules;
  std::vector<std::string> custom_mutators;
  bool logged = false;

public:
  void addModule(const std::string &module_name);
  void addCustomMutator(const std::string &mutator_name);
  void logModules();
};
} // namespace bitcoinfuzz