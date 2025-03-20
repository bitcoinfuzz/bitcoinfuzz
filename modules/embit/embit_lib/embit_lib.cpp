#include <Python.h>
#include <string>
#include <iostream>
#include <cstring>

extern "C" bool embit_miniscript_miniscript_parse(const char* input) {
    if (!Py_IsInitialized()) {
        Py_Initialize();
        
        PyRun_SimpleString("import sys; sys.path.append('.')");
    }
    
    PyGILState_STATE gstate;
    gstate = PyGILState_Ensure();
    
    bool success = false;
    PyObject* main_module = NULL;
    PyObject* miniscript_parse_func = NULL;
    PyObject* args = NULL;
    PyObject* result = NULL;
    PyObject* input_str = NULL;
    
    // Import the main Python module containing miniscript_parse
    main_module = PyImport_ImportModule("main");
    if (!main_module) {
        goto cleanup;
    }
    
    // Get the miniscript_parse function from the module
    miniscript_parse_func = PyObject_GetAttrString(main_module, "miniscript_parse");
    if (!miniscript_parse_func || !PyCallable_Check(miniscript_parse_func)) {
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
    // PyTuple_SetItem steals a reference, so we don't need to decref input_str after
    if (PyTuple_SetItem(args, 0, input_str) < 0) {
        goto cleanup;
    }
    input_str = NULL;  // Ownership transferred to args
    
    // Call the Python function
    result = PyObject_CallObject(miniscript_parse_func, args);
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
    Py_XDECREF(miniscript_parse_func);
    Py_XDECREF(main_module);
    Py_XDECREF(input_str);
    
    PyGILState_Release(gstate);
    
    return success;
}