#ifndef EXIF_DLL
#define EXIF_DLL

#ifdef WIN32
#ifdef EXIF_BUILD
#define EXIF_EXPORT __declspec(dllexport)
#else
#define EXIF_EXPORT __declspec(dllimport)
#endif
#else
#define EXIF_EXPORT
#endif

#endif
