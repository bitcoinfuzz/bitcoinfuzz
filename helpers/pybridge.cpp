#define PY_SSIZE_T_CLEAN
#include <Python.h>

#include "pybridge.h"

#include <bitcoinfuzz/ffi.h>
#include <bitcoinfuzz/result.h>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <map>

namespace bitcoinfuzz {
namespace {
// Wrappers are imported from the source tree, which the Docker image keeps at
// the build path, like the JVM jars JvmLoader loads.
const std::filesystem::path kRoot{BITCOINFUZZ_DIR};

[[noreturn]] void Die(const std::string &what) {
  std::fprintf(stderr, "pybridge: %s\n", what.c_str());
  if (PyErr_Occurred() != nullptr)
    PyErr_Print();
  std::abort();
}

class Gil {
public:
  Gil() : state_(PyGILState_Ensure()) {}
  ~Gil() { PyGILState_Release(state_); }
  Gil(const Gil &) = delete;
  Gil &operator=(const Gil &) = delete;

private:
  PyGILState_STATE state_;
};

void PrependToPath(const std::filesystem::path &dir) {
  PyObject *sys = PyImport_ImportModule("sys");
  PyObject *path =
      sys != nullptr ? PyObject_GetAttrString(sys, "path") : nullptr;
  PyObject *entry = PyUnicode_FromString(dir.c_str());
  if (path == nullptr || entry == nullptr || PyList_Insert(path, 0, entry) < 0)
    Die("cannot add " + dir.string() + " to sys.path");
  Py_DECREF(entry);
  Py_DECREF(path);
  Py_DECREF(sys);
}

void Initialize() {
  if (Py_IsInitialized())
    return;
  Py_Initialize();
  std::atexit([] {
    if (Py_IsInitialized())
      Py_Finalize();
  });
  Gil gil;
  PrependToPath(kRoot / "include" / "bitcoinfuzz");
}

PyObject *Import(const std::string &module) {
  static std::map<std::string, PyObject *> libs;
  auto it = libs.find(module);
  if (it != libs.end())
    return it->second;
  PrependToPath(kRoot / "modules" / module);
  PyObject *lib = PyImport_ImportModule((module + "_lib").c_str());
  if (lib == nullptr)
    Die("cannot import " + module + "_lib");
  return libs.emplace(module, lib).first->second;
}

// Returns a new reference to the result of module.function(input).
PyObject *Call(const std::string &module, const std::string &function,
               std::span<const uint8_t> input) {
  static std::map<std::string, PyObject *> functions;
  const std::string name = module + "_lib." + function;
  auto it = functions.find(name);
  if (it == functions.end()) {
    PyObject *fn = PyObject_GetAttrString(Import(module), function.c_str());
    if (fn == nullptr || !PyCallable_Check(fn))
      Die(name + " is not a function");
    it = functions.emplace(name, fn).first;
  }

  PyObject *arg =
      PyBytes_FromStringAndSize(reinterpret_cast<const char *>(input.data()),
                                static_cast<Py_ssize_t>(input.size()));
  if (arg == nullptr)
    Die("cannot build the input for " + name);
  PyObject *result = PyObject_CallOneArg(it->second, arg);
  Py_DECREF(arg);
  if (result == nullptr)
    Die(name + " raised");
  return result;
}
} // namespace

std::optional<std::string> CallPython(const char *module, const char *function,
                                      std::span<const uint8_t> input) {
  Initialize();
  Gil gil;
  PyObject *result = Call(module, function, input);
  PyObject *status = PyObject_GetAttrString(result, "status");
  PyObject *value = PyObject_GetAttrString(result, "value");
  if (status == nullptr || value == nullptr || !PyLong_Check(status) ||
      !PyBytes_Check(value))
    Die(std::string(module) + "_lib." + function +
        " did not return a bfresult.BfResult");

  const long code = PyLong_AsLong(status);
  std::string data(PyBytes_AS_STRING(value),
                   static_cast<size_t>(PyBytes_GET_SIZE(value)));
  Py_DECREF(value);
  Py_DECREF(status);
  Py_DECREF(result);

  switch (code) {
  case BF_OK:
    return Ok(std::move(data));
  case BF_FAIL:
    return Fail(std::move(data));
  case BF_SKIP:
    return Skip();
  }
  Die(std::string(module) + "_lib." + function + " returned an unknown status");
}

bool CallPythonBool(const char *module, const char *function,
                    std::span<const uint8_t> input) {
  Initialize();
  Gil gil;
  PyObject *result = Call(module, function, input);
  if (!PyBool_Check(result))
    Die(std::string(module) + "_lib." + function + " did not return a bool");
  const bool value = result == Py_True;
  Py_DECREF(result);
  return value;
}
} // namespace bitcoinfuzz
