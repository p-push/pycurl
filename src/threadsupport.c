#include "pycurl.h"

#ifdef WITH_THREAD

PYCURL_INTERNAL PyThreadState *
pycurl_get_thread_state(const CurlObject *self)
{
    /* Get the thread state for callbacks to run in.
     * This is either `self->state' when running inside perform() or
     * `self->multi_stack->state' when running inside multi_perform().
     * When the result is != NULL we also implicitly assert
     * a valid `self->handle'.
     */
    if (self == NULL)
        return NULL;
    assert(Py_TYPE(self) == p_Curl_Type);
    if (self->state != NULL)
    {
        /* inside perform() */
        assert(self->handle != NULL);
        if (self->multi_stack != NULL) {
            assert(self->multi_stack->state == NULL);
        }
        return self->state;
    }
    if (self->multi_stack != NULL && self->multi_stack->state != NULL)
    {
        /* inside multi_perform() */
        assert(self->handle != NULL);
        assert(self->multi_stack->multi_handle != NULL);
        assert(self->state == NULL);
        return self->multi_stack->state;
    }
    return NULL;
}


PYCURL_INTERNAL PyThreadState *
pycurl_get_thread_state_multi(const CurlMultiObject *self)
{
    /* Get the thread state for callbacks to run in when given
     * multi handles instead of regular handles
     */
    if (self == NULL)
        return NULL;
    assert(Py_TYPE(self) == p_CurlMulti_Type);
    if (self->state != NULL)
    {
        /* inside multi_perform() */
        assert(self->multi_handle != NULL);
        return self->state;
    }
    return NULL;
}


PYCURL_INTERNAL int
pycurl_acquire_thread(const CurlObject *self, PyThreadState **state)
{
    *state = pycurl_get_thread_state(self);
    if (*state == NULL)
        return 0;
    PyEval_AcquireThread(*state);
    return 1;
}


PYCURL_INTERNAL int
pycurl_acquire_thread_multi(const CurlMultiObject *self, PyThreadState **state)
{
    *state = pycurl_get_thread_state_multi(self);
    if (*state == NULL)
        return 0;
    PyEval_AcquireThread(*state);
    return 1;
}


PYCURL_INTERNAL void
pycurl_release_thread(PyThreadState *state)
{
    PyEval_ReleaseThread(state);
}

/*************************************************************************
// SSL TSL
**************************************************************************/

#ifdef PYCURL_NEED_OPENSSL_TSL

static PyThread_type_lock *pycurl_openssl_tsl = NULL;

static void
pycurl_ssl_lock(int mode, int n, const char * file, int line)
{
    if (mode & CRYPTO_LOCK) {
        PyThread_acquire_lock(pycurl_openssl_tsl[n], 1);
    } else {
        PyThread_release_lock(pycurl_openssl_tsl[n]);
    }
}

static unsigned long
pycurl_ssl_id(void)
{
    return (unsigned long) PyThread_get_thread_ident();
}

PYCURL_INTERNAL int
pycurl_ssl_init(void)
{
    int i, c = CRYPTO_num_locks();

    pycurl_openssl_tsl = PyMem_New(PyThread_type_lock, c);
    if (pycurl_openssl_tsl == NULL) {
        PyErr_NoMemory();
        return -1;
    }
    memset(pycurl_openssl_tsl, 0, sizeof(PyThread_type_lock) * c);

    for (i = 0; i < c; ++i) {
        pycurl_openssl_tsl[i] = PyThread_allocate_lock();
        if (pycurl_openssl_tsl[i] == NULL) {
            for (--i; i >= 0; --i) {
                PyThread_free_lock(pycurl_openssl_tsl[i]);
            }
            PyMem_Free(pycurl_openssl_tsl);
            PyErr_NoMemory();
            return -1;
        }
    }

    CRYPTO_set_id_callback(pycurl_ssl_id);
    CRYPTO_set_locking_callback(pycurl_ssl_lock);
    return 0;
}

PYCURL_INTERNAL void
pycurl_ssl_cleanup(void)
{
    if (pycurl_openssl_tsl) {
        int i, c = CRYPTO_num_locks();

        CRYPTO_set_id_callback(NULL);
        CRYPTO_set_locking_callback(NULL);

        for (i = 0; i < c; ++i) {
            PyThread_free_lock(pycurl_openssl_tsl[i]);
        }

        PyMem_Free(pycurl_openssl_tsl);
        pycurl_openssl_tsl = NULL;
    }
}
#endif

#ifdef PYCURL_NEED_GNUTLS_TSL
static int
pycurl_ssl_mutex_create(void **m)
{
    if ((*((PyThread_type_lock *) m) = PyThread_allocate_lock()) == NULL) {
        return -1;
    } else {
        return 0;
    }
}

static int
pycurl_ssl_mutex_destroy(void **m)
{
    PyThread_free_lock(*((PyThread_type_lock *) m));
    return 0;
}

static int
pycurl_ssl_mutex_lock(void **m)
{
    return !PyThread_acquire_lock(*((PyThread_type_lock *) m), 1);
}

static int
pycurl_ssl_mutex_unlock(void **m)
{
    PyThread_release_lock(*((PyThread_type_lock *) m));
    return 0;
}

static struct gcry_thread_cbs pycurl_gnutls_tsl = {
    GCRY_THREAD_OPTION_USER,
    NULL,
    pycurl_ssl_mutex_create,
    pycurl_ssl_mutex_destroy,
    pycurl_ssl_mutex_lock,
    pycurl_ssl_mutex_unlock
};

PYCURL_INTERNAL int
pycurl_ssl_init(void)
{
    gcry_control(GCRYCTL_SET_THREAD_CBS, &pycurl_gnutls_tsl);
    return 0;
}

typedef struct PycurlGnutlsTslObject {
    PyObject_HEAD
    PyObject *dict;
} PycurlGnuTsl;

PYCURL_INTERNAL PyTypeObject PycurlGnutlsTsl_Type = {
#if PY_MAJOR_VERSION >= 3
    PyVarObject_HEAD_INIT(NULL, 0)
#else
    PyObject_HEAD_INIT(NULL)
    0,                          /* ob_size */
#endif
    "pycurl.GnutlsTsl",         /* tp_name */
    sizeof(PyTypeObject),    /* tp_basicsize */
    0,                          /* tp_itemsize */
    (destructor)PyObject_Del, /* tp_dealloc */
    0,                          /* tp_print */
#if PY_MAJOR_VERSION >= 3
    0, // (getattrfunc)do_curl_getattr,  /* tp_getattr */
    0, //(setattrfunc)do_curl_setattr,  /* tp_setattr */
#else
    0,  /* tp_getattr */
    0,  /* tp_setattr */
#endif
    0,                          /* tp_reserved */
    0,                          /* tp_repr */
    0,                          /* tp_as_number */
    0,                          /* tp_as_sequence */
    0,                          /* tp_as_mapping */
    0,                          /* tp_hash  */
    0,                          /* tp_call */
    0,                          /* tp_str */
#if PY_MAJOR_VERSION >= 3
    (getattrofunc)PyObject_GenericGetAttr, /* tp_getattro */
    (setattrofunc)PyObject_GenericSetAttr, /* tp_setattro */
#else
    0,                          /* tp_getattro */
    0,                          /* tp_setattro */
#endif
    0,                          /* tp_as_buffer */
    0,         /* tp_flags */
    0,                   /* tp_doc */
    0, /* tp_traverse */
    0,    /* tp_clear */
    0,                          /* tp_richcompare */
    0,                          /* tp_weaklistoffset */
    0,                          /* tp_iter */
    0,                          /* tp_iternext */
    0,    /* tp_methods */
    0,                          /* tp_members */
    0,                          /* tp_getset */
    0,                          /* tp_base */
    0,                          /* tp_dict */
    0,                          /* tp_descr_get */
    0,                          /* tp_descr_set */
    0,                          /* tp_dictoffset */
    0,                          /* tp_init */
    PyType_GenericAlloc,        /* tp_alloc */
    0,      /* tp_new */
    0,            /* tp_free */
};

PYCURL_INTERNAL void
pycurl_ssl_init_ctypes(void)
{
    PyObject *ctypes_module = NULL;
    PyObject *cdll = NULL;

    ctypes_module = PyImport_ImportModule("ctypes");
    if (ctypes_module == NULL) {
        // fail
    }
    
    cdll = PyObject_GetAttr(ctypes_module, "CDLL");
    if (cdll == NULL) {
        // fail
    }
    
    library = PyObject_CallMethod(ctypes_module, "CDLL", "libgnutls.so");
    if (library == NULL) {
        // fail
    }
    
    Py_TYPE(&PycurlGnutlsTsl_Type) = &PyType_Type;
    
    if (PyType_Ready(&PycurlGnutlsTsl_Type) < 0) {
        // fail
    }
    
    cls = PyObject_New(PyType_Type, &PycurlGnutlsTsl_Type);
    
    Py_DECREF(ctypes_module);
    gcry_control(GCRYCTL_SET_THREAD_CBS, &pycurl_gnutls_tsl);
}

PYCURL_INTERNAL void
pycurl_ssl_cleanup(void)
{
    return;
}
#endif



/*************************************************************************
// CurlShareObject
**************************************************************************/

PYCURL_INTERNAL void
share_lock_lock(ShareLock *lock, curl_lock_data data)
{
    PyThread_acquire_lock(lock->locks[data], 1);
}

PYCURL_INTERNAL void
share_lock_unlock(ShareLock *lock, curl_lock_data data)
{
    PyThread_release_lock(lock->locks[data]);
}

PYCURL_INTERNAL ShareLock *
share_lock_new(void)
{
    int i;
    ShareLock *lock = PyMem_New(ShareLock, 1);
    if (lock == NULL) {
        PyErr_NoMemory();
        return NULL;
    }

    for (i = 0; i < CURL_LOCK_DATA_LAST; ++i) {
        lock->locks[i] = PyThread_allocate_lock();
        if (lock->locks[i] == NULL) {
            PyErr_NoMemory();
            goto error;
        }
    }
    return lock;

error:
    for (--i; i >= 0; --i) {
        PyThread_free_lock(lock->locks[i]);
        lock->locks[i] = NULL;
    }
    PyMem_Free(lock);
    return NULL;
}

PYCURL_INTERNAL void
share_lock_destroy(ShareLock *lock)
{
    int i;

    assert(lock);
    for (i = 0; i < CURL_LOCK_DATA_LAST; ++i){
        assert(lock->locks[i] != NULL);
        PyThread_free_lock(lock->locks[i]);
    }
    PyMem_Free(lock);
    lock = NULL;
}

PYCURL_INTERNAL void
share_lock_callback(CURL *handle, curl_lock_data data, curl_lock_access locktype, void *userptr)
{
    CurlShareObject *share = (CurlShareObject*)userptr;
    share_lock_lock(share->lock, data);
}

PYCURL_INTERNAL void
share_unlock_callback(CURL *handle, curl_lock_data data, void *userptr)
{
    CurlShareObject *share = (CurlShareObject*)userptr;
    share_lock_unlock(share->lock, data);
}

#else /* WITH_THREAD */

#if defined(PYCURL_NEED_SSL_TSL)
PYCURL_INTERNAL void
pycurl_ssl_init(void)
{
    return 0;
}

PYCURL_INTERNAL void
pycurl_ssl_cleanup(void)
{
    return;
}
#endif

#endif /* WITH_THREAD */

/* vi:ts=4:et:nowrap
 */
