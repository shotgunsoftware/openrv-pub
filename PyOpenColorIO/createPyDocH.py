#!/usr/bin/env python

"""
This script contains mock OpenColorIO classes which define the __doc__
strings that will end up in the final binding. These __doc__ strings are
also used by sphinx's autodoc extension to also include this documentation
in the html and pdf formats.
"""

import re, sys

from DocStrings import *


def DocStringToCString(name, doc_string):
    _cstr = doc_string
    _cstr = _cstr.rstrip(" ")
    _cstr = _cstr.rstrip("\n")
    _cstr = _cstr.lstrip("\n")
    _cstr = _cstr.lstrip(" ")
    _cstr = _cstr.replace('"', '\\"')
    _cstr = _cstr.replace("\n", "\\n")
    _cstr = _cstr.replace("\r", "\\n")
    return 'const char %s[%d] = "%s";' % (name, len(_cstr) + 1, _cstr)


def GetDocStrings(inst):
    _out = ""
    _cname = inst.__name__
    _cdoc = inst.__doc__
    if _cdoc == None:
        _cdoc = ""
    _out = "%s\n" % DocStringToCString("%s__DOC__" % _cname.upper(), _cdoc)
    for mem in dir(inst):
        if mem[0:2] == "__":
            continue  # skip
        _doc = eval("inst.%s.__doc__" % mem)
        if _doc == None:
            _doc = ""
        _name = "%s_%s__DOC__" % (
            _cname.upper(),
            eval("inst.%s.__name__" % mem).upper(),
        )
        _out += "%s\n" % DocStringToCString(_name, _doc)
        # print mem
    return _out


if __name__ == "__main__":

    if len(sys.argv) <= 1:
        sys.stderr.write("\nYou need to specify an output file\n\n")
        sys.exit(1)

    fileh = open(sys.argv[1], "w")
    fileh.write("\n")
    fileh.write("/* DO NOT EDIT THIS FILE - it is machine generated */\n")
    fileh.write("\n")
    fileh.write("#include <OpenColorIO/OpenColorIO.h>\n")
    fileh.write("\n")
    fileh.write("OCIO_NAMESPACE_ENTER\n")
    fileh.write("{\n")
    fileh.write("\n")
    fileh.write("%s\n" % GetDocStrings(Exception))
    fileh.write("%s\n" % GetDocStrings(ExceptionMissingFile))
    fileh.write("%s\n" % GetDocStrings(OpenColorIO))
    fileh.write("%s\n" % GetDocStrings(Constants))
    fileh.write("%s\n" % GetDocStrings(Config))
    fileh.write("%s\n" % GetDocStrings(ColorSpace))
    fileh.write("%s\n" % GetDocStrings(Processor))
    fileh.write("%s\n" % GetDocStrings(ProcessorMetadata))
    fileh.write("%s\n" % GetDocStrings(Context))
    fileh.write("%s\n" % GetDocStrings(Look))
    fileh.write("%s\n" % GetDocStrings(GpuShaderDesc))
    fileh.write("%s\n" % GetDocStrings(Baker))
    fileh.write("%s\n" % GetDocStrings(Transform))
    fileh.write("\n")
    fileh.write("%s\n" % GetDocStrings(AllocationTransform))
    fileh.write("%s\n" % GetDocStrings(CDLTransform))
    fileh.write("%s\n" % GetDocStrings(ColorSpaceTransform))
    fileh.write("%s\n" % GetDocStrings(DisplayTransform))
    fileh.write("%s\n" % GetDocStrings(ExponentTransform))
    fileh.write("%s\n" % GetDocStrings(FileTransform))
    fileh.write("%s\n" % GetDocStrings(GroupTransform))
    fileh.write("%s\n" % GetDocStrings(LogTransform))
    fileh.write("%s\n" % GetDocStrings(LookTransform))
    fileh.write("%s\n" % GetDocStrings(MatrixTransform))
    fileh.write("\n")
    fileh.write("}\n")
    fileh.write("OCIO_NAMESPACE_EXIT\n")
    fileh.write("\n")
    fileh.flush()
    fileh.close()
