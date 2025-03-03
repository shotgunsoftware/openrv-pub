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
 * Contributer has declined to give copyright information, and gives
 * it freely to the world.
 *
 * Contributor(s):
 */

#include "src/impl.h"

namespace mp4v2
{
    namespace impl
    {

        ///////////////////////////////////////////////////////////////////////////////

        MP4GminAtom::MP4GminAtom(MP4File& file)
            : MP4Atom(file, "gmin")
        {

            AddVersionAndFlags(); /* 0, 1 */

            AddProperty(
                new MP4Integer16Property(*this, "graphicsMode"));       /* 2 */
            AddProperty(new MP4Integer16Property(*this, "opColorRed")); /* 3 */
            AddProperty(
                new MP4Integer16Property(*this, "opColorGreen"));        /* 4 */
            AddProperty(new MP4Integer16Property(*this, "opColorBlue")); /* 5 */
            AddProperty(new MP4Integer16Property(*this, "balance"));     /* 6 */
            AddReserved(*this, "reserved", 2);                           /* 7 */
        }

        void MP4GminAtom::Generate()
        {

            MP4Atom::Generate();

            ((MP4Integer16Property*)m_pProperties[2])->SetValue(0x0040);
            ((MP4Integer16Property*)m_pProperties[3])->SetValue(0x8000);
            ((MP4Integer16Property*)m_pProperties[4])->SetValue(0x8000);
            ((MP4Integer16Property*)m_pProperties[5])->SetValue(0x8000);
            ((MP4Integer16Property*)m_pProperties[6])->SetValue(0x0000);
        }

        ///////////////////////////////////////////////////////////////////////////////

    } // namespace impl
} // namespace mp4v2
