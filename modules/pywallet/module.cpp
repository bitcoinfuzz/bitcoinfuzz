#include "module.h"
#include <Python.h>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <span>
#include <string>

static void python_cleanup() {
  if (Py_IsInitialized()) {
    Py_Finalize();
  }
}

template <typename InputType, typename ReturnType>
static ReturnType
call_python_function(const InputType input, size_t input_len,
                     const char *function_name,
                     PyObject *(*create_input_object)(const InputType, size_t),
                     ReturnType (*convert_result)(PyObject *)) {
  static bool cleanup_registered = false;
  if (!Py_IsInitialized()) {
    Py_Initialize();
    PyRun_SimpleString("import sys; sys.path.append('.')");
    if (!cleanup_registered) {
      atexit(python_cleanup);
      cleanup_registered = true;
    }
  }

  PyGILState_STATE gstate = PyGILState_Ensure();

  ReturnType return_value = {};
  PyObject *main_module = NULL;
  PyObject *parse_func = NULL;
  PyObject *args = NULL;
  PyObject *result = NULL;
  PyObject *input_obj = NULL;

  // Prefer the generated top-level entrypoint used by local builds.
  main_module = PyImport_ImportModule("pywallet_main");
  if (!main_module) {
    // Docker images copy module sources under /app/modules/**/*.py.
    // Fallback to the in-tree module path to keep container runs working.
    PyErr_Clear();
    main_module = PyImport_ImportModule("modules.pywallet.pywallet_lib");
  }
  if (!main_module) {
    goto cleanup;
  }

  // Get the function from the module
  parse_func = PyObject_GetAttrString(main_module, function_name);
  if (!parse_func || !PyCallable_Check(parse_func)) {
    goto cleanup;
  }

  // Create Python object from input
  input_obj = create_input_object(input, input_len);
  if (!input_obj) {
    goto cleanup;
  }

  // Create arguments for the function call
  args = PyTuple_New(1);
  if (!args) {
    goto cleanup;
  }

  // Add the input object to the tuple
  if (PyTuple_SetItem(args, 0, input_obj) < 0) {
    goto cleanup;
  }
  input_obj = NULL; // Ownership transferred to args

  // Call the Python function
  result = PyObject_CallObject(parse_func, args);
  if (!result) {
    goto cleanup;
  }

  // Convert the Python result to the C++ return type
  return_value = convert_result(result);

cleanup:
  Py_XDECREF(result);
  Py_XDECREF(args);
  Py_XDECREF(parse_func);
  Py_XDECREF(main_module);
  Py_XDECREF(input_obj);

  if (PyErr_Occurred()) {
    PyErr_Print();
    PyObject *ptype, *pvalue, *ptraceback;
    PyErr_Fetch(&ptype, &pvalue, &ptraceback);

    Py_XDECREF(ptype);
    Py_XDECREF(pvalue);
    Py_XDECREF(ptraceback);
  }

  PyGILState_Release(gstate);

  return return_value;
}

static PyObject *create_bytes_object(const uint8_t *data, size_t len) {
  return PyBytes_FromStringAndSize(reinterpret_cast<const char *>(data),
                                   static_cast<Py_ssize_t>(len));
}

static char *convert_to_string(PyObject *result) {
  if (PyUnicode_Check(result)) {
    const char *temp = PyUnicode_AsUTF8(result);
    if (temp) {
      return strdup(temp);
    }
  }
  return nullptr;
}

static char *bip32_master_keygen_py(const uint8_t *data, size_t len) {
  return call_python_function<const uint8_t *, char *>(
      data, len, "bip32_master_keygen", create_bytes_object, convert_to_string);
}

namespace bitcoinfuzz {
namespace module {

Pywallet::Pywallet(void) : BaseModule("Pywallet") {}

std::optional<std::string>
Pywallet::bip32_master_keygen(std::span<const uint8_t> buffer) const {
  auto result_ptr = bip32_master_keygen_py(buffer.data(), buffer.size());
  if (result_ptr == nullptr)
    return std::nullopt;

  std::string result(result_ptr);
  free(result_ptr);
  return result;
}

} // namespace module
} // namespace bitcoinfuzz