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

namespace mp4v2
{
    namespace impl
    {

        ///////////////////////////////////////////////////////////////////////////////

        MP4TmcdNameAtom::MP4TmcdNameAtom(MP4File& file)
            : MP4Atom(file, "name")
        {
            AddReserved(*this, "reserved2", 4);
            AddProperty(new MP4StringProperty(*this, "name", false));
        }

        void MP4TmcdNameAtom::Read()
        {
            ((MP4StringProperty*)m_pProperties[1])->SetFixedLength(m_size - 4);
            MP4Atom::Read();
        }

        ///////////////////////////////////////////////////////////////////////////////

    } // namespace impl
} // namespace mp4v2
