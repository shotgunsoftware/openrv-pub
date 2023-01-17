#ifndef __POSIX_FILE__
#define __POSIX_FILE__

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

#define ssize_t long
#define open _open
#define lseek _lseek
#define close _close

#endif
