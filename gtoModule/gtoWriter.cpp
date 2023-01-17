//
// Copyright (c) 2009, Tweak Software
// All rights reserved.
// 
// SPDX-License-Identifier: Apache-2.0 
//


#include "gtoWriter.h"
#include <sstream>

#include <sstream>
#include <assert.h>
namespace PyGto {
using namespace std;

// *****************************************************************************
// We start with a few utility functions...
// *****************************************************************************

// *****************************************************************************
// Flatten a tuple or list into a C array of any type using the converter
// supplied.  'start' is used internally for recursive purposes.  Returns
// the number of items in the C array.
template <typename T>
int flatten(PyObject *object, T *data, int maxItems,
            const char *expectedTypeStr, T (*converter)(PyObject *),
            bool start = true) {
  static int pos;
  if (start) {
    pos = 0;
  }
  if (pos > maxItems) {
    return pos;
  }

  // If we come across a class instance, do we know what to do with it?
  if (isInstance(object)) {
    string classname(PyTypeName(object));

    // If we know what it is, convert it to something useful
    if (classname == "mat3" || classname == "mat4") {
      // mat3 and mat4 convert easily to a list
      PyObject *tmp = PyObject_GetAttrString(object, "mlist");
      Py_XINCREF(tmp);
      object = tmp;

    } else if (classname == "vec3" || classname == "vec4") {
      // vec3 and vec4 have no handy .toList() method, so we have
      // to do it the 'hard way'...
      PyObject *tmp;
      classname == "vec3" ? tmp = PyTuple_New(3) : tmp = PyTuple_New(4);

      PyObject *x = PyObject_GetAttrString(object, "x");
      Py_XINCREF(x);
      PyObject *y = PyObject_GetAttrString(object, "y");
      Py_XINCREF(y);
      PyObject *z = PyObject_GetAttrString(object, "z");
      Py_XINCREF(z);
      PyTuple_SetItem(tmp, 0, x);
      PyTuple_SetItem(tmp, 1, y);
      PyTuple_SetItem(tmp, 2, z);
      if (classname == "vec4") {
        PyObject *w = PyObject_GetAttrString(object, "w");
        Py_XINCREF(w);
        PyTuple_SetItem(tmp, 3, w);
      }
      object = tmp;
    } else if (classname == "quat") {
      // quat has no handy .toList() method either...
      PyObject *tmp = PyTuple_New(4);

      PyObject *w = PyObject_GetAttrString(object, "w");
      Py_XINCREF(w);
      PyObject *x = PyObject_GetAttrString(object, "x");
      Py_XINCREF(x);
      PyObject *y = PyObject_GetAttrString(object, "y");
      Py_XINCREF(y);
      PyObject *z = PyObject_GetAttrString(object, "z");
      Py_XINCREF(z);
      PyTuple_SetItem(tmp, 0, w);
      PyTuple_SetItem(tmp, 1, x);
      PyTuple_SetItem(tmp, 2, y);
      PyTuple_SetItem(tmp, 3, z);
      object = tmp;
    } else {
      // Otherwise, barf on it
      PyErr_Format(gtoError(),
                   "Can't handle '%s' class data directly."
                   "  Convert it to a tuple or list first.",
                   classname.c_str());
      return -1;
    }
  }

  // Put atoms directly into the buffer, and recurse on more complex types
  for (int i = 0; i < PySequence_Size(object); ++i) {
    PyObject *item = PySequence_GetItem(object, i);
    if (PyTuple_Check(item) || PyList_Check(item) || isInstance(item)) {
      flatten(item, data, maxItems, expectedTypeStr, converter, false);
    } else {
      // Add the atom to the buffer and move on
      data[pos] = converter(item);
      if (PyErr_Occurred()) {
        if (!PyErr_ExceptionMatches(PyExc_TypeError)) {
          // This is something other than a type error, so
          // this will cause a Python traceback later...
          return -1;
        }
        // Data of a type not handled by the converter
        PyErr_Format(gtoError(),
                     "Expected data of type '%s', but "
                     "got '%s'",
                     expectedTypeStr, PyTypeName(item));
        return -1;
      }
      pos++;
      if (pos > maxItems) {
        return pos;
      }
    }
  }

  return pos;
}

// *****************************************************************************
// The next several functions implement the methods on the Python gto.Writer
// class.
// *****************************************************************************

// *****************************************************************************
// gto.Writer.__init__. Does nothing, but is required anyway
  int gtoWriter_init(PyObject *self, PyObject *args, PyObject *kwds) {
    return 0;
  }

// *****************************************************************************
// Properly deallocate the instance-specific stuff-holder object
  void gtoWriter_PyObject_dealloc(PyObject *self) {
    assert(self != NULL);
    gtoWriter_PyObject *gwSelf = (gtoWriter_PyObject *)self;
    delete gwSelf->m_writer;
    delete gwSelf->m_propertyNames;

    Py_TYPE(self)->tp_free(self);
  }

// *****************************************************************************
// Implements gto.Writer.open( filename )
PyObject *gtoWriter_open(PyObject *self, PyObject *args) {
  char *filename;
  Gto::Writer::FileType filemode = Gto::Writer::CompressedGTO;

  if (!PyArg_ParseTuple(args, "s|i:gtoWriter_open", &filename, &filemode)) {
    // Invalid parameters, let Python do a stack trace
    return NULL;
  }

  gtoWriter_PyObject *writer = (gtoWriter_PyObject *)self;

  writer->m_writer = new Gto::Writer();
  writer->m_propCount = 0;
  writer->m_beginDataCalled = false;
  writer->m_objectDef = false;
  writer->m_componentDef = false;
  writer->m_propertyNames = new vector<string>;

  // Ask the writer to open the given file
  if (!writer->m_writer->open(filename, filemode)) {
    PyErr_Format(gtoError(), "Unable to open specified file: %s", filename);
    return NULL;
  }

  Py_XINCREF(Py_None);
  return Py_None;
}

// *****************************************************************************
// Implements gto.Writer.close()
PyObject *gtoWriter_close(PyObject *self) {
  // Get a handle to our Gto::Writer instance
  gtoWriter_PyObject *writer = (gtoWriter_PyObject *)self;

  if (writer->m_writer == NULL) {
    PyErr_SetString(gtoError(), "no file is open.");
    return NULL;
  }

  // Close the file
  writer->m_writer->close();
  writer->m_writer = nullptr;

  Py_XINCREF(Py_None);
  return Py_None;
}

// *****************************************************************************
// Implements gto.Writer.beginObject( name, protocol, protocolVersion )
PyObject *gtoWriter_beginObject(PyObject *self, PyObject *args) {
  char *name;
  char *protocol;
  unsigned int protocolVersion;

  if (!PyArg_ParseTuple(args, "ssi:gtoWriter_beginObject", &name, &protocol,
                        &protocolVersion)) {
    // Invalid parameters, let Python do a stack trace
    return NULL;
  }

  // Get a handle to our Gto::Writer instance
  gtoWriter_PyObject *writer = (gtoWriter_PyObject *)self;

  if (writer->m_writer == NULL) {
    PyErr_SetString(gtoError(), "no file is open.");
    return NULL;
  }

  // Check for dumbassness
  if (writer->m_objectDef == true) {
    PyErr_SetString(gtoError(), "Can't nest object declarations");
    return NULL;
  }

  if (writer->m_beginDataCalled == true) {
    PyErr_SetString(gtoError(), "Once beginData is called, no new "
                                "objects can be declared");
    return NULL;
  }

  // Make it so
  writer->m_writer->beginObject(name, protocol, protocolVersion);
  writer->m_objectDef = true;

  Py_XINCREF(Py_None);
  return Py_None;
}

// *****************************************************************************
// Implements gto.Writer.endObject()
PyObject *gtoWriter_endObject(PyObject *self) {
  // Get a handle to our Gto::Writer instance
  gtoWriter_PyObject *writer = (gtoWriter_PyObject *)self;

  if (writer->m_writer == NULL) {
    PyErr_SetString(gtoError(), "no file is open.");
    return NULL;
  }

  // Check for dumbassness
  if (writer->m_objectDef == false) {
    PyErr_SetString(gtoError(), "endObject called before beginObject");
    return NULL;
  }

  // Make it so
  writer->m_writer->endObject();
  writer->m_objectDef = false;

  Py_XINCREF(Py_None);
  return Py_None;
}

// *****************************************************************************
// Implements gto.Writer.beginComponent( name, interp, flags )
PyObject *gtoWriter_beginComponent(PyObject *self, PyObject *args) {
  char *name;
  char *interp = "";
  int flags = 0;

  // Try GTOv2 prototype first...
  if (!PyArg_ParseTuple(args, "s|i:gtoWriter_beginComponent", &name, &flags)) {
    PyErr_Clear();
    // If that doesn't work, try the GTOv3 prototype
    if (!PyArg_ParseTuple(args, "ss|i:gtoWriter_beginComponent", &name, &interp,
                          &flags)) {
      // Invalid parameters, let Python do a stack trace
      return NULL;
    }
  }

  // Get a handle to our Gto::Writer instance
  gtoWriter_PyObject *writer = (gtoWriter_PyObject *)self;

  if (writer->m_writer == NULL) {
    PyErr_SetString(gtoError(), "no file is open.");
    return NULL;
  }

  // Check for dumbassness
  if (writer->m_objectDef == false) {
    PyErr_SetString(gtoError(), "Components can only exist inside object "
                                "blocks");
    return NULL;
  }
  if (writer->m_componentDef == true) {
    PyErr_SetString(gtoError(), "Can't nest component declarations");
    return NULL;
  }

  // Make it so
  writer->m_writer->beginComponent(name, interp, flags);
  writer->m_componentDef = true;

  Py_XINCREF(Py_None);
  return Py_None;
}

// *****************************************************************************
// Implements gto.Writer.endComponent()
PyObject *gtoWriter_endComponent(PyObject *self) {
  // Get a handle to our Gto::Writer instance
  gtoWriter_PyObject *writer = (gtoWriter_PyObject *)self;

  if (writer->m_writer == NULL) {
    PyErr_SetString(gtoError(), "no file is open.");
    return NULL;
  }

  // Check for dumbassness
  if (writer->m_componentDef == false) {
    PyErr_SetString(gtoError(), "endComponent called before "
                                "beginComponent");
    return NULL;
  }

  // Make it so
  writer->m_writer->endComponent();
  writer->m_componentDef = false;

  Py_XINCREF(Py_None);
  return Py_None;
}

// *****************************************************************************
// Implements gto.Writer.property( name, type, numElements, width, interp )
PyObject *gtoWriter_property(PyObject *self, PyObject *args) {
  char *name;
  int type;
  int numElements;
  int width = 1;
  char *interp = "";

  if (!PyArg_ParseTuple(args, "sii|is:gtoWriter_property", &name, &type,
                        &numElements, &width, &interp)) {
    // Invalid parameters, let Python do a stack trace
    return NULL;
  }

  // Get a handle to our Gto::Writer instance
  gtoWriter_PyObject *writer = (gtoWriter_PyObject *)self;

  if (writer->m_writer == NULL) {
    PyErr_SetString(gtoError(), "no file is open.");
    return NULL;
  }

  // Check for dumbassness
  if (writer->m_objectDef == false || writer->m_componentDef == false) {
    PyErr_SetString(gtoError(), "Properties can only exist inside "
                                "object/component blocks");
    return NULL;
  }

  // Store name for later dumbassness checking in propertyData()
  writer->m_propertyNames->push_back(name);

  // Make it so
  writer->m_writer->property(name, (Gto::DataType)type, numElements, width,
                             interp);

  Py_XINCREF(Py_None);
  return Py_None;
}

// *****************************************************************************
// implements gto.Writer.intern( string | tuple | list )
PyObject *gtoWriter_intern(PyObject *self, PyObject *data) {
  // Get a handle to our Gto::Writer instance
  gtoWriter_PyObject *writer = (gtoWriter_PyObject *)self;
  if (writer->m_writer == NULL) {
    PyErr_SetString(gtoError(), "no file is open.");
    return NULL;
  }

  // Handle a single string
  if (PyBytes_Check(data)) {
    char *str = PyBytes_AsString(data);
    writer->m_writer->intern(str);
  }
  // Handle a bunch of strings all at once
  else if (PySequence_Check(data)) {
    for (int i = 0; i < PySequence_Size(data); ++i) {
      PyObject *pstr = PySequence_GetItem(data, i);
      if (PyBytes_Check(pstr)) {
        char *str = PyBytes_AsString(pstr);
        writer->m_writer->intern(str);
      } else if (PySequence_Check(pstr)) {
        for (int j = 0; j < PySequence_Size(pstr); ++j) {
          PyObject *ppstr = PySequence_GetItem(pstr, j);
          if (!PyBytes_Check(ppstr)) {
            PyErr_SetString(gtoError(), "Non-string in sub-sequence");
            return NULL;
          }
          char *str = PyBytes_AsString(ppstr);
          writer->m_writer->intern(str);
        }
      } else {
        PyErr_SetString(gtoError(), "Non-string or sequence in sequence");
        return NULL;
      }
    }
  }
  // We can't handle what we were given
  else {
    PyErr_SetString(gtoError(), "intern requires a string or a "
                                "sequence of strings");
    return NULL;
  }

  Py_XINCREF(Py_None);
  return Py_None;
}

// *****************************************************************************
// implements gto.Writer.lookup( string )
PyObject *gtoWriter_lookup(PyObject *self, PyObject *str) {
  // Get a handle to our Gto::Writer instance
  gtoWriter_PyObject *writer = (gtoWriter_PyObject *)self;
  if (writer->m_writer == NULL) {
    PyErr_SetString(gtoError(), "no file is open.");
    return NULL;
  }

  // Check for dumbassness
  if (writer->m_beginDataCalled == false) {
    PyErr_SetString(gtoError(), "lookup() cannot be used until "
                                "beginData() is called");
    return NULL;
  }

  // Make it so
  PyObject *strId_PyObj =
      PyLong_FromLong(writer->m_writer->lookup(PyBytes_AsString(str)));
  return strId_PyObj;
}

// *****************************************************************************
// implements gto.Writer.beginData()
PyObject *gtoWriter_beginData(PyObject *self) {
  // Get a handle to our Gto::Writer instance
  gtoWriter_PyObject *writer = (gtoWriter_PyObject *)self;

  if (writer->m_writer == NULL) {
    PyErr_SetString(gtoError(), "no file is open.");
    return NULL;
  }

  // Check for dumbassness
  if (writer->m_writer->properties().size() == 0) {
    PyErr_SetString(gtoError(), "There are no properties to write");
    return NULL;
  }

  // Make it so
  writer->m_writer->beginData();
  writer->m_beginDataCalled = true;

  Py_XINCREF(Py_None);
  return Py_None;
}

// *****************************************************************************
// implements gto.Writer.endData()
PyObject *gtoWriter_endData(PyObject *self) {
  // Get a handle to our Gto::Writer instance
  gtoWriter_PyObject *writer = (gtoWriter_PyObject *)self;
  if (writer->m_writer == NULL) {
    PyErr_SetString(gtoError(), "no file is open.");
    return NULL;
  }

  // Check for dumbassness
  if (writer->m_beginDataCalled == false) {
    PyErr_SetString(gtoError(), "endData called before beginData");
    return NULL;
  }

  // Make it so
  writer->m_writer->endData();

  Py_XINCREF(Py_None);
  return Py_None;
}

// *****************************************************************************
// implements gto.Writer.propertyData( data )
PyObject *gtoWriter_propertyData(PyObject *self, PyObject *rawdata) {
  // Get a handle to our Gto::Writer instance
  gtoWriter_PyObject *writer = (gtoWriter_PyObject *)self;
  if (writer->m_writer == NULL) {
    PyErr_SetString(gtoError(), "no file is open.");
    return NULL;
  }

  // Check for dumbassness
  if (!writer->m_beginDataCalled) {
    PyErr_SetString(gtoError(), "propertyData called before beginData");
    return NULL;
  }

  // If we're handed a single value, tuple-ize it for the code below
  if (PyLong_Check(rawdata) || PyFloat_Check(rawdata) ||
      PyBytes_Check(rawdata)) {
    PyObject *tmp = PyTuple_New(1);
    PyTuple_SetItem(tmp, 0, rawdata);
    Py_XDECREF(rawdata);
    rawdata = tmp;
  }

  // Get a handle to the property definition for the current property
  // and do some sanity checking
  Gto::PropertyHeader prop;
  prop = writer->m_writer->properties()[writer->m_propCount];
  if (writer->m_propCount >= writer->m_writer->properties().size()) {
    PyErr_SetString(gtoError(), "Undeclared data.");
    return NULL;
  }

  const char *currentPropName =
      (*writer->m_propertyNames)[writer->m_propCount].c_str();

  // Determine how many elements we have in the data
  int dataSize = prop.size * elementSize(prop.dims);

  // Write that data!
  if (prop.type == Gto::Int) {
    int *data = new int[dataSize];
    int numItems = flatten(rawdata, data, dataSize, "int", PyInt_AsInt);
    if (PyErr_Occurred()) {
      return NULL;
    }
    if (numItems != dataSize) {
      PyErr_Format(gtoError(),
                   "Property '%s' was declared as having %d"
                   " x %d values, but %d values were given for writing",
                   currentPropName, prop.size, int(elementSize(prop.dims)),
                   numItems);
      return NULL;
    }
    writer->m_writer->propertyData(data);
    writer->m_propCount++;

    delete[] data;

    Py_XINCREF(Py_None);
    return Py_None;
  }

  if (prop.type == Gto::Float) {
    float *data = new float[dataSize];
    int numItems = flatten(rawdata, data, dataSize, "float", PyFloat_AsFloat);
    if (PyErr_Occurred()) {
      return NULL;
    }
    if (numItems != dataSize) {
      PyErr_Format(gtoError(),
                   "Property '%s' was declared as having %d"
                   " x %d values, but %d values were given for writing",
                   currentPropName, prop.size, int(elementSize(prop.dims)),
                   numItems);
      return NULL;
    }
    writer->m_writer->propertyData(data);
    writer->m_propCount++;

    delete[] data;

    Py_XINCREF(Py_None);
    return Py_None;
  }
  if (prop.type == Gto::Double) {
    double *data = new double[dataSize];
    int numItems = flatten(rawdata, data, dataSize, "double", PyFloat_AsDouble);
    if (PyErr_Occurred()) {
      return NULL;
    }
    if (numItems != dataSize) {
      PyErr_Format(gtoError(),
                   "Property '%s' was declared as having %d"
                   " x %d values, but %d values were given for writing",
                   currentPropName, prop.size, int(elementSize(prop.dims)),
                   numItems);
      return NULL;
    }
    writer->m_writer->propertyData(data);
    writer->m_propCount++;

    delete[] data;

    Py_XINCREF(Py_None);
    return Py_None;
  }
  if (prop.type == Gto::Short) {
    unsigned short *data = new unsigned short[dataSize];
    int numItems = flatten(rawdata, data, dataSize, "short", PyInt_AsShort);
    if (PyErr_Occurred()) {
      return NULL;
    }
    if (numItems != dataSize) {
      PyErr_Format(gtoError(),
                   "Property '%s' was declared as having %d"
                   " x %d values, but %d values were given for writing",
                   currentPropName, prop.size, int(elementSize(prop.dims)),
                   numItems);
      return NULL;
    }
    writer->m_writer->propertyData(data);
    writer->m_propCount++;

    delete[] data;

    Py_XINCREF(Py_None);
    return Py_None;
  }
  if (prop.type == Gto::Byte) {
    unsigned char *data = new unsigned char[dataSize];
    int numItems = flatten(rawdata, data, dataSize, "byte", PyInt_AsByte);
    if (PyErr_Occurred()) {
      return NULL;
    }
    if (numItems != dataSize) {
      PyErr_Format(gtoError(),
                   "Property '%s' was declared as having %d"
                   " x %d values, but %d values were given for writing",
                   currentPropName, prop.size, int(elementSize(prop.dims)),
                   numItems);
      return NULL;
    }
    writer->m_writer->propertyData(data);
    writer->m_propCount++;

    delete[] data;

    Py_XINCREF(Py_None);
    return Py_None;
  }
  if (prop.type == Gto::String) {
    char **strings = new char *[dataSize];
    int numItems =
        flatten(rawdata, strings, dataSize, "string", PyBytes_AsString);
    if (PyErr_Occurred()) {
      return NULL;
    }
    if (numItems != dataSize) {
      PyErr_Format(gtoError(),
                   "Property '%s' was declared as having %d"
                   " x %d values, but %d values were given for writing",
                   currentPropName, prop.size, int(elementSize(prop.dims)),
                   numItems);
      return NULL;
    }

    int *data = new int[dataSize];
    for (int i = 0; i < numItems; ++i) {
      data[i] = writer->m_writer->lookup(strings[i]);
      if (data[i] == -1) {
        PyErr_Format(gtoError(),
                     "'%s' needs to be \"interned\" before it can "
                     "be used as data in property #%d",
                     strings[i], writer->m_propCount);
        return NULL;
      }
    }
    writer->m_writer->propertyData(data);
    writer->m_propCount++;

    delete[] strings;
    delete[] data;

    Py_XINCREF(Py_None);
    return Py_None;
  }

  PyErr_Format(gtoError(), "Undefined property type: %d  in property '%s'",
               prop.type, currentPropName);
  return NULL;
}

}; // End namespace PyGto
