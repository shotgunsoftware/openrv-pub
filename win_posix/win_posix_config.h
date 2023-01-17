
#if !defined(WIN_POSIX_EXPORT)
#if defined(WIN_POSIX_BUILD)
#define WIN_POSIX_EXPORT __declspec(dllexport)
#else
#define WIN_POSIX_EXPORT __declspec(dllimport)
#endif
#endif
