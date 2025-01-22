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

#include "gtoHeader.h"

namespace PyGto
{
    using namespace std;

    PyObject* type_call(PyObject* self, PyObject* args, PyObject* kwds)
    {
        PyObject* obj;

        obj = Py_TYPE(self)->tp_new(Py_TYPE(self), args, kwds);

        if (Py_TYPE(self)->tp_init(obj, args, kwds) < 0)
        {
            Py_XDECREF(obj);
            obj = NULL;
        }

        return obj;
    }

    // *****************************************************************************
    //          ObjectInfo Class
    // *****************************************************************************

    // *****************************************************************************
    //
    PyObject* ObjectInfo_new(PyTypeObject* type, PyObject* args, PyObject* kwds)
    {
        ObjectInfo_PyObject* self =
            (ObjectInfo_PyObject*)type->tp_alloc(type, 0);

        if (self != NULL)
        {
            self->name = PyBytes_FromString("");
            self->protocolName = PyBytes_FromString("");
            self->protocolVersion = 0;
            self->numComponents = 0;
            self->pad = 0;
            self->oi = nullptr;
        }

        return (PyObject*)self;
    }

    // *****************************************************************************
    //
    void ObjectInfo_dealloc(PyObject* _self)
    {
        ObjectInfo_PyObject* self = (ObjectInfo_PyObject*)_self;
        Py_XDECREF(self->name);
        Py_XDECREF(self->protocolName);

        ((PyTypeObject*)Py_TYPE(self))->tp_free((PyObject*)self);
    }

    // *****************************************************************************
    // Implements ObjectInfo.__repr__(self)
    PyObject* ObjectInfo_repr(PyObject* self)
    {
        PyObject* name = PyObject_GetAttrString(self, "name");
        if (name == NULL)
        {
            return PyUnicode_FromFormat("<INVALID ObjectInfo object>");
        }
        return PyUnicode_FromFormat("<ObjectInfo: '%s'>",
                                    PyBytes_AsString(name));
    }

    // *****************************************************************************
    PyObject* newObjectInfo(Gto::Reader* reader,
                            const Gto::Reader::ObjectInfo& oi)
    {
        PyObject* module = PyImport_AddModule("gto");
        PyObject* moduleDict = PyModule_GetDict(module);
        PyObject* classObj = PyDict_GetItemString(moduleDict, "ObjectInfo");

        PyObject* args = Py_BuildValue("()");
        PyObject* objInfo = PyObject_Call(classObj, args, NULL);
        Py_XDECREF(args);

        PyObject* t;
        t = PyBytes_FromString(reader->stringFromId(oi.name).c_str());
        PyObject_SetAttrString(objInfo, "name", t);
        Py_XDECREF(t);

        t = PyBytes_FromString(reader->stringFromId(oi.protocolName).c_str());
        PyObject_SetAttrString(objInfo, "protocolName", t);
        Py_XDECREF(t);

        t = PyLong_FromLong(oi.protocolVersion);
        PyObject_SetAttrString(objInfo, "protocolVersion", t);
        Py_XDECREF(t);

        t = PyLong_FromLong(oi.numComponents);
        PyObject_SetAttrString(objInfo, "numComponents", t);
        Py_XDECREF(t);

        t = PyLong_FromLong(oi.pad);
        PyObject_SetAttrString(objInfo, "pad", t);
        Py_XDECREF(t);

        ((ObjectInfo_PyObject*)objInfo)->oi = &oi;

        return objInfo;
    }

    // *****************************************************************************
    //          ComponentInfo Class
    // *****************************************************************************

    // *****************************************************************************
    //
    PyObject* ComponentInfo_new(PyTypeObject* type, PyObject* args,
                                PyObject* kwds)
    {
        ComponentInfo_PyObject* self =
            (ComponentInfo_PyObject*)type->tp_alloc(type, 0);

        if (self != NULL)
        {
            self->name = PyBytes_FromString("");
            self->numProperties = 0;
            self->flags = 0;
            self->interpretation = PyBytes_FromString("");
            self->childLevel = 0;

            Py_XINCREF(Py_None);
            self->object = Py_None;
        }

        return (PyObject*)self;
    }

    // *****************************************************************************
    //
    void ComponentInfo_dealloc(PyObject* _self)
    {
        ComponentInfo_PyObject* self = (ComponentInfo_PyObject*)_self;
        Py_XDECREF(self->name);
        Py_XDECREF(self->interpretation);
        Py_XDECREF(self->object);

        ((PyTypeObject*)Py_TYPE(self))->tp_free((PyObject*)self);
    }

    // *****************************************************************************
    // Implements ComponentInfo.__repr__(self)
    PyObject* ComponentInfo_repr(PyObject* self)
    {
        PyObject* name = PyObject_GetAttrString(self, "name");
        if (name == NULL)
        {
            return PyUnicode_FromString("<INVALID ComponentInfo object>");
        }
        return PyUnicode_FromFormat("<ComponentInfo: '%s'>",
                                    PyBytes_AsString(name));
    }

    // *****************************************************************************
    PyObject* newComponentInfo(Gto::Reader* reader,
                               const Gto::Reader::ComponentInfo& ci)
    {
        PyObject* module = PyImport_AddModule("gto");
        PyObject* moduleDict = PyModule_GetDict(module);
        PyObject* classObj = PyDict_GetItemString(moduleDict, "ComponentInfo");

        PyObject* args = Py_BuildValue("()");
        PyObject* compInfo = PyObject_Call(classObj, args, NULL);
        Py_XDECREF(args);

        PyObject* t;
        t = PyBytes_FromString(reader->stringFromId(ci.name).c_str());
        PyObject_SetAttrString(compInfo, "name", t);
        Py_XDECREF(t);

        t = PyLong_FromLong(ci.numProperties);
        PyObject_SetAttrString(compInfo, "numProperties", t);
        Py_XDECREF(t);

        t = PyLong_FromLong(ci.flags);
        PyObject_SetAttrString(compInfo, "flags", t);
        Py_XDECREF(t);

        t = PyBytes_FromString(reader->stringFromId(ci.interpretation).c_str());
        PyObject_SetAttrString(compInfo, "interpretation", t);
        Py_XDECREF(t);

        t = PyLong_FromLong(ci.childLevel);
        PyObject_SetAttrString(compInfo, "childLevel", t);
        Py_XDECREF(t);

        t = newObjectInfo(reader, (*ci.object));
        PyObject_SetAttrString(compInfo, "object", t);
        Py_XDECREF(t);

        return compInfo;
    }

    // *****************************************************************************
    //          PropertyInfo Class
    // *****************************************************************************

    // *****************************************************************************
    //
    PyObject* PropertyInfo_new(PyTypeObject* type, PyObject* args,
                               PyObject* kwds)
    {
        PropertyInfo_PyObject* self =
            (PropertyInfo_PyObject*)type->tp_alloc(type, 0);

        if (self != NULL)
        {
            self->name = PyBytes_FromString("");
            self->size = 0;
            self->type = 0;
            self->width = 0;

            self->dimensions = PyTuple_New(4);
            self->interpretation = PyBytes_FromString("");
            self->pad = 0;

            Py_XINCREF(Py_None);
            self->component = Py_None;
        }

        return (PyObject*)self;
    }

    // *****************************************************************************
    //
    void PropertyInfo_dealloc(PyObject* _self)
    {
        PropertyInfo_PyObject* self = (PropertyInfo_PyObject*)_self;
        Py_XDECREF(self->name);
        Py_XDECREF(self->dimensions);
        Py_XDECREF(self->interpretation);
        Py_XDECREF(self->component);

        ((PyTypeObject*)Py_TYPE(self))->tp_free((PyObject*)self);
    }

    // *****************************************************************************
    // Implements PropertyInfo.__repr__(self)
    PyObject* PropertyInfo_repr(PyObject* self)
    {
        PyObject* name = PyObject_GetAttrString(self, "name");
        if (name == NULL)
        {
            return PyUnicode_FromString("<INVALID PropertyInfo object>");
        }
        return PyUnicode_FromFormat("<PropertyInfo: '%s'>",
                                    PyBytes_AsString(name));
    }

    // *****************************************************************************
    PyObject* newPropertyInfo(Gto::Reader* reader,
                              const Gto::Reader::PropertyInfo& pi)
    {
        PyObject* module = PyImport_AddModule("gto");
        PyObject* moduleDict = PyModule_GetDict(module);
        PyObject* classObj = PyDict_GetItemString(moduleDict, "PropertyInfo");

        PyObject* args = Py_BuildValue("()");
        PyObject* propInfo = PyObject_Call(classObj, args, NULL);
        Py_XDECREF(args);

        PyObject* t;
        t = PyBytes_FromString(reader->stringFromId(pi.name).c_str());
        PyObject_SetAttrString(propInfo, "name", t);
        Py_XDECREF(t);

        t = PyLong_FromLong(pi.size);
        PyObject_SetAttrString(propInfo, "size", t);
        Py_XDECREF(t);

        t = PyLong_FromLong(pi.type);
        PyObject_SetAttrString(propInfo, "type", t);
        Py_XDECREF(t);

        t = PyLong_FromLong(pi.dims.x);
        PyObject_SetAttrString(propInfo, "width", t);
        Py_XDECREF(t);

        t = Py_BuildValue("(llll)", pi.dims.x, pi.dims.y, pi.dims.z, pi.dims.w);
        PyObject_SetAttrString(propInfo, "dimensions", t);
        Py_XDECREF(t);

        t = PyBytes_FromString(reader->stringFromId(pi.interpretation).c_str());
        PyObject_SetAttrString(propInfo, "interpretation", t);
        Py_XDECREF(t);

        t = PyLong_FromLong(0);
        PyObject_SetAttrString(propInfo, "pad", t);
        Py_XDECREF(t);

        t = newComponentInfo(reader, (*pi.component));
        PyObject_SetAttrString(propInfo, "component", t);
        Py_XDECREF(t);

        return propInfo;
    }

} //  End namespace PyGto
