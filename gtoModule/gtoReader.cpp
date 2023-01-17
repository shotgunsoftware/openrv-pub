//
//  Copyright (c) 2009, Tweak Software
//  All rights reserved.
// 
//  Redistribution and use in source and binary forms, with or without
//  modification, are permitted provided that the following conditions
//  are met:
//
//     * Redistributions of source code must retain the above
//       copyright notice, this list of conditions and the following
//       disclaimer.
//
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials
//       provided with the distribution.
//
//     * Neither the name of the Tweak Software nor the names of its
//       contributors may be used to endorse or promote products
//       derived from this software without specific prior written
//       permission.
// 
//  THIS SOFTWARE IS PROVIDED BY Tweak Software ''AS IS'' AND ANY EXPRESS
//  OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
//  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
//  ARE DISCLAIMED. IN NO EVENT SHALL Tweak Software BE LIABLE FOR
//  ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
//  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
//  OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
//  BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
//  LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
//  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
//  USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
//  DAMAGE.
//

#include "gtoReader.h"
#include "gtoHeader.h"

#include <Python.h>

#include <vector>
#include <assert.h>

namespace PyGto
{
  using namespace std;

  // *****************************************************************************
  // We start with a few utility functions...
  // *****************************************************************************

  // *****************************************************************************
  // Returns the C++ Gto::Reader instance if a file is open, otherwise NULL
  Reader *readerIfOpen(PyObject *self)
  {
    assert(self != NULL);
    gtoReader_PyObject *reader = (gtoReader_PyObject *)self;

    if (!reader)
    {
      PyErr_SetString(gtoError(), "Fatal internal error: no readerEngine!");
      return NULL;
    }

    if (!reader->m_reader)
    {
      PyErr_SetString(gtoError(), "Fatal internal error: "
                                  "no Gto::Reader instance!");
      return NULL;
    }

    if (!reader->m_isOpen)
    {
      PyErr_SetString(gtoError(), "no file is open.");
      return NULL;
    }

    return reader->m_reader;
  }

  // *****************************************************************************
  // Converts from gto's enum-based types to their string representation
  static const char *typeAsString(int type)
  {
    switch (type)
    {
    case Gto::Int:
      return "int";
    case Gto::Float:
      return "float";
    case Gto::Double:
      return "double";
    case Gto::Half:
      return "half";
    case Gto::String:
      return "string";
    case Gto::Boolean:
      return "bool";
    case Gto::Short:
      return "short";
    case Gto::Byte:
      return "byte";
    }
    return "unknown";
  }

  // *****************************************************************************
  // The next several functions implement the methods on our derived C++
  // Gto::Reader class.  They get called by Gto::Reader::open(), and their main
  // purpose is to convert their arguments into Python objects and then call
  // their equivalent methods on the Python gto.Reader class.
  // *****************************************************************************

  // *****************************************************************************
  Reader::Reader(PyObject *callingInstance, unsigned int mode)
      : m_callingInstance(callingInstance), Gto::Reader(mode)
  {
    // Nothing
  }

  // *****************************************************************************
  Request Reader::object(const std::string &name, const std::string &protocol,
                         unsigned int protocolVersion,
                         const Gto::Reader::ObjectInfo &info)
  {
    assert(m_callingInstance != NULL);

    // Build the Python equivalent of the Gto::ObjectInfo struct
    PyObject *oi = newObjectInfo(this, info);

    PyObject *returnValue = NULL;
    returnValue =
        PyObject_CallMethod(m_callingInstance, "object", "ssiO", name.c_str(),
                            protocol.c_str(), protocolVersion, oi);

    Py_XDECREF(oi);

    if (returnValue == NULL)
    {
      // Invalid parameters, stop the reader and let Python do a stack trace
      fail();
      return Request(false);
    }

    if (PyObject_IsTrue(returnValue))
    {
      return Request(true, (void *)returnValue);
    }
    return Request(false);
  }

  // *****************************************************************************
  Request Reader::component(const std::string &name, const std::string &interp,
                            const Gto::Reader::ComponentInfo &info)
  {
    assert(m_callingInstance != NULL);

    // Build the Python equivalent of the Gto::ComponentInfo struct
    PyObject *ci = newComponentInfo(this, info);

    PyObject *returnValue = NULL;
    // Try calling the component method
    returnValue = PyObject_CallMethod(m_callingInstance, "component", "ssO",
                                      name.c_str(), interp.c_str(), ci);

    Py_XDECREF(ci);

    if (returnValue == NULL)
    {
      // Invalid parameters, stop the reader and let Python do a stack trace
      fail();
      return Request(false);
    }

    if (PyObject_IsTrue(returnValue))
    {
      return Request(true, (void *)returnValue);
    }
    return Request(false);
  }

  // *****************************************************************************
  Request Reader::property(const std::string &name, const std::string &interp,
                           const Gto::Reader::PropertyInfo &info)
  {
    assert(m_callingInstance != NULL);

    // Build the Python equivalent of the Gto::PropertyInfo struct
    PyObject *pi = newPropertyInfo(this, info);

    PyObject *returnValue = NULL;
    // Try calling the property method with the propInfo tuple
    returnValue = PyObject_CallMethod(m_callingInstance, "property", "ssO",
                                      name.c_str(), interp.c_str(), pi);

    Py_XDECREF(pi);

    if (returnValue == NULL)
    {
      // Invalid parameters, stop the reader and let Python do a stack trace
      fail();
      return Request(false);
    }

    if (PyObject_IsTrue(returnValue))
    {
      return Request(true, (void *)returnValue);
    }
    return Request(false, NULL);
  }

  // *****************************************************************************
  // Note that this method does not call any overloaded Python method, it
  // is here simply to allocate space for Gto::Reader.
  void *Reader::data(const PropertyInfo &pinfo, size_t bytes)
  {
    assert(m_callingInstance != NULL);

    size_t esize = elementSize(pinfo.dims);
    size_t nelements = esize * pinfo.size;

    switch (pinfo.type)
    {
    case Gto::Int:
    case Gto::String:
      m_tmpIntData.resize(nelements);
      return (void *)&m_tmpIntData.front();
    case Gto::Float:
      m_tmpFloatData.resize(nelements);
      return (void *)&m_tmpFloatData.front();
    case Gto::Double:
      m_tmpDoubleData.resize(nelements);
      return (void *)&m_tmpDoubleData.front();
    case Gto::Byte:
      m_tmpCharData.resize(nelements);
      return (void *)&m_tmpCharData.front();
    case Gto::Short:
      m_tmpShortData.resize(nelements);
      return (void *)&m_tmpShortData.front();
    default:
      PyErr_Format(gtoError(), "Unsupported data type: %s",
                   typeAsString(pinfo.type));
      fail();
    }
    return NULL;
  }

  // *****************************************************************************
  void Reader::dataRead(const PropertyInfo &pinfo)
  {
    assert(m_callingInstance != NULL);

    // Build a tuple out of the data for this property
    PyObject *dataTuple = PyTuple_New(pinfo.size);
    for (size_t i = 0; i < pinfo.size; ++i)
    {
      if (pinfo.dims.x == 1 && pinfo.dims.y == 0)
      {
        switch (pinfo.type)
        {
        case Gto::String:
          PyTuple_SetItem(
              dataTuple, i,
              PyBytes_FromString(stringFromId(m_tmpIntData[i]).c_str()));
          break;
        case Gto::Int:
          PyTuple_SetItem(dataTuple, i, PyLong_FromLong(m_tmpIntData[i]));
          break;
        case Gto::Float:
          PyTuple_SetItem(dataTuple, i, PyFloat_FromDouble(m_tmpFloatData[i]));
          break;
        case Gto::Double:
          PyTuple_SetItem(dataTuple, i, PyFloat_FromDouble(m_tmpDoubleData[i]));
          break;
        case Gto::Byte:
          PyTuple_SetItem(dataTuple, i, PyLong_FromLong(m_tmpCharData[i]));
          break;
        case Gto::Short:
          PyTuple_SetItem(dataTuple, i, PyLong_FromLong(m_tmpShortData[i]));
          break;
        }
      }
      else
      {
        size_t esize = elementSize(pinfo.dims);
        PyObject *subTuple = PyTuple_New(esize);

        for (size_t w = 0; w < esize; w++)
        {
          size_t di = (i * esize) + w;
          switch (pinfo.type)
          {
          case Gto::String:
            PyTuple_SetItem(
                subTuple, w,
                PyBytes_FromString(stringFromId(m_tmpIntData[di]).c_str()));
            break;
          case Gto::Int:
            PyTuple_SetItem(subTuple, w, PyLong_FromLong(m_tmpIntData[di]));
            break;
          case Gto::Float:
            PyTuple_SetItem(subTuple, w, PyFloat_FromDouble(m_tmpFloatData[di]));
            break;
          case Gto::Double:
            PyTuple_SetItem(subTuple, w, PyFloat_FromDouble(m_tmpDoubleData[di]));
            break;
          case Gto::Byte:
            PyTuple_SetItem(subTuple, w, PyLong_FromLong(m_tmpCharData[di]));
            break;
          case Gto::Short:
            PyTuple_SetItem(subTuple, w, PyLong_FromLong(m_tmpShortData[di]));
            break;
          }
        }
        PyTuple_SetItem(dataTuple, i, subTuple);
      }
    }

    // Clear any temporary space we used
    m_tmpFloatData.clear();
    m_tmpDoubleData.clear();
    m_tmpIntData.clear();
    m_tmpShortData.clear();
    m_tmpCharData.clear();

    assert(dataTuple != NULL);

    // Build the Python equivalent of the Gto::PropertyInfo struct
    PyObject *pi = newPropertyInfo(this, pinfo);

    PyObject *returnValue = NULL;
    returnValue =
        PyObject_CallMethod(m_callingInstance, "dataRead", "sOO",
                            stringFromId(pinfo.name).c_str(), dataTuple, pi);

    Py_XDECREF(pi);
    Py_XDECREF(dataTuple);
    // The data method was not properly overridden...
    if (returnValue == NULL)
    {
      // Invalid parameters, stop the reader and let Python do a stack trace
      fail();
    }
    Py_XDECREF(returnValue);
  }

  // *****************************************************************************
  // Implements the gto.Reader( [mode] ) constructor
  int gtoReader_init(PyObject *self, PyObject *args, PyObject *kwds)
  {
    int mode = Gto::Reader::None;

    if (!PyArg_ParseTuple(args, "|i:gtoReader_init", &mode))
    {
      // Invalid parameters, let Python do a stack trace
      return -1;
    }

    // Create a new Python object to hold the pointer to the C++ reader
    // instance and add it to this Python instance's dictionary
    gtoReader_PyObject *reader = (gtoReader_PyObject *)self;

    reader->m_reader = new Reader((PyObject *)self, mode);
    if (!reader->m_reader)
    {
      PyErr_Format(gtoError(), "Unable to create instance of Gto::Reader.  "
                               "Bad parameters?");
      return -1;
    }

    reader->m_isOpen = false;
    return 0;
  }

  // *****************************************************************************
  // Properly deallocate the instance-specific stuff-holder object
  void gtoReader_PyObject_dealloc(PyObject *self)
  {
    assert(self != NULL);

    gtoReader_PyObject *grSelf = (gtoReader_PyObject *)self;
    if (grSelf->m_reader)
    {
      delete grSelf->m_reader;
      grSelf->m_reader = nullptr;
    }

    Py_TYPE(self)->tp_free(self);
  }

  // *****************************************************************************
  // Implements gto.Reader.open( filename )
  PyObject *gtoReader_open(PyObject *self, PyObject *filename)
  {
    if (!PyBytes_Check(filename))
    {
      // Invalid parameters, let Python do a stack trace
      PyErr_SetString(gtoError(), "TypeError: filename must be bytes");
      return NULL;
    }

    gtoReader_PyObject *reader = (gtoReader_PyObject *)self;

    if (reader == NULL)
    {
      PyErr_Format(
          gtoError(),
          "The open() method was called before the constructor.  If your\n"
          "           derived Reader class has an __init__ method, you need to\n"
          "           call gto.Reader.__init__() at the end of it.");
      return NULL;
    }

    // We set isOpen _before_ calling open, because it is the open method that
    // calls all our other crap.  If we don't do it now, we don't get another
    // chance until it's all over.
    reader->m_isOpen = true;
    if (!reader->m_reader->open(PyBytes_AsString(filename)))
    {
      // Something went wrong.  If the error was in the Python world,
      // the error message should already be set.  If not, there was a
      // problem in the C++ world, so set the error message now.
      if (!PyErr_Occurred())
      {
        PyErr_Format(gtoError(), "Unable to open %s: %s",
                     PyBytes_AsString(filename),
                                 reader->m_reader->why().c_str());
      }
      reader->m_isOpen = false;
      return NULL;
    }

    Py_XINCREF(Py_None);
    return Py_None;
  }

  // *****************************************************************************
  // Implements gto.Reader.fail( why )
  PyObject *gtoReader_fail(PyObject *self, PyObject *why)
  {
    if (!PyBytes_Check(why))
    {
      // Invalid parameters, let Python do a stack trace
      return NULL;
    }

    Reader *reader = readerIfOpen(self);
    if (reader == NULL)
    {
      return NULL;
    }

    reader->fail(PyBytes_AsString(why));

    Py_XINCREF(Py_None);
    return Py_None;
  }

  // *****************************************************************************
  // Implements gto.Reader.why()
  PyObject *gtoReader_why(PyObject *self)
  {
    Reader *reader = readerIfOpen(self);
    if (reader == NULL)
    {
      return NULL;
    }

    PyObject *errMsg = PyBytes_FromString(reader->why().c_str());

    return errMsg;
  }

  // *****************************************************************************
  // Implements gto.Reader.close()
  PyObject *gtoReader_close(PyObject *self)
  {
    gtoReader_PyObject *reader = (gtoReader_PyObject *)self;

    if (reader == NULL || reader->m_reader == NULL || !reader->m_isOpen)
    {
      PyErr_SetString(gtoError(), "no file is open.");
      return NULL;
    }

    reader->m_reader->close();
    reader->m_isOpen = false;

    Py_XINCREF(Py_None);
    return Py_None;
  }

  // *****************************************************************************
  // Implements a default
  //      gto.Reader.object( name, protocol, protocolVersion, ObjectInfo )
  PyObject *gtoReader_object(PyObject *self, PyObject *args)
  {
    char *name;
    char *protocol;
    int protocolVersion;
    PyObject *objInfo;

    if (!PyArg_ParseTuple(args, "ssiO:gtoReader_object", &name, &protocol,
                          &protocolVersion, &objInfo))
    {
      // Invalid parameters, let Python do a stack trace
      return NULL;
    }

#ifdef DEBUG
    cout << "object \"" << name << "\" protocol \"" << protocol << "\"";
    cout << endl;
#endif

    // Assume we want all the objects in the file
    return PyLong_FromLong(1);
  }

  // *****************************************************************************
  // Implements a default gto.Reader.component( name, interp, ComponentInfo )
  PyObject *gtoReader_component(PyObject *self, PyObject *args)
  {
    char *name;
    char *interp;
    PyObject *compInfo;

    if (!PyArg_ParseTuple(args, "ssO:gtoReader_component", &name, &interp,
                          &compInfo))
    {
      // Invalid parameters, let Python do a stack trace
      return NULL;
    }

#ifdef DEBUG
    cout << "\tcomponent \"" << name << "\"" << endl;
#endif

    // Assume we want all the components in the file
    return PyLong_FromLong(1);
  }

  // *****************************************************************************
  // Implements a default gto.Reader.property( name, interp, PropertyInfo )
  PyObject *gtoReader_property(PyObject *self, PyObject *args)
  {
    char *name;
    char *interp;
    PyObject *propInfo;

    if (!PyArg_ParseTuple(args, "ssO:gtoReader_property", &name, &interp,
                          &propInfo))
    {
      // Invalid parameters, let Python do a stack trace
      return NULL;
    }

#ifdef DEBUG
    cout << "\t\tproperty " << name << endl;
#endif

    // Assume we want all the properties in the file
    return PyLong_FromLong(1);
  }

  // *****************************************************************************
  // Implements a default gto.Reader.dataRead( propertyInfo )
  PyObject *gtoReader_dataRead(PyObject *self, PyObject *args)
  {
    char *name;
    PyObject *dataTuple;
    PyObject *propInfo;

    if (!PyArg_ParseTuple(args, "sOO:gtoReader_dataRead", &name, &dataTuple,
                          &propInfo))
    {
      // Invalid parameters, let Python do a stack trace
      return NULL;
    }

#ifdef DEBUG
    cout << "data " << name << endl;
#endif

    Py_XINCREF(Py_None);
    return Py_None;
  }

  // *****************************************************************************
  // Implements gto.Reader.stringFromId( int )
  PyObject *gtoReader_stringFromId(PyObject *self, PyObject *id)
  {
    if (!PyLong_Check(id))
    {
      // Invalid parameters, let Python do a stack trace
      return NULL;
    }

    Reader *reader = readerIfOpen(self);
    if (reader == NULL)
    {
      return NULL;
    }

    PyObject *str =
        PyBytes_FromString(reader->stringFromId(PyLong_AsLong(id)).c_str());
    return str;
  }

  // *****************************************************************************
  // Implements gto.Reader.stringTable()
  PyObject *gtoReader_stringTable(PyObject *self)
  {
    Reader *reader = readerIfOpen(self);
    if (reader == NULL)
    {
      return NULL;
    }

    Gto::Reader::StringTable stringTable = reader->stringTable();

    PyObject *stringTableTuple = PyTuple_New(stringTable.size());
    for (int i = 0; i < stringTable.size(); ++i)
    {
      PyTuple_SetItem(stringTableTuple, i,
                      PyBytes_FromString(stringTable[i].c_str()));
    }

    return stringTableTuple;
  }

  // *****************************************************************************
  // Implements gto.Reader.isSwapped()
  PyObject *gtoReader_isSwapped(PyObject *self)
  {
    Reader *reader = readerIfOpen(self);
    if (reader == NULL)
    {
      return NULL;
    }

    if (reader->isSwapped())
    {
      Py_XINCREF(Py_True);
      return Py_True;
    }

    Py_XINCREF(Py_False);
    return Py_False;
  }

  // *****************************************************************************
  // Implements gto.Reader.objects()
  PyObject *gtoReader_objects(PyObject *self)
  {
    Reader *reader = readerIfOpen(self);

    if (reader == NULL)
    {
      return NULL;
    }

    if (reader->readMode() != Gto::Reader::RandomAccess)
    {
      PyErr_SetString(gtoError(), "file was not opened for random access.");
      return NULL;
    }

    Gto::Reader::Objects &objects = reader->objects();

    PyObject *objectsTuple = PyTuple_New(objects.size());
    for (int i = 0; i < objects.size(); ++i)
    {
      PyObject *oi = newObjectInfo(reader, objects[i]);
      PyTuple_SetItem(objectsTuple, i, oi);
    }

    return objectsTuple;
  }

  // *****************************************************************************
  // Implements gto.Reader.accessObject()
  PyObject *gtoReader_accessObject(PyObject *self, PyObject *objInfo)
  {
    if (string(PyTypeName(objInfo)) != "ObjectInfo")
    {
      PyErr_SetString(gtoError(), "accessObject requires an ObjectInfo instance");
      return NULL;
    }

    Reader *reader = readerIfOpen(self);
    if (reader == NULL)
    {
      return NULL;
    }

    if (reader->readMode() != Gto::Reader::RandomAccess)
    {
      PyErr_SetString(gtoError(), "file was not opened for random access.");
      return NULL;
    }

    // Get the original C++ ObjectInfo reference from the Python ObjectInfo
    // class
    ObjectInfo_PyObject *objInfoPyObj = (ObjectInfo_PyObject *)objInfo;

    Gto::Reader::ObjectInfo &oi = *(Gto::Reader::ObjectInfo *)objInfoPyObj->oi;

    if (reader->accessObject(oi))
    {
      Py_XINCREF(Py_True);
      return Py_True;
    }

    Py_XINCREF(Py_False);
    return Py_False;
  }

  // *****************************************************************************
  // Implements gto.Reader.components()
  PyObject *gtoReader_components(PyObject *self)
  {
    Reader *reader = readerIfOpen(self);
    if (reader == NULL)
    {
      return NULL;
    }
    if (reader->readMode() != Gto::Reader::RandomAccess)
    {
      PyErr_SetString(gtoError(), "file was not opened for random access.");
      return NULL;
    }

    Gto::Reader::Components &components = reader->components();

    PyObject *componentsTuple = PyTuple_New(components.size());
    for (int i = 0; i < components.size(); ++i)
    {
      PyObject *ci = newComponentInfo(reader, components[i]);
      PyTuple_SetItem(componentsTuple, i, ci);
    }

    return componentsTuple;
  }

  // *****************************************************************************
  // Implements gto.Reader.properties()
  PyObject *gtoReader_properties(PyObject *self)
  {
    Reader *reader = readerIfOpen(self);
    if (reader == NULL)
    {
      return NULL;
    }
    if (reader->readMode() != Gto::Reader::RandomAccess)
    {
      PyErr_SetString(gtoError(), "file was not opened for random access.");
      return NULL;
    }

    Gto::Reader::Properties &properties = reader->properties();

    PyObject *propertiesTuple = PyTuple_New(properties.size());
    for (int i = 0; i < properties.size(); ++i)
    {
      PyObject *pi = newPropertyInfo(reader, properties[i]);
      PyTuple_SetItem(propertiesTuple, i, pi);
    }

    return propertiesTuple;
  }
} // End namespace PyGto
