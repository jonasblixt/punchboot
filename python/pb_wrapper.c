#include <stdio.h>
#include <Python.h>
#include <pb-tools/wire.h>
#include <pb-tools/api.h>

static PyModuleDef module =
{
   PyModuleDef_HEAD_INIT,
   .m_name = "punchboot",
   .m_doc = NULL,
   .m_size = -1,
   .m_methods = NULL,
};

PyMODINIT_FUNC PyInit_punchboot(void)
{
    PyObject *m_p;
    struct pb_context *ctx;

    /* Module creation. */
    m_p = PyModule_Create(&module);

    if (m_p == NULL)
    {
        return (NULL);
    }

    pb_api_create_context(&ctx, NULL);
    return (m_p);
}
