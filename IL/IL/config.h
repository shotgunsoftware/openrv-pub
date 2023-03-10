/* include/IL/config.h.  Generated by configure.  */
/* include/IL/config.h.in.  Generated from configure.ac by autoheader.  */

/* Altivec extension found */
/* #undef ALTIVEC_GCC */

/* PPC_ASM assembly found */
/* #undef GCC_PCC_ASM */

/* X86_64_ASM assembly found */
/* #undef GCC_X86_64_ASM */

#ifdef ARCH_IA32
/* X86_ASM assembly found */
//#define GCC_X86_ASM 
#endif

/* Define to 1 if you have the <dlfcn.h> header file. */
#define HAVE_DLFCN_H 1

/* Define to 1 if you have the <inttypes.h> header file. */
#define HAVE_INTTYPES_H 1

/* Define to 1 if you have the <memory.h> header file. */
#define HAVE_MEMORY_H 1

/* Define to 1 if you have the <stdint.h> header file. */
#define HAVE_STDINT_H 1

/* Define to 1 if you have the <stdlib.h> header file. */
#define HAVE_STDLIB_H 1

/* Define to 1 if you have the <strings.h> header file. */
#define HAVE_STRINGS_H 1

/* Define to 1 if you have the <string.h> header file. */
#define HAVE_STRING_H 1

/* Define to 1 if you have the <sys/stat.h> header file. */
#define HAVE_SYS_STAT_H 1

/* Define to 1 if you have the <sys/types.h> header file. */
#define HAVE_SYS_TYPES_H 1

/* Define to 1 if you have the <unistd.h> header file. */
#define HAVE_UNISTD_H 1

/* Support Allegro API */
/* #undef ILUT_USE_ALLEGRO */

/* Support DirectX8 API */
/* #undef ILUT_USE_DIRECTX8 */

/* Support DirectX9 API */
/* #undef ILUT_USE_DIRECTX9 */

/* Support OpenGL API */
/* #undef ILUT_USE_OPENGL */

/* Support SDL API */
/* #undef ILUT_USE_SDL */

/* BMP support */
/* #undef IL_NO_BMP */

/* DCX support */
/* #undef IL_NO_DCX */

/* GIF support */
/* #undef IL_NO_GIF */

/* HDR support */
/* #undef IL_NO_HDR */

/* ICON support */
/* #undef IL_NO_ICON */

/* JPG support */
/*#define IL_NO_JPG */

/* LCMS support */
/*#define IL_NO_LCMS */

/* LIF support */
/* #undef IL_NO_LIF */

/* MDL support */
/* #undef IL_NO_MDL */

/* MNG support */
/*#define IL_NO_MNG */

/* PCD support */
/* #undef IL_NO_PCD */

/* PCX support */
/* #undef IL_NO_PCX */

/* PIC support */
/* #undef IL_NO_PIC */

/* PIX support */
/* #undef IL_NO_PIX */

/* PNG support */
/*#define IL_NO_PNG  */

/* PNM support */
/* #undef IL_NO_PNM */

/* PSD support */
/* #undef IL_NO_PSD */

/* PSP support */
/* #undef IL_NO_PSP */

/* PXR support */
/* #undef IL_NO_PXR */

/* RAW support */
/* #undef IL_NO_RAW */

/* SGI support */
/* #undef IL_NO_SGI */

/* TGA support */
/* #undef IL_NO_TGA */

/* TIF support */
/*#define IL_NO_TIF  */

/* WAD support */
/* #undef IL_NO_WAD */

/* WAL support */
/* #undef IL_NO_WAL */

/* XPM support */
/* #undef IL_NO_XPM */

/* Use libjpeg without modification. always enabled. */
#define IL_USE_JPEGLIB_UNMODIFIED 

/* LCMS include without lcms/ support */
#define LCMS_NODIRINCLUDE 

/* Building on Mac OS X */
#define MAX_OS_X 

/* memalign memory allocation */
/* #undef MEMALIGN */

/* mm_malloc memory allocation */
#ifndef WIN32
#define MM_MALLOC 
#endif

/* Name of package */
#define PACKAGE "DevIL"

/* Define to the address where bug reports for this package should be sent. */
#define PACKAGE_BUGREPORT ""

/* Define to the full name of this package. */
#define PACKAGE_NAME ""

/* Define to the full name and version of this package. */
#define PACKAGE_STRING ""

/* Define to the one symbol short name of this package. */
#define PACKAGE_TARNAME ""

/* Define to the version of this package. */
#define PACKAGE_VERSION ""

/* posix_memalign memory allocation */
/* #undef POSIX_MEMALIGN */

#ifdef ARCH_IA32
/* SSE extension found */
#define SSE 

/* SSE2 extension found */
#define SSE2 
#endif

/* SSE3 extension found */
/* #undef SSE3 */

/* Define to 1 if you have the ANSI C header files. */
#define STDC_HEADERS 1

/* valloc memory allocation */
#ifndef WIN32
#define VALLOC 
#endif

/* Memory must be vector aligned */
#ifndef WIN32
#define VECTORMEM 
#endif

/* Version number of package */
#define VERSION "1.6.8"

/* Define to 1 if your processor stores words with the most significant byte
   first (like Motorola and SPARC, unlike Intel and VAX). */
/* #undef WORDS_BIGENDIAN */

/* Define to 1 if the X Window System is missing or not being used. */
/* #undef X_DISPLAY_MISSING */
