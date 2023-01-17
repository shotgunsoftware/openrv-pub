#ifndef __WINGL_H__
#define __WINGL_H__

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <gl/gl.h>
#include <gl/glu.h>

/* Functions */
int isExtensionSupported(const char *extstring);

#undef WIN32_LEAN_AND_MEAN
#undef NOMINMAX

#endif
