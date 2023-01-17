//
// Copyright (c) 2009, Tweak Software
// All rights reserved.
// 
// SPDX-License-Identifier: Apache-2.0 
//

//
// This is the gto module for Python 2.2.1.  It may or may not work with other
// versions.
//

#include "gtomodule.h"
#include "gtoHeader.h"
#include "gtoReader.h"
#include "gtoWriter.h"

#include <vector>

namespace PyGto
{
  // A python exception object
  static PyObject* g_gtoError = NULL;

  // *****************************************************************************
  // Just returns a pointer to the module-wide g_gtoError object
  PyObject* gtoError()
  {
    return g_gtoError;
  }

  // *****************************************************************************
  // Returns the Python type name of an object as a string
  const char* PyTypeName( PyObject* object )
  {
    // Figure out the class name (as a string)
    PyObject* itemClass = PyObject_GetAttrString( object, "__class__" );

    if( itemClass == NULL )
    {
      return nullptr;
    }

    PyObject* itemClassName = PyObject_GetAttrString( itemClass, "__name__" );
    Py_XDECREF(itemClass);

    if( itemClassName == NULL )
    {
      return nullptr;
    }

    const char* typeName = PyBytes_AsString( itemClassName );
    Py_XDECREF(itemClassName);
    return typeName;
  }

  bool isInstance( PyObject* object )
  {
    return !( PyLong_Check(object)  ||
              PyFloat_Check(object) || PyBytes_Check(object) ||
              PyTuple_Check(object) || PyList_Check(object) );
  }

}  // End namespace PyGto

// This module has no module-scope methods
static PyMethodDef ModuleMethods[] = {{NULL}};

static void gto_free(void *self)
{
  Py_XDECREF(PyGto::g_gtoError);
}

static struct PyModuleDef ModuleDef = {
    {
        1,    /* ob_refcnt */
        NULL, /* ob_type */
        NULL, /* m_init */
        0,    /* m_index */
        NULL, /* m_copy */
    },
    "gto", /* m_name */
    NULL,  /* m_doc */
    -1,    /* m_size */
    NULL,  /* m_methods */
    NULL,  /* m_reload */
    NULL,  /* m_traverse */
    NULL,  /* m_clear */
    gto_free,  /* m_free */
};

// *****************************************************************************
static void defineConstants( PyObject* moduleDict )
{
  PyObject *tmp = PyBytes_FromString("gto I/O module  v3.01\n"
                                      "Copyright (c) 2020 Autodesk\n"
                                      "Compiled on " __DATE__ " at " __TIME__);
  PyDict_SetItemString(moduleDict, "__doc__", tmp);
  Py_XDECREF(tmp);

  tmp = PyLong_FromLong(Gto::Transposed);
  PyDict_SetItemString(moduleDict, "TRANSPOSED", tmp);
  Py_XDECREF(tmp);

  tmp = PyLong_FromLong(Gto::Matrix);
  PyDict_SetItemString(moduleDict, "MATRIX", tmp);
  Py_XDECREF(tmp);

  tmp = PyLong_FromLong(Gto::Int);
  PyDict_SetItemString(moduleDict, "INT", tmp);
  Py_XDECREF(tmp);

  tmp = PyLong_FromLong(Gto::Float);
  PyDict_SetItemString(moduleDict, "FLOAT", tmp);
  Py_XDECREF(tmp);

  tmp = PyLong_FromLong(Gto::Double);
  PyDict_SetItemString(moduleDict, "DOUBLE", tmp);
  Py_XDECREF(tmp);

  tmp = PyLong_FromLong(Gto::Half);
  PyDict_SetItemString(moduleDict, "HALF", tmp);
  Py_XDECREF(tmp);

  tmp = PyLong_FromLong(Gto::String);
  PyDict_SetItemString(moduleDict, "STRING", tmp);
  Py_XDECREF(tmp);

  tmp = PyLong_FromLong(Gto::Boolean);
  PyDict_SetItemString(moduleDict, "BOOLEAN", tmp);
  Py_XDECREF(tmp);

  tmp = PyLong_FromLong(Gto::Short);
  PyDict_SetItemString(moduleDict, "SHORT", tmp);
  Py_XDECREF(tmp);

  tmp = PyLong_FromLong(Gto::Byte);
  PyDict_SetItemString(moduleDict, "BYTE", tmp);
  Py_XDECREF(tmp);

  tmp = PyLong_FromLong(GTO_VERSION);
  PyDict_SetItemString(moduleDict, "GTO_VERSION", tmp);
  Py_XDECREF(tmp);
}

// This function is called by Python when the module is imported in python3
extern "C"
#ifdef PLATFORM_WINDOWS
    __declspec( dllexport ) PyObject*
#else
    PyObject*
#endif
    PyInit_gto()
{
  // Create a new gto module object
  PyObject* module;

  module = PyModule_Create( &ModuleDef );

  PyObject* moduleDict = PyModule_GetDict( module );

  // Create the exception and add it to the module
  PyGto::g_gtoError = PyErr_NewException( "gto.Error", NULL, NULL );
  PyDict_SetItemString( moduleDict, "Error", PyGto::g_gtoError );

  // Add 'constants' to the module
  defineConstants( moduleDict );

  // Create info classes
  PyTypeObject* objectInfoType = &PyGto::ObjectInfo_PyObjectType;
  if( PyType_Ready( objectInfoType ) >= 0 )
  {
    Py_XINCREF( (PyObject*)objectInfoType );
    PyModule_AddObject( module, "ObjectInfo", (PyObject*)objectInfoType );
  }

  PyTypeObject* componentInfoType = &PyGto::ComponentInfo_PyObjectType;
  if( PyType_Ready( componentInfoType ) >= 0 )
  {
    Py_XINCREF( (PyObject*)componentInfoType );
    PyModule_AddObject( module, "ComponentInfo", (PyObject*)componentInfoType );
  }

  PyTypeObject* propertyInfoType = &PyGto::PropertyInfo_PyObjectType;
  if( PyType_Ready( propertyInfoType ) >= 0 )
  {
    Py_XINCREF( (PyObject*)propertyInfoType );
    PyModule_AddObject( module, "PropertyInfo", (PyObject*)propertyInfoType );
  }

  // Create the Reader class
  PyTypeObject* readerType = &PyGto::gtoReader_PyObjectType;
  if( PyType_Ready( readerType ) >= 0 )
  {
    Py_XINCREF(readerType);

    PyModule_AddObject(module, "Reader", (PyObject *) readerType);
    PyObject *readerDict = readerType->tp_dict;

    // Add a couple of Reader-specific constants
    PyObject *tmp = PyLong_FromLong(Gto::Reader::None);
    PyDict_SetItemString(readerDict, "NONE", tmp);
    Py_XDECREF(tmp);

    tmp = PyLong_FromLong(Gto::Reader::HeaderOnly);
    PyDict_SetItemString(readerDict, "HEADERONLY", tmp);

    tmp = PyLong_FromLong(Gto::Reader::RandomAccess);
    PyDict_SetItemString(readerDict, "RANDOMACCESS", tmp);
    Py_XDECREF(tmp);

    tmp = PyLong_FromLong(Gto::Reader::BinaryOnly);
    PyDict_SetItemString(readerDict, "BINARYONLY", tmp);
    Py_XDECREF(tmp);

    tmp = PyLong_FromLong(Gto::Reader::TextOnly);
    PyDict_SetItemString(readerDict, "TEXTONLY", tmp);
    Py_XDECREF(tmp);
  }

  // Create the Writer class
  PyTypeObject* writerType = &PyGto::gtoWriter_PyObjectType;
  if( PyType_Ready( writerType ) >= 0 )
  {
    Py_XINCREF(writerType);

    PyModule_AddObject(module, "Writer", (PyObject *) writerType);
    PyObject *writerDict = readerType->tp_dict;

    // Add a couple of Writer-specific constants
    PyObject *tmp = PyLong_FromLong(Gto::Writer::BinaryGTO);
    PyDict_SetItemString(writerDict, "BINARYGTO", tmp);
    Py_XDECREF(tmp);

    tmp = PyLong_FromLong(Gto::Writer::CompressedGTO);
    PyDict_SetItemString(writerDict, "COMPRESSEDGTO", tmp);
    Py_XDECREF(tmp);

    tmp = PyLong_FromLong(Gto::Writer::TextGTO);
    PyDict_SetItemString(writerDict, "TEXTGTO", tmp);
    Py_XDECREF(tmp);
  }
  return module;
}

// *****************************************************************************
// This function is called by Python when the module is imported in python2
extern "C"
#ifdef PLATFORM_WINDOWS
    __declspec( dllexport ) void
#else
    void
#endif
    initgto()
{
  (void)PyInit_gto();
}
