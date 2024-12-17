/*
 * DIRENT.H (formerly DIRLIB.H)
 * This file has no copyright assigned and is placed in the Public Domain.
 * This file is a part of the mingw-runtime package.
 * No warranty is given; refer to the file DISCLAIMER within the package.
 *
 */
#ifndef _DIRENT_H_
#define _DIRENT_H_

#ifdef _POSIX_
#define REDEF_POSIX
#undef _POSIX_
#endif

#ifdef _UNICODE
#define REDEF__UNICODE
#undef _UNICODE
#endif

#ifdef UNICODE
#define REDEF_UNICODE
#undef UNICODE
#endif

#include <stdio.h>
#include <io.h>
#include <win_posix_config.h>

#include <windows.h> /* for GetFileAttributes */

#ifndef RC_INVOKED

#ifdef __cplusplus
extern "C"
{
#endif

#ifdef _UNICODE
#define _tdirent _wdirent
#define _TDIR _WDIR
#define _topendir _wopendir
#define _tclosedir _wclosedir
#define _treaddir _wreaddir
#define _trewinddir _wrewinddir
#define _ttelldir _wtelldir
#define _tseekdir _wseekdir
#else
#define _tdirent dirent
#define _TDIR DIR
#define _topendir opendir
#define _tclosedir closedir
#define _treaddir readdir
#define _trewinddir rewinddir
#define _ttelldir telldir
#define _tseekdir seekdir
#endif

    struct _tdirent
    {
        long d_ino;              /* Always zero. */
        unsigned short d_reclen; /* Always zero. */
        unsigned short d_namlen; /* Length of name in d_name. */
        char d_name[MAX_PATH];   /* File name. */
    };

    /*
     * This is an internal data structure. Good programmers will not use it
     * except as an argument to one of the functions below.
     * dd_stat field is now int (was short in older versions).
     */
    typedef struct
    {
        /* disk transfer area for this dir */
        struct __finddata64_t dd_dta;

        /* dirent struct to return from dir (NOTE: this makes this thread
         * safe as long as only one thread uses a particular DIR struct at
         * a time) */
        struct _tdirent dd_dir;

        /* _findnext handle */
        intptr_t dd_handle;

        /*
         * Status of search:
         *   0 = not started yet (next entry to read is first entry)
         *  -1 = off the end
         *   positive = 0 based index of next entry
         */
        int dd_stat;

        /* given path for dir with search pattern (struct is extended) */
        char dd_name[1];
    } _TDIR;

    WIN_POSIX_EXPORT _TDIR* __cdecl _topendir(const char*);
    WIN_POSIX_EXPORT struct _tdirent* __cdecl _treaddir(_TDIR*);
    WIN_POSIX_EXPORT int __cdecl _tclosedir(_TDIR*);
    WIN_POSIX_EXPORT void __cdecl _trewinddir(_TDIR*);
    WIN_POSIX_EXPORT long __cdecl _ttelldir(_TDIR*);
    WIN_POSIX_EXPORT void __cdecl _tseekdir(_TDIR*, long);

    /* wide char versions */

    struct _wdirent
    {
        long d_ino;               /* Always zero. */
        unsigned short d_reclen;  /* Always zero. */
        unsigned short d_namlen;  /* Length of name in d_name. */
        wchar_t d_name[MAX_PATH]; /* File name. */
    };

    /*
     * This is an internal data structure. Good programmers will not use it
     * except as an argument to one of the functions below.
     */
    typedef struct
    {
        /* disk transfer area for this dir */
        struct _wfinddata64_t dd_dta;

        /* dirent struct to return from dir (NOTE: this makes this thread
         * safe as long as only one thread uses a particular DIR struct at
         * a time) */
        struct _wdirent dd_dir;

        /* _findnext handle */
        intptr_t dd_handle;

        /*
         * Status of search:
         *   0 = not started yet (next entry to read is first entry)
         *  -1 = off the end
         *   positive = 0 based index of next entry
         */
        int dd_stat;

        /* given path for dir with search pattern (struct is extended) */
        wchar_t dd_name[1];
    } _WDIR;

    WIN_POSIX_EXPORT _WDIR* __cdecl _wopendir(const wchar_t*);
    WIN_POSIX_EXPORT struct _wdirent* __cdecl _wreaddir(_WDIR*);
    WIN_POSIX_EXPORT int __cdecl _wclosedir(_WDIR*);
    WIN_POSIX_EXPORT void __cdecl _wrewinddir(_WDIR*);
    WIN_POSIX_EXPORT long __cdecl _wtelldir(_WDIR*);
    WIN_POSIX_EXPORT void __cdecl _wseekdir(_WDIR*, long);

#ifdef __cplusplus
}
#endif

#endif /* Not RC_INVOKED */

#ifdef REDEF_POSIX
#define _POSIX_
#endif

#ifdef REDEF__UNICODE
#define _UNICODE
#endif

#ifdef REDEF_UNICODE
#define UNICODE
#endif

#endif /* Not _DIRENT_H_ */
