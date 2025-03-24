#include <Python.h>
#include <string>
#include <iostream>
#include <cstring>

static bool call_python_parser(const char* input, const char* function_name) {
    if (!Py_IsInitialized()) {
        Py_Initialize();
        PyRun_SimpleString("import sys; sys.path.append('.')");
    }
    
    PyGILState_STATE gstate;
    gstate = PyGILState_Ensure();
    
    bool success = false;
    PyObject* main_module = NULL;
    PyObject* parse_func = NULL;
    PyObject* args = NULL;
    PyObject* result = NULL;
    PyObject* input_str = NULL;
    
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
    
    // Create a Python string from the input
    input_str = PyUnicode_FromString(input);
    if (!input_str) {
        goto cleanup;
    }
    
    // Create arguments for the function call
    args = PyTuple_New(1);
    if (!args) {
        goto cleanup;
    }
    
    // Add the input string to the tuple
    if (PyTuple_SetItem(args, 0, input_str) < 0) {
        goto cleanup;
    }
    input_str = NULL;  // Ownership transferred to args
    
    // Call the Python function
    result = PyObject_CallObject(parse_func, args);
    if (!result) {
        goto cleanup;
    }
    
    // Convert Python boolean result to C++ bool
    if (PyBool_Check(result)) {
        success = (result == Py_True);
    }
    
cleanup:
    Py_XDECREF(result);
    Py_XDECREF(args);
    Py_XDECREF(parse_func);
    Py_XDECREF(main_module);
    Py_XDECREF(input_str);
    
    PyGILState_Release(gstate);
    
    return success;
}

extern "C" bool embit_miniscript_miniscript_parse(const char* input) {
    return call_python_parser(input, "miniscript_parse");
}

extern "C" bool embit_descriptor_parse(const char* input) {
    return call_python_parser(input, "descriptor_parse");
}