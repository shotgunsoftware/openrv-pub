/* exif-mnote-data.h
 *
 * Copyright © 2003 Lutz Müller <lutz@users.sourceforge.net>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __EXIF_MNOTE_DATA_H__
#define __EXIF_MNOTE_DATA_H__

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

#include <libexif/exif-dll.h>
#include <libexif/exif-log.h>

    typedef struct _ExifMnoteData ExifMnoteData;

    EXIF_EXPORT void exif_mnote_data_ref(ExifMnoteData*);
    EXIF_EXPORT void exif_mnote_data_unref(ExifMnoteData*);

    EXIF_EXPORT void exif_mnote_data_load(ExifMnoteData*, const unsigned char*,
                                          unsigned int);
    EXIF_EXPORT void exif_mnote_data_save(ExifMnoteData*, unsigned char**,
                                          unsigned int*);

    EXIF_EXPORT unsigned int exif_mnote_data_count(ExifMnoteData*);
    EXIF_EXPORT unsigned int exif_mnote_data_get_id(ExifMnoteData*,
                                                    unsigned int);
    EXIF_EXPORT const char* exif_mnote_data_get_name(ExifMnoteData*,
                                                     unsigned int);
    EXIF_EXPORT const char* exif_mnote_data_get_title(ExifMnoteData*,
                                                      unsigned int);
    EXIF_EXPORT const char* exif_mnote_data_get_description(ExifMnoteData*,
                                                            unsigned int);

    /* Returns NULL or val */
    EXIF_EXPORT char* exif_mnote_data_get_value(ExifMnoteData*, unsigned int,
                                                char* val, unsigned int maxlen);

    EXIF_EXPORT void exif_mnote_data_log(ExifMnoteData*, ExifLog*);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __EXIF_MNOTE_DATA_H__ */
