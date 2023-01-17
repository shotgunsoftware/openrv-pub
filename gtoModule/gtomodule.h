//
// Copyright (c) 2009, Tweak Software
// All rights reserved.
// 
// SPDX-License-Identifier: Apache-2.0 
//


#ifndef __GTOMODULE_H__
#define __GTOMODULE_H__

#include <Python.h>
#include <structmember.h>

namespace PyGto {

// *****************************************************************************
// Just returns a pointer to the module-wide g_gtoError object
PyObject *gtoError();

// *****************************************************************************
// Returns the Python type name of an object as a string
const char *PyTypeName(PyObject *object);

bool isInstance(PyObject *object);
}; // namespace PyGto

#endif // End #ifdef __GTOMODULE_H__
