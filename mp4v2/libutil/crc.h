///////////////////////////////////////////////////////////////////////////////
//
//  The contents of this file are subject to the Mozilla Public License
//  Version 1.1 (the "License"); you may not use this file except in
//  compliance with the License. You may obtain a copy of the License at
//  http://www.mozilla.org/MPL/
//
//  Software distributed under the License is distributed on an "AS IS"
//  basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
//  License for the specific language governing rights and limitations
//  under the License.
//
//  The Original Code is MP4v2.
//
//  The Initial Developer of the Original Code is Kona Blend.
//  Portions created by Kona Blend are Copyright (C) 2008.
//  All Rights Reserved.
//
//  Contributors:
//      Kona Blend, kona8lend@@gmail.com
//
///////////////////////////////////////////////////////////////////////////////

#ifndef MP4V2_UTIL_CRC_H
#define MP4V2_UTIL_CRC_H

namespace mp4v2
{
    namespace util
    {

        ///////////////////////////////////////////////////////////////////////////////

        MP4V2_EXPORT
        uint32_t crc32(const unsigned char*, uint32_t); // ISO/IEC 8802-3:1989

        ///////////////////////////////////////////////////////////////////////////////

    } // namespace util
} // namespace mp4v2

#endif // MP4V2_UTIL_CRC_H
