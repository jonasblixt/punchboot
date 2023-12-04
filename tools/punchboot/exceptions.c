#include "python_wrapper.h"
#include <pb-tools/error.h>
#include <Python.h>

static PyObject *pb_exc_base;
static PyObject *pb_exceptions[PB_RESULT_END];

static const char *pb_exc_names[] = {
    "OK", /* OK, is not an exception */
    "GenericError",
    "AuthenticationError",
    "NotAuthenticatedError",
    "NotSupportedError",
    "ArgumentError",
    "CommandError",
    "PartVerifyError",
    "PartNotBootableError",
    "NoMemoryError",
    "TransferError",
    "NotFoundError",
    "StreamNotInitializedError",
    "TimeoutError",
    "KeyRevokedError",
    "SignatureError",
    "MemError",
    "IOError",
};

#ifdef __GNUC__
_Static_assert(sizeof(pb_exc_names) / sizeof(pb_exc_names[0]) == PB_RESULT_END,
               "Number of exception strings do not match error enum.");
#endif

void *pb_exception_from_rc(int err)
{
    if (err == 0)
        return NULL;

    if (-err < 0 || -err >= PB_RESULT_END)
        PyErr_SetNone(pb_exceptions[1]); // Return the generic 'Error' for unknown codes
    else
        PyErr_SetNone(pb_exceptions[-err]);
    return NULL;
}

int pb_exceptions_init(PyObject *mod)
{
    char exc_name_buf[64];

    pb_exc_base = PyErr_NewException("punchboot.Error", pb_exc_base, NULL);
    if (pb_exc_base == NULL)
        return -1;
    if (PyModule_AddObject(mod, "Error", pb_exc_base) < 0) {
        Py_DECREF(pb_exc_base);
        return -1;
    }

    for (int i = 1; i < PB_RESULT_END; i++) {
        snprintf(exc_name_buf, sizeof(exc_name_buf), "punchboot.%s", pb_exc_names[i]);
        pb_exceptions[i] = PyErr_NewException(exc_name_buf, pb_exc_base, NULL);
        if (pb_exceptions[i] == NULL)
            goto err_out;
        if (PyModule_AddObject(mod, pb_exc_names[i], pb_exceptions[i]) < 0)
            goto err_out;
    }
    return 0;
err_out:
    for (int i = 1; i < PB_RESULT_END; i++) {
        if (pb_exceptions[i] != NULL)
            Py_DECREF(pb_exceptions[i]);
    }
    return -1;
}
