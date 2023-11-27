#ifndef PB_PYTHON_WRAPPER_H
#define PB_PYTHON_WRAPPER_H

#include <Python.h>

int pb_exceptions_init(PyObject *mod);
void *pb_exception_from_rc(int err);

#endif
