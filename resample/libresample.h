/**********************************************************************

  resample.h

  Real-time library interface by Dominic Mazzoni

  Based on resample-1.7:
    http://www-ccrma.stanford.edu/~jos/resample/

  License: LGPL - see the file LICENSE.txt for more information

**********************************************************************/

#ifndef LIBRESAMPLE_INCLUDED
#define LIBRESAMPLE_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif	/* __cplusplus */

#ifdef WIN32
#ifdef LIBRESAMPLE_BUILD
#define LIBRESAMPLE_EXPORT __declspec(dllexport)
#else
#define LIBRESAMPLE_EXPORT __declspec(dllimport)
#endif
#else
#define LIBRESAMPLE_EXPORT
#endif

LIBRESAMPLE_EXPORT void *resample_open(int      highQuality,
                                       double   minFactor,
                                       double   maxFactor);

LIBRESAMPLE_EXPORT void *resample_dup(const void *handle);

LIBRESAMPLE_EXPORT void resample_reset(void *handle);

LIBRESAMPLE_EXPORT int resample_get_filter_width(const void *handle);

LIBRESAMPLE_EXPORT int resample_process(void   *handle,
                                        double  factor,
                                        float  *inBuffer,
                                        int     inBufferLen,
                                        int     lastFlag,
                                        int    *inBufferUsed,
                                        float  *outBuffer,
                                        int     outBufferLen);

LIBRESAMPLE_EXPORT void resample_close(void *handle);

#ifdef __cplusplus
}		/* extern "C" */
#endif	/* __cplusplus */

#endif /* LIBRESAMPLE_INCLUDED */
