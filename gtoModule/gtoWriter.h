//
// Copyright (c) 2009, Tweak Software
// All rights reserved.
// 
// SPDX-License-Identifier: Apache-2.0 
//

#ifndef __GTOWRITER_H__
#define __GTOWRITER_H__

#include "gtomodule.h"
#include <Gto/Writer.h>
#include <string>
#include <vector>

namespace PyGto
{

  static char writerDocString[] =
      "\n"
      "The Writer class is designed as an API to a state machine. You indicate\n"
      "a conceptual hierarchy to the file and then all the data. The writer\n"
      "handles generating the string table, the header information, etc.\n"
      "\n";

  // *****************************************************************************
  // Function prototypes
  int gtoWriter_init(PyObject *self, PyObject *args, PyObject *kwds);
  void gtoWriter_PyObject_dealloc(PyObject *self);
  PyObject *gtoWriter_open(PyObject *self, PyObject *args);
  PyObject *gtoWriter_close(PyObject *self);
  PyObject *gtoWriter_beginObject(PyObject *self, PyObject *args);
  PyObject *gtoWriter_endObject(PyObject *self);
  PyObject *gtoWriter_beginComponent(PyObject *self, PyObject *args);
  PyObject *gtoWriter_endComponent(PyObject *self);
  PyObject *gtoWriter_property(PyObject *self, PyObject *args);
  PyObject *gtoWriter_beginData(PyObject *self);
  PyObject *gtoWriter_endData(PyObject *self);
  PyObject *gtoWriter_propertyData(PyObject *self, PyObject *args);
  PyObject *gtoWriter_intern(PyObject *self, PyObject *args);
  PyObject *gtoWriter_lookup(PyObject *self, PyObject *args);

  // *****************************************************************************
  // We need to create a Python object that gets added to
  // <writerInstance>.__dict__ to hold some instance-specific stuff
  typedef struct
  {
    PyObject_HEAD

    // C++ Writer instance to use
    Gto::Writer *m_writer;

    // property currently being written
    int m_propCount;

    // For graceful sanity checking...
    bool m_beginDataCalled;
    bool m_objectDef;
    bool m_componentDef;
    std::vector<std::string> *m_propertyNames;

  } gtoWriter_PyObject;

  // *****************************************************************************
  // Table of methods available in the gtoWriter class
  static PyMethodDef gtoWriterMethods[] = {
      {"open", (PyCFunction)gtoWriter_open, METH_VARARGS,
       "open( filename [, mode] )"},
      {"close", (PyCFunction)gtoWriter_close, METH_NOARGS, "close()"},
      {"beginObject", (PyCFunction)gtoWriter_beginObject, METH_VARARGS,
       "beginObject( string name, string protocol )"},
      {"endObject", (PyCFunction)gtoWriter_endObject, METH_NOARGS, "endObject()"},
      {"beginComponent", (PyCFunction)gtoWriter_beginComponent, METH_VARARGS,
       "beginComponent( string name, transposed = false )"},
      {"endComponent", (PyCFunction)gtoWriter_endComponent, METH_NOARGS,
       "endComponent()"},
      {"property", (PyCFunction)gtoWriter_property, METH_VARARGS,
       "property( string name, int type, int numElements,"
       " int partsPerElement  )"},
      {"intern", (PyCFunction)gtoWriter_intern, METH_O, "intern(string)"},
      {"lookup", (PyCFunction)gtoWriter_lookup, METH_O, "lookup(string)"},
      {"beginData", (PyCFunction)gtoWriter_beginData, METH_NOARGS, "beginData()"},
      {"propertyData", (PyCFunction)gtoWriter_propertyData, METH_VARARGS,
       "propertyData( tuple data )"},
      {"endData", (PyCFunction)gtoWriter_endData, METH_NOARGS, "endData()"},
      {NULL}};

  static PyTypeObject gtoWriter_PyObjectType = {
      PyVarObject_HEAD_INIT(NULL, 0)
      "PyGtoWriterEngine",               // tp_name
      sizeof(gtoWriter_PyObject),        // tp_basicsize
      0,                                 // tp_itemsize
      gtoWriter_PyObject_dealloc,        // tp_dealloc
      0,                                 // tp_print
      0,                                 // tp_getattr
      0,                                 // tp_setattr
      0,                                 // tp_as_async
      0,                                 // tp_repr
      0,                                 // tp_as_number
      0,                                 // tp_as_sequence
      0,                                 // tp_as_mapping
      0,                                 // tp_hash
      0,                                 // tp_call
      0,                                 // tp_str
      0,                                 // tp_getattro
      0,                                 // tp_setattro
      0,                                 // tp_as_buffer
      Py_TPFLAGS_DEFAULT |
          Py_TPFLAGS_BASETYPE,          // tp_flags
      writerDocString,                   // tp_doc
      0,                                 // tp_traverse
      0,                                 // tp_clear
      0,                                 // tp_richcompare
      0,                                 // tp_weaklistoffset
      0,                                 // tp_iter
      0,                                 // tp_iternext
      gtoWriterMethods,                  // tp_methods
      0,                                 // tp_members
      0,                                 // tp_getset
      0,                                 // tp_base
      0,                                 // tp_dict
      0,                                 // tp_descr_get
      0,                                 // tp_decr_set
      0,                                 // tp_dictoffset
      gtoWriter_init,                    // tp_init
      PyType_GenericAlloc,               // tp_alloc
      PyType_GenericNew,                 // tp_new
  };

  // *****************************************************************************
  // Since Python can't convert directly to int...
  inline int PyInt_AsInt(PyObject *p) { return (int)PyLong_AsLong(p); }

  // *****************************************************************************
  // Since Python can't convert directly to float...
  inline float PyFloat_AsFloat(PyObject *p) { return (float)PyFloat_AsDouble(p); }

  // *****************************************************************************
  // Since Python can't convert directly to short...
  inline unsigned short PyInt_AsShort(PyObject *p)
  {
    return (unsigned short)PyLong_AsLong(p);
  }

  // *****************************************************************************
  // Since Python can't convert directly to char...
  inline unsigned char PyInt_AsByte(PyObject *p)
  {
    return (unsigned char)PyLong_AsLong(p);
  }

}; // End namespace PyGto

#endif // End #ifdef __GTOWRITER_H__
