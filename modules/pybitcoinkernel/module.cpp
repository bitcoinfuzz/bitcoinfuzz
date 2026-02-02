#include "module.h"
#include <Python.h>
#include <cstring>
#include <iostream>
#include <span>
#include <string>
#include <optional>
#include <mutex>

static std::once_flag python_init_flag;

static void initialize_python_once() {
  std::call_once(python_init_flag, []() {
    Py_InitializeEx(0);
    PyEval_InitThreads();
    PyRun_SimpleString("import sys; sys.path.append('.')");
  });
}

template <typename ReturnType>
static ReturnType
call_python_function(const uint8_t *input, size_t input_len,
                     const char *function_name,
                     PyObject *(*create_input_object)(const uint8_t *, size_t),
                     ReturnType (*convert_result)(PyObject *)) {
  initialize_python_once();

  PyGILState_STATE gstate = PyGILState_Ensure();

  ReturnType return_value{};
  PyObject *main_module = nullptr;
  PyObject *parse_func = nullptr;
  PyObject *args = nullptr;
  PyObject *result = nullptr;
  PyObject *input_obj = nullptr;

  main_module = PyImport_ImportModule("main");
  if (!main_module) goto cleanup;

  parse_func = PyObject_GetAttrString(main_module, function_name);
  if (!parse_func || !PyCallable_Check(parse_func)) goto cleanup;

  input_obj = create_input_object(input, input_len);
  if (!input_obj) goto cleanup;

  args = PyTuple_New(1);
  if (!args) goto cleanup;

  if (PyTuple_SetItem(args, 0, input_obj) < 0) goto cleanup;
  input_obj = nullptr; // reference stolen

  result = PyObject_CallObject(parse_func, args);
  if (!result) goto cleanup;

  return_value = convert_result(result);

cleanup:
  Py_XDECREF(result);
  Py_XDECREF(args);
  Py_XDECREF(parse_func);
  Py_XDECREF(main_module);
  Py_XDECREF(input_obj);

  if (PyErr_Occurred()) {
    PyErr_Print();
  }

  PyGILState_Release(gstate);
  return return_value;
}

static PyObject *create_bytes_object(const uint8_t *data, size_t len) {
  return PyBytes_FromStringAndSize(reinterpret_cast<const char *>(data), len);
}

static char *convert_to_string(PyObject *result) {
  if (PyUnicode_Check(result)) {
    const char *temp = PyUnicode_AsUTF8(result);
    if (temp) {
      return strdup(temp); // caller owns
    }
  }
  return nullptr;
}

char *block_parse(const uint8_t *data, size_t len) {
  return call_python_function<char *>(
      data, len, "block_parse", create_bytes_object, convert_to_string);
}

char *transaction_parse(const uint8_t *data, size_t len) {
  return call_python_function<char *>(
      data, len, "transaction_parse", create_bytes_object, convert_to_string);
}

namespace bitcoinfuzz {
namespace module {

Pybitcoinkernel::Pybitcoinkernel(void)
    : BaseModule("Pybitcoinkernel") {}

std::optional<std::string>
Pybitcoinkernel::kernel_transaction(std::span<const uint8_t> buffer) const {
  char *result_ptr = transaction_parse(buffer.data(), buffer.size());
  if (!result_ptr)
    return std::nullopt;

  std::string result(result_ptr);
  free(result_ptr);
  return result;
}

std::optional<std::string>
Pybitcoinkernel::kernel_block(std::span<const uint8_t> buffer) const {
  char *result_ptr = block_parse(buffer.data(), buffer.size());
  if (!result_ptr)
    return std::nullopt;

  std::string result(result_ptr);
  free(result_ptr);
  return result;
}

} // namespace module
} // namespace bitcoinfuzz
