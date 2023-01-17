//
// Copyright (c) 2009, Tweak Software
// All rights reserved.
// 
// SPDX-License-Identifier: Apache-2.0 
//


#ifndef __GTOHEADER_H__
#define __GTOHEADER_H__

#include "gtomodule.h"
#include <Gto/Reader.h>

#include <stddef.h>

// This file defines three Python classes which are used by the Python
// gto.Reader class to pass around information about Objects, Components, and
// Properties.  They are analagous to the C++ Gto::Reader::ObjectInfo,
// Gto::Reader::ComponentInfo, and Gto::Reader::PropertyInfo structs.
//
// Conveniently, the __getattr__ method is defined by Python so we don't have
// to worry about it.  Thus, for example, we can refer to an instance of
// PropertyInfo from Python like so:  pinfo.size

namespace PyGto
{

    // *****************************************************************************
    // C++ Reader helper function prototypes
    PyObject *newObjectInfo(Gto::Reader *reader, const Gto::Reader::ObjectInfo &oi);

    PyObject *newComponentInfo(Gto::Reader *reader,
                               const Gto::Reader::ComponentInfo &ci);

    PyObject *newPropertyInfo(Gto::Reader *reader,
                              const Gto::Reader::PropertyInfo &pi);

    // *****************************************************************************
    // Python method prototypes
    PyObject *type_call(PyObject *self, PyObject *args, PyObject *kwds);

    PyObject *ObjectInfo_new(PyTypeObject *type, PyObject *args, PyObject *kwds);
    void ObjectInfo_dealloc(PyObject* self);
    PyObject *ObjectInfo_repr(PyObject *self);

    PyObject *ComponentInfo_new(PyTypeObject *type, PyObject *args, PyObject *kwds);
    void ComponentInfo_dealloc(PyObject* self);
    PyObject *ComponentInfo_repr(PyObject *self);

    PyObject *PropertyInfo_new(PyTypeObject *type, PyObject *args, PyObject *kwds);
    void PropertyInfo_dealloc(PyObject* self);
    PyObject *PropertyInfo_repr(PyObject *self);

    // *****************************************************************************
    // ObjectInfo type struct
    typedef struct
    {
        PyObject_HEAD

        PyObject *name;
        PyObject *protocolName;
        int protocolVersion;
        int numComponents;
        int pad;

        // objectInfo reference for the reader's cache
        const Gto::Reader::ObjectInfo *oi;
    } ObjectInfo_PyObject;

    static PyMemberDef ObjectInfo_members[] = {
      {"name", T_OBJECT_EX, offsetof(ObjectInfo_PyObject, name), 0, "name"},
      {"protocolName", T_OBJECT_EX, offsetof(ObjectInfo_PyObject, protocolName), 0, "protocolName"},
      {"protocolVersion", T_INT, offsetof(ObjectInfo_PyObject, protocolVersion), 0, "protocolVersion"},
      {"numComponents", T_INT, offsetof(ObjectInfo_PyObject, numComponents), 0, "numComponents"},
      {"pad", T_INT, offsetof(ObjectInfo_PyObject, pad), 0, "pad"},
      {NULL}
    };


    static PyTypeObject ObjectInfo_PyObjectType = {
        PyVarObject_HEAD_INIT(NULL, 0)
        "PyGtoObjectInfo",           // tp_name
        sizeof(ObjectInfo_PyObject), // tp_basicsize
        0,                           // tp_itemsize
        ObjectInfo_dealloc,          // tp_dealloc
        0,                           // tp_print
        0,                           // tp_getattr
        0,                           // tp_setattr
        0,                           // tp_compare
        ObjectInfo_repr,             // tp_repr
        0,                           // tp_as_number
        0,                           // tp_as_sequence
        0,                           // tp_as_mapping
        0,                           // tp_hash
        type_call,                   // tp_call
        0,                           // tp_str
        0,                           // tp_getattro
        0,                           // tp_setattro
        0,                           // tp_as_buffer
        Py_TPFLAGS_DEFAULT,          // tp_flags
        "PyGtoObjectInfo",           // tp_doc
        0,                           // tp_traverse
        0,                           // tp_clear
        0,                           // tp_richcompare
        0,                           // tp_weaklistoffset
        0,                           // tp_iter
        0,                           // tp_iternext
        0,                           // tp_methods
        ObjectInfo_members,          // tp_members
        0,                           // tp_getset
        0,                           // tp_base
        0,                           // tp_dict
        0,                           // tp_descr_get
        0,                           // tp_decr_set
        0,                           // tp_dictoffset
        0,                           // tp_init
        PyType_GenericAlloc,         // tp_alloc
        ObjectInfo_new,              // tp_new
    };

    // *****************************************************************************
    // ComponentInfo type struct
    typedef struct
    {
        PyObject_HEAD
        PyObject *name;
        int numProperties;
        int flags;
        PyObject *interpretation;
        int childLevel;
        PyObject *object;
    } ComponentInfo_PyObject;

    static PyMemberDef ComponentInfo_members[] = {
      {"name", T_OBJECT_EX, offsetof(ComponentInfo_PyObject, name), 0, "name"},
      {"numProperties", T_INT, offsetof(ComponentInfo_PyObject, numProperties), 0, "numProperties"},
      {"flags", T_INT, offsetof(ComponentInfo_PyObject, flags), 0, "flags"},
      {"interpretation", T_OBJECT_EX, offsetof(ComponentInfo_PyObject, interpretation), 0, "interpretation"},
      {"childLevel", T_INT, offsetof(ComponentInfo_PyObject, childLevel), 0, "childLevel"},
      {"object", T_OBJECT_EX, offsetof(ComponentInfo_PyObject, object), 0, "object"},
      {NULL}
    };

    static PyTypeObject ComponentInfo_PyObjectType = {
        PyVarObject_HEAD_INIT(NULL, 0)
        "PyGtoComponentInfo",           // tp_name
        sizeof(ComponentInfo_PyObject), // tp_basicsize
        0,                              // tp_itemsize
        ComponentInfo_dealloc,          // tp_dealloc
        0,                              // tp_print
        0,                              // tp_getattr
        0,                              // tp_setattr
        0,                              // tp_compare
        ComponentInfo_repr,             // tp_repr
        0,                              // tp_as_number
        0,                              // tp_as_sequence
        0,                              // tp_as_mapping
        0,                              // tp_hash
        type_call,                      // tp_call
        0,                              // tp_str
        0,                              // tp_getattro
        0,                              // tp_setattro
        0,                              // tp_as_buffer
        Py_TPFLAGS_DEFAULT,             // tp_flags
        "PyGtoComponentInfo",           // tp_doc
        0,                              // tp_traverse
        0,                              // tp_clear
        0,                              // tp_richcompare
        0,                              // tp_weaklistoffset
        0,                              // tp_iter
        0,                              // tp_iternext
        0,                              // tp_methods
        ComponentInfo_members,          // tp_members
        0,                              // tp_getset
        0,                              // tp_base
        0,                              // tp_dict
        0,                              // tp_descr_get
        0,                              // tp_decr_set
        0,                              // tp_dictoffset
        0,                              // tp_init
        PyType_GenericAlloc,            // tp_alloc
        ComponentInfo_new,              // tp_new
    };

    // *****************************************************************************
    // PropertyInfo type struct
    typedef struct
    {
        PyObject_HEAD
        PyObject *name;
        int size;
        int type;
        int width;
        PyObject *dimensions;
        PyObject *interpretation;
        int pad;
        PyObject *component;
    } PropertyInfo_PyObject;

    static PyMemberDef PropertyInfo_members[] = {
        {"name", T_OBJECT_EX, offsetof(PropertyInfo_PyObject, name), 0, "name"},
        {"size", T_INT, offsetof(PropertyInfo_PyObject, size), 0, "size"},
        {"type", T_INT, offsetof(PropertyInfo_PyObject, type), 0, "type"},
        {"width", T_INT, offsetof(PropertyInfo_PyObject, width), 0, "width"},
        {"dimensions", T_OBJECT_EX, offsetof(PropertyInfo_PyObject, dimensions), 0, "dimensions"},
        {"interpretation", T_OBJECT_EX, offsetof(PropertyInfo_PyObject, interpretation), 0, "interpretation"},
        {"pad", T_INT, offsetof(PropertyInfo_PyObject, pad), 0, "pad"},
        {"component", T_OBJECT_EX, offsetof(PropertyInfo_PyObject, component), 0, "component"},
        {NULL}
    };


    static PyTypeObject PropertyInfo_PyObjectType = {
        PyVarObject_HEAD_INIT(NULL, 0)
        "PyGtoPropertyInfo",           // tp_name
        sizeof(PropertyInfo_PyObject), // tp_basicsize
        0,                             // tp_itemsize
        PropertyInfo_dealloc,          // tp_dealloc
        0,                             // tp_print
        0,                             // tp_getattr
        0,                             // tp_setattr
        0,                             // tp_compare
        PropertyInfo_repr,             // tp_repr
        0,                             // tp_as_number
        0,                             // tp_as_sequence
        0,                             // tp_as_mapping
        0,                             // tp_hash
        type_call,                     // tp_call
        0,                             // tp_str
        0,                             // tp_getattro
        0,                             // tp_setattro
        0,                             // tp_as_buffer
        Py_TPFLAGS_DEFAULT,            // tp_flags
        "PyGtoPropertyInfo",           // tp_doc
        0,                             // tp_traverse
        0,                             // tp_clear
        0,                             // tp_richcompare
        0,                             // tp_weaklistoffset
        0,                             // tp_iter
        0,                             // tp_iternext
        0,                             // tp_methods
        PropertyInfo_members,          // tp_members
        0,                             // tp_getset
        0,                             // tp_base
        0,                             // tp_dict
        0,                             // tp_descr_get
        0,                             // tp_decr_set
        0,                             // tp_dictoffset
        0,                             // tp_init
        PyType_GenericAlloc,           // tp_alloc
        PropertyInfo_new,              // tp_new
    };

} //  End namespace PyGto

#endif // End #ifdef __GTOHEADER_H__
