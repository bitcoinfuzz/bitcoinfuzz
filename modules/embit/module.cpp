#include "module.h"
#include <Python.h>
#include <cstring>
#include <iostream>
#include <span>
#include <string>

template <typename InputType, typename ReturnType>
static ReturnType
call_python_function(const InputType input, size_t input_len,
                     const char *function_name,
                     PyObject *(*create_input_object)(const InputType, size_t),
                     ReturnType (*convert_result)(PyObject *)) {
  if (!Py_IsInitialized()) {
    Py_Initialize();
    PyRun_SimpleString("import sys; sys.path.append('.')");
  }

  PyGILState_STATE gstate = PyGILState_Ensure();

  ReturnType return_value = {};
  PyObject *main_module = NULL;
  PyObject *parse_func = NULL;
  PyObject *args = NULL;
  PyObject *result = NULL;
  PyObject *input_obj = NULL;

  // Import the main Python module containing the parser function
  main_module = PyImport_ImportModule("main");
  if (!main_module) {
    goto cleanup;
  }

  // Get the parser function from the module
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
    PyObject *ptype, *pvalue, *ptraceback;
    PyErr_Fetch(&ptype, &pvalue, &ptraceback);

    Py_XDECREF(ptype);
    Py_XDECREF(pvalue);
    Py_XDECREF(ptraceback);
  }

  PyGILState_Release(gstate);

  return return_value;
}

// helper functions to create Python objects from different input types
static PyObject *create_string_object(const char *input, size_t) {
  return PyUnicode_FromString(input);
}

static PyObject *create_bytes_object(const uint8_t *data, size_t len) {
  return PyBytes_FromStringAndSize((const char *)data, len);
}

// helper functions to convert py results to cpp return types
static bool convert_to_bool(PyObject *result) {
  if (PyBool_Check(result)) {
    return (result == Py_True);
  }
  return false;
}

static char *convert_to_string(PyObject *result) {
  if (PyUnicode_Check(result)) {
    const char *temp = PyUnicode_AsUTF8(result);
    if (temp) {
      return strdup(temp);
    }
  }
  return NULL;
}

bool embit_miniscript_parse(std::string input) {
  return call_python_function<const char *, bool>(
      input.c_str(), input.size(), "miniscript_parse", create_string_object,
      convert_to_bool);
}

bool embit_descriptor_parse(std::string input) {
  return call_python_function<const char *, bool>(
      input.c_str(), input.size(), "descriptor_parse", create_string_object,
      convert_to_bool);
}

char *embit_psbt_parse(const uint8_t *data, size_t len) {
  return call_python_function<const uint8_t *, char *>(
      data, len, "psbt_parse", create_bytes_object, convert_to_string);
}

namespace bitcoinfuzz {
namespace module {
Embit::Embit(void) : BaseModule("Embit") {}

std::optional<bool> Embit::miniscript_parse(std::string str) const {
  return embit_miniscript_parse(str);
}

std::optional<bool> Embit::descriptor_parse(std::string str) const {
  // Skip these fragments since Embit hasn't implemented them
  const std::vector<std::string> fragments = {"combo(", "thresh(", "raw(",
                                              "rawtr(", "pk("};
  for (const auto &fragment : fragments) {
    if (str.find(fragment) != std::string::npos) {
      return std::nullopt;
    }
  }
  return embit_descriptor_parse(str);
}

std::optional<std::string>
Embit::psbt_parse(std::span<const uint8_t> buffer) const {
  auto result_ptr = embit_psbt_parse(buffer.data(), buffer.size());
  if (result_ptr == nullptr)
    return std::nullopt;

  std::string result(result_ptr);
  free(result_ptr);
  return result;
}
} // namespace module
} // namespace bitcoinfuzz
