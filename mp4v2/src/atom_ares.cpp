/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is MPEG4IP.
 *
 * The Initial Developer of the Original Code is Cisco Systems Inc.
 * Portions created by Cisco Systems Inc. are
 * Copyright (C) Cisco Systems Inc. 2004.  All Rights Reserved.
 *
 * Contributor(s):
 *      Bill May  wmay@cisco.com
 */

#include "src/impl.h"

namespace mp4v2 {
namespace impl {

///////////////////////////////////////////////////////////////////////////////

MP4ARESAtom::MP4ARESAtom (MP4File &file)
        : MP4Atom(file, "ARES")
{
    MP4StringProperty* tagProp =
        new MP4StringProperty(*this, "tag");
    tagProp->SetFixedLength(4);
    AddProperty(tagProp);

    MP4StringProperty* versionProp =
        new MP4StringProperty(*this, "version");
    versionProp->SetFixedLength(4);
    AddProperty(versionProp);
    
    AddProperty(new MP4Integer32Property(*this, "cid"));
    AddProperty(new MP4Integer32Property(*this, "width"));
    AddProperty(new MP4Integer32Property(*this, "height"));
    AddProperty(new MP4Integer32Property(*this, "unknown1"));
    AddProperty(new MP4Integer32Property(*this, "zero"));
    AddProperty(new MP4Integer32Property(*this, "unknown2"));
    AddReserved(*this, "padding", 80); // the rest
}

void MP4ARESAtom::Read()
{
    /* For some reason ARES is larger than it reports */
    SetSize(GetSize() + 4);
    SetEnd(GetEnd() + 4);

    /* do the usual read */
    MP4Atom::Read();
}

///////////////////////////////////////////////////////////////////////////////

}
} // namespace mp4v2::impl



