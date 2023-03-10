/*
  Copyright 2008 Larry Gritz and the other authors and contributors.
  All Rights Reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are
  met:
  * Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
  * Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.
  * Neither the name of the software's owners nor the names of its
    contributors may be used to endorse or promote products derived from
    this software without specific prior written permission.
  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

  (This is the Modified BSD License)
*/


/// \file
/// Declarations for things that are used privately by ImageIO.


#ifndef OPENIMAGEIO_IMAGEIO_PVT_H
#define OPENIMAGEIO_IMAGEIO_PVT_H

#include "OpenImageIO/imageio.h"
#include "OpenImageIO/thread.h"



OIIO_NAMESPACE_ENTER
{

namespace pvt {

/// Mutex allowing thread safety of ImageOutput internals
///
extern recursive_mutex imageio_mutex;
extern atomic_int oiio_threads;
extern atomic_int oiio_read_chunk;
extern ustring plugin_searchpath;
extern std::string format_list;
extern std::string extension_list;


// For internal use - use error() below for a nicer interface.
void seterror (const std::string& message);

// Make sure all plugins are inventoried.  Should only be called while
// imageio_mutex is held.  For internal use only.
void catalog_all_plugins (std::string searchpath);

/// Use error() privately only.  Protoype is conceptually printf-like, but
/// also fully typesafe:
/// void error (const char *format, ...);
TINYFORMAT_WRAP_FORMAT (void, error, /**/,
    std::ostringstream msg;, msg, seterror(msg.str());)

/// Given the format, set the default quantization range.
void get_default_quantize (TypeDesc format,
                           long long &quant_min, long long &quant_max);

/// Turn potentially non-contiguous-stride data (e.g. "RGBxRGBx") into
/// contiguous-stride ("RGBRGB"), for any format or stride values
/// (measured in bytes).  Caller must pass in a dst pointing to enough
/// memory to hold the contiguous rectangle.  Return a ptr to where the
/// contiguous data ended up, which is either dst or src (if the strides
/// indicated that data were already contiguous).
const void *contiguize (const void *src, int nchannels,
                        stride_t xstride, stride_t ystride, stride_t zstride, 
                        void *dst, int width, int height, int depth,
                        TypeDesc format);

/// Turn contiguous data from any format into float data.  Return a
/// pointer to the converted data (which may still point to src if no
/// conversion was necessary).
const float *convert_to_float (const void *src, float *dst, int nvals,
                               TypeDesc format);

/// Turn contiguous float data into any format.  Return a pointer to the
/// converted data (which may still point to src if no conversion was
/// necessary).
const void *convert_from_float (const float *src, void *dst, size_t nvals,
                                long long quant_min, long long quant_max,
                                TypeDesc format);
/// DEPRECATED (1.4)
const void *convert_from_float (const float *src, void *dst, size_t nvals,
                                long long quant_black, long long quant_white,
                                long long quant_min, long long quant_max,
                                TypeDesc format);

/// A version of convert_from_float that will break up big jobs with
/// multiple threads.
const void *parallel_convert_from_float (const float *src, void *dst,
                                         size_t nvals,
                                         TypeDesc format, int nthreads=0);
/// DEPRECATED (1.4)
const void *parallel_convert_from_float (const float *src, void *dst,
                                         size_t nvals,
                                         long long quant_black, long long quant_white,
                                         long long quant_min, long long quant_max,
                                         TypeDesc format, int nthreads=0) ;

}  // namespace pvt

}
OIIO_NAMESPACE_EXIT

//Define a default plugin search path
#define OIIO_DEFAULT_PLUGIN_SEARCHPATH ""

#endif // OPENIMAGEIO_IMAGEIO_PVT_H
