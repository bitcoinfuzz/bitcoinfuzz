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
#define MODULE_ENTRY(Flag, Name, Class) compiled.insert(Name);
#define CGO_MODULE_ENTRY(Flag, Name, Class) compiled.insert(Name);
#include "module_defs.h"
#undef MODULE_ENTRY
#undef CGO_MODULE_ENTRY
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
