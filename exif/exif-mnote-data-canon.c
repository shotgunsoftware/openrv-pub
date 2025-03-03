/* exif-mnote-data-canon.c
 *
 * Copyright � 2002, 2003 Lutz M�ller <lutz@users.sourceforge.net>
 * Copyright � 2003 Matthieu Castet <mat-c@users.sourceforge.net>
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

#include <config.h>
#include "exif-mnote-data-canon.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <libexif/exif-byte-order.h>
#include <libexif/exif-utils.h>
#include <libexif/exif-data.h>

#define DEBUG

static void exif_mnote_data_canon_clear(ExifMnoteDataCanon* n)
{
    ExifMnoteData* d = (ExifMnoteData*)n;
    unsigned int i;

    if (!n)
        return;

    if (n->entries)
    {
        for (i = 0; i < n->count; i++)
            if (n->entries[i].data)
            {
                exif_mem_free(d->mem, n->entries[i].data);
                n->entries[i].data = NULL;
            }
        exif_mem_free(d->mem, n->entries);
        n->entries = NULL;
        n->count = 0;
    }
}

static void exif_mnote_data_canon_free(ExifMnoteData* n)
{
    if (!n)
        return;

    exif_mnote_data_canon_clear((ExifMnoteDataCanon*)n);
}

static void exif_mnote_data_canon_get_tags(ExifMnoteDataCanon* dc,
                                           unsigned int n, unsigned int* m,
                                           unsigned int* s)
{
    unsigned int from = 0, to;

    if (!dc || !m)
        return;
    for (*m = 0; *m < dc->count; (*m)++)
    {
        to = from + mnote_canon_entry_count_values(&dc->entries[*m]);
        if (to > n)
        {
            if (s)
                *s = n - from;
            break;
        }
        from = to;
    }
}

static char* exif_mnote_data_canon_get_value(ExifMnoteData* note,
                                             unsigned int n, char* val,
                                             unsigned int maxlen)
{
    ExifMnoteDataCanon* dc = (ExifMnoteDataCanon*)note;
    unsigned int m, s;

    if (!dc)
        return NULL;
    exif_mnote_data_canon_get_tags(dc, n, &m, &s);
    if (m >= dc->count)
        return NULL;
    return mnote_canon_entry_get_value(&dc->entries[m], s, val, maxlen);
}

static void exif_mnote_data_canon_set_byte_order(ExifMnoteData* d,
                                                 ExifByteOrder o)
{
    ExifByteOrder o_orig;
    ExifMnoteDataCanon* n = (ExifMnoteDataCanon*)d;
    unsigned int i;

    if (!n)
        return;

    o_orig = n->order;
    n->order = o;
    for (i = 0; i < n->count; i++)
    {
        n->entries[i].order = o;
        exif_array_set_byte_order(n->entries[i].format, n->entries[i].data,
                                  n->entries[i].components, o_orig, o);
    }
}

static void exif_mnote_data_canon_set_offset(ExifMnoteData* n, unsigned int o)
{
    if (n)
        ((ExifMnoteDataCanon*)n)->offset = o;
}

static void exif_mnote_data_canon_save(ExifMnoteData* ne, unsigned char** buf,
                                       unsigned int* buf_size)
{
    ExifMnoteDataCanon* n = (ExifMnoteDataCanon*)ne;
    unsigned int i, o, s, doff;

    if (!n || !buf || !buf_size)
        return;

    /*
     * Allocate enough memory for all entries and the number
     * of entries.
     */
    *buf_size = 2 + n->count * 12 + 4;
    *buf = exif_mem_alloc(ne->mem, sizeof(char) * *buf_size);
    if (!*buf)
        return;

    /* Save the number of entries */
    exif_set_short(*buf, n->order, (ExifShort)n->count);

    /* Save each entry */
    for (i = 0; i < n->count; i++)
    {
        o = 2 + i * 12;
        exif_set_short(*buf + o + 0, n->order, (ExifShort)n->entries[i].tag);
        exif_set_short(*buf + o + 2, n->order, (ExifShort)n->entries[i].format);
        exif_set_long(*buf + o + 4, n->order, n->entries[i].components);
        o += 8;
        s = exif_format_get_size(n->entries[i].format)
            * n->entries[i].components;
        if (s > 4)
        {
            *buf_size += s;

            /* Ensure even offsets. Set padding bytes to 0. */
            if (s & 1)
                *buf_size += 1;
            *buf = exif_mem_realloc(ne->mem, *buf, sizeof(char) * *buf_size);
            if (!*buf)
                return;
            doff = *buf_size - s;
            if (s & 1)
            {
                doff--;
                *(*buf + *buf_size - 1) = '\0';
            }
            exif_set_long(*buf + o, n->order, n->offset + doff);
        }
        else
            doff = o;

        /*
         * Write the data. Fill unneeded bytes with 0. Do not
         * crash if data is NULL.
         */
        if (!n->entries[i].data)
            memset(*buf + doff, 0, s);
        else
            memcpy(*buf + doff, n->entries[i].data, s);
        if (s < 4)
            memset(*buf + doff + s, 0, (4 - s));
    }
}

/* XXX
 * FIXME: exif_mnote_data_canon_load() may fail and there is no
 *        semantics to express that.
 *        See bug #1054323 for details, especially the comment by liblit
 *        after it has supposedly been fixed:
 *
 *        https://sourceforge.net/tracker/?func=detail&aid=1054323&group_id=12272&atid=112272
 *        Unfortunately, the "return" statements aren't commented at
 *        all, so it isn't trivial to find out what is a normal
 *        return, and what is a reaction to an error condition.
 */

static void exif_mnote_data_canon_load(ExifMnoteData* ne,
                                       const unsigned char* buf,
                                       unsigned int buf_size)
{
    ExifMnoteDataCanon* n = (ExifMnoteDataCanon*)ne;
    ExifShort c;
    unsigned int i, o, s;

    if (!n || !buf || !buf_size || (buf_size < 6 + n->offset + 2))
        return;

    /* Read the number of entries and remove old ones. */
    c = exif_get_short(buf + 6 + n->offset, n->order);
    exif_mnote_data_canon_clear(n);

    /* Parse the entries */
    for (i = 0; i < c; i++)
    {
        o = 6 + 2 + n->offset + 12 * i;
        if (o + 8 > buf_size)
            return;

        n->count = i + 1;
        n->entries = exif_mem_realloc(ne->mem, n->entries,
                                      sizeof(MnoteCanonEntry) * (i + 1));
        memset(&n->entries[i], 0, sizeof(MnoteCanonEntry));
        n->entries[i].tag = exif_get_short(buf + o, n->order);
        n->entries[i].format = exif_get_short(buf + o + 2, n->order);
        n->entries[i].components = exif_get_long(buf + o + 4, n->order);
        n->entries[i].order = n->order;

        /*
         * Size? If bigger than 4 bytes, the actual data is not
         * in the entry but somewhere else (offset).
         */
        s = exif_format_get_size(n->entries[i].format)
            * n->entries[i].components;
        if (!s)
            return;
        o += 8;
        if (s > 4)
            o = exif_get_long(buf + o, n->order) + 6;
        if (o + s > buf_size)
            return;

        /* Sanity check */
        n->entries[i].data = exif_mem_alloc(ne->mem, sizeof(char) * s);
        if (!n->entries[i].data)
            return;
        n->entries[i].size = s;
        memcpy(n->entries[i].data, buf + o, s);
    }
}

static unsigned int exif_mnote_data_canon_count(ExifMnoteData* n)
{
    ExifMnoteDataCanon* dc = (ExifMnoteDataCanon*)n;
    unsigned int i, c;

    for (i = c = 0; dc && (i < dc->count); i++)
        c += mnote_canon_entry_count_values(&dc->entries[i]);
    return c;
}

static unsigned int exif_mnote_data_canon_get_id(ExifMnoteData* d,
                                                 unsigned int i)
{
    ExifMnoteDataCanon* dc = (ExifMnoteDataCanon*)d;
    unsigned int m;

    if (!dc)
        return 0;
    exif_mnote_data_canon_get_tags(dc, i, &m, NULL);
    if (m >= dc->count)
        return 0;
    return dc->entries[m].tag;
}

static const char* exif_mnote_data_canon_get_name(ExifMnoteData* note,
                                                  unsigned int i)
{
    ExifMnoteDataCanon* dc = (ExifMnoteDataCanon*)note;
    unsigned int m, s;

    if (!dc)
        return NULL;
    exif_mnote_data_canon_get_tags(dc, i, &m, &s);
    if (m >= dc->count)
        return NULL;
    return mnote_canon_tag_get_name_sub(dc->entries[m].tag, s, dc->options);
}

static const char* exif_mnote_data_canon_get_title(ExifMnoteData* note,
                                                   unsigned int i)
{
    ExifMnoteDataCanon* dc = (ExifMnoteDataCanon*)note;
    unsigned int m, s;

    if (!dc)
        return NULL;
    exif_mnote_data_canon_get_tags(dc, i, &m, &s);
    if (m >= dc->count)
        return NULL;
    return mnote_canon_tag_get_title_sub(dc->entries[m].tag, s, dc->options);
}

static const char* exif_mnote_data_canon_get_description(ExifMnoteData* note,
                                                         unsigned int i)
{
    ExifMnoteDataCanon* dc = (ExifMnoteDataCanon*)note;
    unsigned int m;

    if (!dc)
        return NULL;
    exif_mnote_data_canon_get_tags(dc, i, &m, NULL);
    if (m >= dc->count)
        return NULL;
    return mnote_canon_tag_get_description(dc->entries[m].tag);
}

ExifMnoteData* exif_mnote_data_canon_new(ExifMem* mem, ExifDataOption o)
{
    ExifMnoteData* d;
    ExifMnoteDataCanon* dc;

    if (!mem)
        return NULL;

    d = exif_mem_alloc(mem, sizeof(ExifMnoteDataCanon));
    if (!d)
        return NULL;

    exif_mnote_data_construct(d, mem);

    /* Set up function pointers */
    d->methods.free = exif_mnote_data_canon_free;
    d->methods.set_byte_order = exif_mnote_data_canon_set_byte_order;
    d->methods.set_offset = exif_mnote_data_canon_set_offset;
    d->methods.load = exif_mnote_data_canon_load;
    d->methods.save = exif_mnote_data_canon_save;
    d->methods.count = exif_mnote_data_canon_count;
    d->methods.get_id = exif_mnote_data_canon_get_id;
    d->methods.get_name = exif_mnote_data_canon_get_name;
    d->methods.get_title = exif_mnote_data_canon_get_title;
    d->methods.get_description = exif_mnote_data_canon_get_description;
    d->methods.get_value = exif_mnote_data_canon_get_value;

    dc = (ExifMnoteDataCanon*)d;
    dc->options = o;
    return d;
}
