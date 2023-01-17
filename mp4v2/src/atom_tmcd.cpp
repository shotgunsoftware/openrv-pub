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

MP4TmcdAtom::MP4TmcdAtom (MP4File &file)
        : MP4Atom(file, "tmcd")
{
/*
       format tmcd
       reserved       00 00 00 00 00 00 
       data_reference 1
       reserved2      0
       flags          0
       timescale      2997
       frameduration  125
       numframes      24
       reserved3      26
       name:          001
 */
}

void MP4TmcdAtom::Read()
{
    if (string(GetParentAtom()->GetType()) == "stsd")
    {
        AddReserved(*this, "reserved1", 6);

        AddProperty(new MP4Integer16Property(*this, "dataReferenceIndex"));

        AddReserved(*this, "reserved2", 4);

        AddProperty(new MP4Integer32Property(*this, "flags"));
        AddProperty(new MP4Integer32Property(*this, "timescale"));
        AddProperty(new MP4Integer32Property(*this, "frameduration"));
        AddProperty(new MP4Integer8Property(*this, "numframes"));

        AddReserved(*this, "reserved3", 1); 

        ExpectChildAtom("name", Required, OnlyOne);

        MP4Atom::Read();

        if (!FindChildAtom("name"))
        {
            MP4Atom* pChildAtom = MP4Atom::CreateAtom(m_File, this, "name");
            InsertChildAtom(pChildAtom, m_pChildAtoms.Size());
            pChildAtom->Generate();
            SetSize(GetSize() + pChildAtom->GetSize());
        }
    }
    else if (string(GetParentAtom()->GetType()) == "gmhd")
    {
        ExpectChildAtom("tcmi", Required, OnlyOne);
        MP4Atom::Read();
    }

}

///////////////////////////////////////////////////////////////////////////////

}
} // namespace mp4v2::impl
