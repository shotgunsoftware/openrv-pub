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

#include <boost/bind.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/regex.hpp>

//#include <OpenEXR/half.h>
#include <half.h>

#include <cmath>

#include "OpenImageIO/imagebuf.h"
#include "OpenImageIO/imagebufalgo.h"
#include "OpenImageIO/imagebufalgo_util.h"
#include "OpenImageIO/dassert.h"
#include "OpenImageIO/platform.h"
#include "OpenImageIO/filter.h"
#include "OpenImageIO/thread.h"
#include "kissfft.hh"



///////////////////////////////////////////////////////////////////////////
// Guidelines for ImageBufAlgo functions:
//
// * Signature will always be:
//       bool function (ImageBuf &R /* result */, 
//                      const ImageBuf &A, ...other input images...,
//                      ...other parameters...
//                      ROI roi = ROI::All(),
//                      int nthreads = 0);
// * The ROI should restrict the operation to those pixels (and channels)
//   specified. Default ROI::All() means perform the operation on all
//   pixel in R's data window.
// * It's ok to omit ROI and threads from the few functions that
//   (a) can't possibly be parallelized, and (b) do not make sense to
//   apply to anything less than the entire image.
// * Be sure to clamp the channel range to those actually used.
// * If R is initialized, do not change any pixels outside the ROI.
//   If R is uninitialized, redefine ROI to be the union of the input
//   images' data windows and allocate R to be that size.
// * Try to always do the "reasonable thing" rather than be too brittle.
// * For errors (where there is no "reasonable thing"), set R's error
//   condition using R.error() with R.error() and return false.
// * Always use IB::Iterators/ConstIterator, NEVER use getpixel/setpixel.
// * Use the iterator Black or Clamp wrap modes to avoid lots of special
//   cases inside the pixel loops.
// * Use OIIO_DISPATCH_* macros to call type-specialized templated
//   implemenations.  It is permissible to use OIIO_DISPATCH_COMMON_TYPES_*
//   to tame the cross-product of types, especially for binary functions
//   (A,B inputs as well as R output).
///////////////////////////////////////////////////////////////////////////


OIIO_NAMESPACE_ENTER
{


// Convenient helper struct to bundle a 3-int describing a block size.
struct Dim3 {
    int x, y, z;
    Dim3 (int x, int y=1, int z=1) : x(x), y(y), z(z) { }
};



bool
ImageBufAlgo::IBAprep (ROI &roi, ImageBuf *dst,
                       const ImageBuf *A, const ImageBuf *B,
                       ImageSpec *force_spec, int prepflags)
{
    if (A && !A->initialized()) {
        if (dst)
            dst->error ("Uninitialized input image");
        return false;
    }
    if (B && !B->initialized()) {
        if (dst)
            dst->error ("Uninitialized destination image");
        return false;
    }
    if (dst->initialized()) {
        // Valid destination image.  Just need to worry about ROI.
        if (roi.defined()) {
            // Shrink-wrap ROI to the destination (including chend)
            roi = roi_intersection (roi, get_roi(dst->spec()));
        } else {
            // No ROI? Set it to all of dst's pixel window.
            roi = get_roi (dst->spec());
        }
        // If the dst is initialized but is a cached image, we'll need
        // to fully read it into allocated memory so that we're able
        // to write to it subsequently.
        if (dst->storage() == ImageBuf::IMAGECACHE) {
            dst->read (dst->subimage(), dst->miplevel(), true /*force*/);
            ASSERT (dst->storage() == ImageBuf::LOCALBUFFER);
        }
    } else {
        // Not an initialized destination image!
        ASSERT ((A || roi.defined()) &&
                "ImageBufAlgo without any guess about region of interest");
        ROI full_roi;
        if (! roi.defined()) {
            // No ROI -- make it the union of the pixel regions of the inputs
            roi = get_roi (A->spec());
            full_roi = get_roi_full (A->spec());
            if (B) {
                roi = roi_union (roi, get_roi (B->spec()));
                full_roi = roi_union (full_roi, get_roi_full (B->spec()));
            }
        } else {
            if (A) {
                roi.chend = std::min (roi.chend, A->nchannels());
                if (! (prepflags & IBAprep_NO_COPY_ROI_FULL))
                    full_roi = A->roi_full();
            } else {
                full_roi = roi;
            }
        }
        // Now we allocate space for dst.  Give it A's spec, but adjust
        // the dimensions to match the ROI.
        ImageSpec spec;
        if (A) {
            // If there's an input image, give dst A's spec (with
            // modifications detailed below...)
            spec = force_spec ? (*force_spec) : A->spec();
            // For two inputs, if they aren't the same data type, punt and
            // allocate a float buffer. If the user wanted something else,
            // they should have pre-allocated dst with their desired format.
            if (B && A->spec().format != B->spec().format)
                spec.set_format (TypeDesc::FLOAT);
            // No good can come from automatically polluting an ImageBuf
            // with some other ImageBuf's tile sizes.
            spec.tile_width = 0;
            spec.tile_height = 0;
            spec.tile_depth = 0;
        } else if (force_spec) {
            spec = *force_spec;
        } else {
            spec.set_format (TypeDesc::FLOAT);
            spec.nchannels = roi.chend;
            spec.default_channel_names ();
        }
        // Set the image dimensions based on ROI.
        set_roi (spec, roi);
        if (full_roi.defined())
            set_roi_full (spec, full_roi);
        else
            set_roi_full (spec, roi);

        if (prepflags & IBAprep_NO_COPY_METADATA)
            spec.extra_attribs.clear();
        else if (! (prepflags & IBAprep_COPY_ALL_METADATA)) {
            // Since we're altering pixels, be sure that any existing SHA
            // hash of dst's pixel values is erased.
            spec.erase_attribute ("oiio:SHA-1");
            static boost::regex regex_sha ("SHA-1=[[:xdigit:]]*[ ]*");
            std::string desc = spec.get_string_attribute ("ImageDescription");
            if (desc.size())
                spec.attribute ("ImageDescription",
                                boost::regex_replace (desc, regex_sha, ""));
        }

        dst->reset (spec);
    }
    if (prepflags & IBAprep_REQUIRE_ALPHA) {
        if (dst->spec().alpha_channel < 0 ||
              (A && A->spec().alpha_channel < 0) ||
              (B && B->spec().alpha_channel < 0)) {
            dst->error ("images must have alpha channels");
            return false;
        }
    }
    if (prepflags & IBAprep_REQUIRE_Z) {
        if (dst->spec().z_channel < 0 ||
              (A && A->spec().z_channel < 0) ||
              (B && B->spec().z_channel < 0)) {
            dst->error ("images must have depth channels");
            return false;
        }
    }
    if (prepflags & IBAprep_REQUIRE_SAME_NCHANNELS) {
        int n = dst->spec().nchannels;
        if ((A && A->spec().nchannels != n) ||
            (B && B->spec().nchannels != n)) {
            dst->error ("images must have the same number of channels");
            return false;
        }
    }
    if (prepflags & IBAprep_NO_SUPPORT_VOLUME) {
        if (dst->spec().depth > 1 ||
                (A && A->spec().depth > 1) ||
                (B && B->spec().depth > 1)) {
            dst->error ("volumes not supported");
            return false;
        }
    }
    return true;
}



/// Given data types a and b, return a type that is a best guess for one
/// that can handle both without any loss of range or precision.
TypeDesc::BASETYPE
ImageBufAlgo::type_merge (TypeDesc::BASETYPE a, TypeDesc::BASETYPE b)
{
    // Same type already? done.
    if (a == b)
        return a;
    if (a == TypeDesc::UNKNOWN)
        return b;
    if (b == TypeDesc::UNKNOWN)
        return a;
    // Canonicalize so a's size (in bytes) is >= b's size in bytes. This
    // unclutters remaining cases.
    if (TypeDesc(a).size() < TypeDesc(b).size())
        std::swap (a, b);
    // Double or float trump anything else
    if (a == TypeDesc::DOUBLE || a == TypeDesc::FLOAT)
        return a;
    if (a == TypeDesc::UINT32 && (b == TypeDesc::UINT16 || b == TypeDesc::UINT8))
        return a;
    if (a == TypeDesc::INT32 && (b == TypeDesc::INT16 || b == TypeDesc::UINT16 ||
                                 b == TypeDesc::INT8 || b == TypeDesc::UINT8))
        return a;
    if ((a == TypeDesc::UINT16 || a == TypeDesc::HALF) && b == TypeDesc::UINT8)
        return a;
    if ((a == TypeDesc::INT16 || a == TypeDesc::HALF) &&
        (b == TypeDesc::INT8 || b == TypeDesc::UINT8))
        return a;
    // Out of common cases. For all remaining edge cases, punt and say that
    // we prefer float.
    return TypeDesc::FLOAT;
}



template<typename T>
static bool
fill_ (ImageBuf &dst, const float *values, ROI roi=ROI(), int nthreads=1)
{
    if (nthreads != 1 && roi.npixels() >= 1000) {
        // Lots of pixels and request for multi threads? Parallelize.
        ImageBufAlgo::parallel_image (
            boost::bind(fill_<T>, boost::ref(dst), values,
                        _1 /*roi*/, 1 /*nthreads*/),
            roi, nthreads);
        return true;
    }

    // Serial case
    for (ImageBuf::Iterator<T> p (dst, roi);  !p.done();  ++p)
        for (int c = roi.chbegin;  c < roi.chend;  ++c)
            p[c] = values[c];
    return true;
}


bool
ImageBufAlgo::fill (ImageBuf &dst, const float *pixel, ROI roi, int nthreads)
{
    ASSERT (pixel && "fill must have a non-NULL pixel value pointer");
    if (! IBAprep (roi, &dst))
        return false;
    bool ok;
    OIIO_DISPATCH_TYPES (ok, "fill", fill_, dst.spec().format,
                         dst, pixel, roi, nthreads);
    return ok;
}


bool
ImageBufAlgo::zero (ImageBuf &dst, ROI roi, int nthreads)
{
    if (! IBAprep (roi, &dst))
        return false;
    float *zero = ALLOCA(float,roi.chend);
    memset (zero, 0, roi.chend*sizeof(float));
    return fill (dst, zero, roi, nthreads);
}



template<typename T>
static bool
checker_ (ImageBuf &dst, Dim3 size,
          const float *color1, const float *color2,
          Dim3 offset,
          ROI roi, int nthreads=1)
{
    if (nthreads != 1 && roi.npixels() >= 1000) {
        // Lots of pixels and request for multi threads? Parallelize.
        ImageBufAlgo::parallel_image (
            boost::bind(checker_<T>, boost::ref(dst),
                        size, color1, color2, offset,
                        _1 /*roi*/, 1 /*nthreads*/),
            roi, nthreads);
        return true;
    }

    // Serial case
    for (ImageBuf::Iterator<T> p (dst, roi);  !p.done();  ++p) {
        int xtile = (p.x()-offset.x)/size.x;  xtile += (p.x()<offset.x);
        int ytile = (p.y()-offset.y)/size.y;  ytile += (p.y()<offset.y);
        int ztile = (p.z()-offset.z)/size.z;  ztile += (p.z()<offset.z);
        int v = xtile + ytile + ztile;
        if (v & 1)
            for (int c = roi.chbegin;  c < roi.chend;  ++c)
                p[c] = color2[c];
        else
            for (int c = roi.chbegin;  c < roi.chend;  ++c)
                p[c] = color1[c];
    }
    return true;
}



bool
ImageBufAlgo::checker (ImageBuf &dst, int width, int height, int depth,
                       const float *color1, const float *color2,
                       int xoffset, int yoffset, int zoffset,
                       ROI roi, int nthreads)
{
    if (! IBAprep (roi, &dst))
        return false;
    bool ok;
    OIIO_DISPATCH_TYPES (ok, "checker", checker_, dst.spec().format,
                         dst, Dim3(width, height, depth), color1, color2,
                         Dim3(xoffset, yoffset, zoffset), roi, nthreads);
    return ok;
}



template<typename DSTTYPE, typename SRCTYPE>
static bool
convolve_ (ImageBuf &dst, const ImageBuf &src, const ImageBuf &kernel,
           bool normalize, ROI roi, int nthreads)
{
    if (nthreads != 1 && roi.npixels() >= 1000) {
        // Lots of pixels and request for multi threads? Parallelize.
        ImageBufAlgo::parallel_image (
            boost::bind(convolve_<DSTTYPE,SRCTYPE>, boost::ref(dst),
                        boost::cref(src), boost::cref(kernel), normalize,
                        _1 /*roi*/, 1 /*nthreads*/),
            roi, nthreads);
        return true;
    }

    // Serial case

    float scale = 1.0f;
    if (normalize) {
        scale = 0.0f;
        for (ImageBuf::ConstIterator<float> k (kernel); ! k.done(); ++k)
            scale += k[0];
        scale = 1.0f / scale;
    }

    float *sum = ALLOCA (float, roi.chend);
    ROI kroi = get_roi (kernel.spec());
    ImageBuf::Iterator<DSTTYPE> d (dst, roi);
    ImageBuf::ConstIterator<SRCTYPE> s (src, roi, ImageBuf::WrapClamp);
    for ( ; ! d.done();  ++d) {

        for (int c = roi.chbegin; c < roi.chend; ++c)
            sum[c] = 0.0f;

        for (ImageBuf::ConstIterator<float> k (kernel, kroi); !k.done(); ++k) {
            float kval = k[0];
            s.pos (d.x() + k.x(), d.y() + k.y(), d.z() + k.z());
            for (int c = roi.chbegin; c < roi.chend; ++c)
                sum[c] += kval * s[c];
        }
        
        for (int c = roi.chbegin; c < roi.chend; ++c)
            d[c] = scale * sum[c];
    }

    return true;
}



bool
ImageBufAlgo::convolve (ImageBuf &dst, const ImageBuf &src,
                        const ImageBuf &kernel, bool normalize,
                        ROI roi, int nthreads)
{
    if (! IBAprep (roi, &dst, &src))
        return false;
    if (dst.nchannels() != src.nchannels()) {
        dst.error ("channel number mismatch: %d vs. %d", 
                   dst.spec().nchannels, src.spec().nchannels);
        return false;
    }
    bool ok;
    OIIO_DISPATCH_TYPES2 (ok, "convolve", convolve_,
                          dst.spec().format, src.spec().format,
                          dst, src, kernel, normalize, roi, nthreads);
    return ok;
}



inline float binomial (int n, int k)
{
    float p = 1;
    for (int i = 1;  i <= k;  ++i)
        p *= float(n - (k-i)) / i;
    return p;
}


bool
ImageBufAlgo::make_kernel (ImageBuf &dst, string_view name,
                           float width, float height, float depth,
                           bool normalize)
{
    int w = std::max (1, (int)ceilf(width));
    int h = std::max (1, (int)ceilf(height));
    int d = std::max (1, (int)ceilf(depth));
    // Round up size to odd
    w |= 1;
    h |= 1;
    d |= 1;
    ImageSpec spec (w, h, 1 /*channels*/, TypeDesc::FLOAT);
    spec.depth = d;
    spec.x = -w/2;
    spec.y = -h/2;
    spec.z = -d/2;
    spec.full_x = spec.x;
    spec.full_y = spec.y;
    spec.full_z = spec.z;
    spec.full_width = spec.width;
    spec.full_height = spec.height;
    spec.full_depth = spec.depth;
    dst.reset (spec);

    if (Filter2D *filter = Filter2D::create (name, width, height)) {
        // Named continuous filter from filter.h
        for (ImageBuf::Iterator<float> p (dst);  ! p.done();  ++p)
            p[0] = (*filter)((float)p.x(), (float)p.y());
        delete filter;
    } else if (name == "binomial") {
        // Binomial filter
        float *wfilter = ALLOCA (float, width);
        for (int i = 0;  i < width;  ++i)
            wfilter[i] = binomial (width-1, i);
        float *hfilter = (height == width) ? wfilter : ALLOCA (float, height);
        if (height != width)
            for (int i = 0;  i < height;  ++i)
                hfilter[i] = binomial (height-1, i);
        float *dfilter = ALLOCA (float, depth);
        if (depth == 1)
            dfilter[0] = 1;
        else
            for (int i = 0;  i < depth;  ++i)
                dfilter[i] = binomial (depth-1, i);
        for (ImageBuf::Iterator<float> p (dst);  ! p.done();  ++p)
            p[0] = wfilter[p.x()-spec.x] * hfilter[p.y()-spec.y] * dfilter[p.z()-spec.z];
    } else {
        // No filter -- make a box
        float val = normalize ? 1.0f / ((w*h*d)) : 1.0f;
        for (ImageBuf::Iterator<float> p (dst);  ! p.done();  ++p)
            p[0] = val;
        dst.error ("Unknown kernel \"%s\"", name);
        return false;
    }
    if (normalize) {
        float sum = 0;
        for (ImageBuf::Iterator<float> p (dst);  ! p.done();  ++p)
            sum += p[0];
        for (ImageBuf::Iterator<float> p (dst);  ! p.done();  ++p)
            p[0] = p[0] / sum;
    }
    return true;
}



// Helper function for unsharp mask to perform the thresholding
static bool
threshold_to_zero (ImageBuf &dst, float threshold,
                   ROI roi, int nthreads)
{
    ASSERT (dst.spec().format.basetype == TypeDesc::FLOAT);

    if (nthreads != 1 && roi.npixels() >= 1000) {
        // Lots of pixels and request for multi threads? Parallelize.
        ImageBufAlgo::parallel_image (
            boost::bind(threshold_to_zero, boost::ref(dst), threshold,
                        _1 /*roi*/, 1 /*nthreads*/),
            roi, nthreads);
        return true;
    }

    // Serial case
    for (ImageBuf::Iterator<float> p (dst, roi);  ! p.done();  ++p)
        for (int c = roi.chbegin;  c < roi.chend;  ++c)
            if (fabsf(p[c]) < threshold)
                p[c] = 0.0f;

    return true;
}



bool
ImageBufAlgo::unsharp_mask (ImageBuf &dst, const ImageBuf &src,
                            string_view kernel, float width,
                            float contrast, float threshold,
                            ROI roi, int nthreads)
{
    if (! IBAprep (roi, &dst, &src,
            IBAprep_REQUIRE_SAME_NCHANNELS | IBAprep_NO_SUPPORT_VOLUME))
        return false;

    // Blur the source image, store in Blurry
    ImageSpec BlurrySpec = src.spec();
    BlurrySpec.set_format (TypeDesc::FLOAT);  // force float
    ImageBuf Blurry (BlurrySpec);

    if (kernel == "median") {
        median_filter (Blurry, src, ceilf(width), 0, roi, nthreads);
    } else {
        ImageBuf K;
        if (! make_kernel (K, kernel, width, width)) {
            dst.error ("%s", K.geterror());
            return false;
        }
        if (! convolve (Blurry, src, K, true, roi, nthreads)) {
            dst.error ("%s", Blurry.geterror());
            return false;
        }
    }

    // Compute the difference between the source image and the blurry
    // version.  (We store it in the same buffer we used for the difference
    // image.)
    ImageBuf &Diff (Blurry);
    bool ok = sub (Diff, src, Blurry, roi, nthreads);

    if (ok && threshold > 0.0f)
        ok = threshold_to_zero (Diff, threshold, roi, nthreads);

    // Scale the difference image by the contrast
    if (ok)
        ok = mul (Diff, Diff, contrast, roi, nthreads);
    if (! ok) {
        dst.error ("%s", Diff.geterror());
        return false;
    }

    // Add the scaled difference to the original, to get the final answer
    ok = add (dst, src, Diff, roi, nthreads);

    return ok;
}



template<class Rtype, class Atype>
static bool
median_filter_impl (ImageBuf &R, const ImageBuf &A, int width, int height,
                    ROI roi, int nthreads)
{
    if (nthreads != 1 && roi.npixels() >= 1000) {
        // Possible multiple thread case -- recurse via parallel_image
        ImageBufAlgo::parallel_image (
            boost::bind(median_filter_impl<Rtype,Atype>,
                        boost::ref(R), boost::cref(A),
                        width, height, _1 /*roi*/, 1 /*nthreads*/),
            roi, nthreads);
        return true;
    }

    if (width < 1)
        width = 1;
    if (height < 1)
        height = width;
    int w_2 = std::max (1, width/2);
    int h_2 = std::max (1, height/2);
    int windowsize = width*height;
    int nchannels = R.nchannels();
    float **chans = OIIO_ALLOCA (float*, nchannels);
    for (int c = 0;  c < nchannels;  ++c)
        chans[c] = OIIO_ALLOCA (float, windowsize);

    ImageBuf::ConstIterator<Atype> a (A, roi);
    for (ImageBuf::Iterator<Rtype> r (R, roi);  !r.done();  ++r) {
        a.rerange (r.x()-w_2, r.x()-w_2+width,
                   r.y()-h_2, r.y()-h_2+height,
                   r.z(), r.z()+1, ImageBuf::WrapClamp);
        int n = 0;
        for ( ;  ! a.done(); ++a) {
            if (a.exists()) {
                for (int c = 0;  c < nchannels;  ++c)
                    chans[c][n] = a[c];
                ++n;
            }
        }
        if (n) {
            int mid = n/2;
            for (int c = 0;  c < nchannels;  ++c) {
                std::sort (chans[c]+0, chans[c]+n);
                r[c] = chans[c][mid];
            }
        } else {
            for (int c = 0;  c < nchannels;  ++c)
                r[c] = 0.0f;
        }
    }
    return true;
}



bool
ImageBufAlgo::median_filter (ImageBuf &dst, const ImageBuf &src,
                             int width, int height,
                             ROI roi, int nthreads)
{
    if (! IBAprep (roi, &dst, &src,
            IBAprep_REQUIRE_SAME_NCHANNELS | IBAprep_NO_SUPPORT_VOLUME))
        return false;

    bool ok;
    OIIO_DISPATCH_COMMON_TYPES2 (ok, "median_filter",
                                 median_filter_impl, dst.spec().format,
                                 src.spec().format, dst, src,
                                 width, height, roi, nthreads);
    return ok;
}



// Helper function: fft of the horizontal rows
static bool
hfft_ (ImageBuf &dst, const ImageBuf &src, bool inverse, bool unitary,
       ROI roi, int nthreads)
{
    ASSERT (dst.spec().format.basetype == TypeDesc::FLOAT &&
            src.spec().format.basetype == TypeDesc::FLOAT &&
            dst.spec().nchannels == 2 && src.spec().nchannels == 2 &&
            dst.roi() == src.roi() &&
            (dst.storage() == ImageBuf::LOCALBUFFER || dst.storage() == ImageBuf::APPBUFFER) &&
            (src.storage() == ImageBuf::LOCALBUFFER || src.storage() == ImageBuf::APPBUFFER)
        );

    if (nthreads != 1 && roi.npixels() >= 1000) {
        // Lots of pixels and request for multi threads? Parallelize.
        ImageBufAlgo::parallel_image (
            boost::bind (hfft_, boost::ref(dst), boost::cref(src),
                         inverse, unitary,
                         _1 /*roi*/, 1 /*nthreads*/),
            roi, nthreads);
        return true;
    }

    // Serial case
    int width = roi.width();
    float rescale = sqrtf (1.0f / width);
    kissfft<float> F (width, inverse);
    for (int z = roi.zbegin;  z < roi.zend;  ++z) {
        for (int y = roi.ybegin;  y < roi.yend;  ++y) {
            std::complex<float> *s, *d;
            s = (std::complex<float> *)src.pixeladdr(roi.xbegin, y, z);
            d = (std::complex<float> *)dst.pixeladdr(roi.xbegin, y, z);
            F.transform (s, d);
            if (unitary)
                for (int x = 0;  x < width;  ++x)
                    d[x] *= rescale;
        }
    }
    return true;
}



bool
ImageBufAlgo::fft (ImageBuf &dst, const ImageBuf &src,
                   ROI roi, int nthreads)
{
    if (src.spec().depth > 1) {
        dst.error ("ImageBufAlgo::fft does not support volume images");
        return false;
    }
    if (! roi.defined())
        roi = roi_union (get_roi (src.spec()), get_roi_full (src.spec()));
    roi.chend = roi.chbegin+1;   // One channel only

    // Construct a spec that describes the result
    ImageSpec spec = src.spec();
    spec.width = spec.full_width = roi.width();
    spec.height = spec.full_height = roi.height();
    spec.depth = spec.full_depth = 1;
    spec.x = spec.full_x = 0;
    spec.y = spec.full_y = 0;
    spec.z = spec.full_z = 0;
    spec.set_format (TypeDesc::FLOAT);
    spec.channelformats.clear();
    spec.nchannels = 2;
    spec.channelnames.clear();
    spec.channelnames.push_back ("real");
    spec.channelnames.push_back ("imag");

    // And a spec that describes the transposed intermediate
    ImageSpec specT = spec;
    std::swap (specT.width, specT.height);
    std::swap (specT.full_width, specT.full_height);

    // Resize dst
    dst.reset (dst.name(), spec);

    // Copy src to a 2-channel (for "complex") float buffer
    ImageBuf A (spec);
    if (src.nchannels() < 2) {
        // If we're pasting fewer than 2 channels, zero out channel 1.
        ROI r = roi;
        r.chbegin = 1; r.chend = 2;
        zero (A, r);
    }
    if (! ImageBufAlgo::paste (A, 0, 0, 0, 0, src, roi, nthreads)) {
        dst.error ("%s", A.geterror());
        return false;
    }

    // FFT the rows (into temp buffer B).
    ImageBuf B (spec);
    hfft_ (B, A, false /*inverse*/, true /*unitary*/,
           get_roi(B.spec()), nthreads);

    // Transpose and shift back to A
    A.clear ();
    ImageBufAlgo::transpose (A, B, ROI::All(), nthreads);

    // FFT what was originally the columns (back to B)
    B.reset (specT);
    hfft_ (B, A, false /*inverse*/, true /*unitary*/,
           get_roi(A.spec()), nthreads);

    // Transpose again, into the dest
    ImageBufAlgo::transpose (dst, B, ROI::All(), nthreads);

    return true;
}



bool
ImageBufAlgo::ifft (ImageBuf &dst, const ImageBuf &src,
                    ROI roi, int nthreads)
{
    if (src.nchannels() != 2 || src.spec().format != TypeDesc::FLOAT) {
        dst.error ("ifft can only be done on 2-channel float images");
        return false;
    }
    if (src.spec().depth > 1) {
        dst.error ("ImageBufAlgo::ifft does not support volume images");
        return false;
    }

    if (! roi.defined())
        roi = roi_union (get_roi (src.spec()), get_roi_full (src.spec()));
    roi.chbegin = 0;
    roi.chend = 2;

    // Construct a spec that describes the result
    ImageSpec spec = src.spec();
    spec.width = spec.full_width = roi.width();
    spec.height = spec.full_height = roi.height();
    spec.depth = spec.full_depth = 1;
    spec.x = spec.full_x = 0;
    spec.y = spec.full_y = 0;
    spec.z = spec.full_z = 0;
    spec.set_format (TypeDesc::FLOAT);
    spec.channelformats.clear();
    spec.nchannels = 2;
    spec.channelnames.clear();
    spec.channelnames.push_back ("real");
    spec.channelnames.push_back ("imag");

    // Inverse FFT the rows (into temp buffer B).
    ImageBuf B (spec);
    hfft_ (B, src, true /*inverse*/, true /*unitary*/,
           get_roi(B.spec()), nthreads);

    // Transpose and shift back to A
    ImageBuf A;
    ImageBufAlgo::transpose (A, B, ROI::All(), nthreads);

    // Inverse FFT what was originally the columns (back to B)
    B.reset (A.spec());
    hfft_ (B, A, true /*inverse*/, true /*unitary*/,
           get_roi(A.spec()), nthreads);

    // Transpose again, into the dst, in the process throw out the
    // imaginary part and go back to a single (real) channel.
    spec.nchannels = 1;
    spec.channelnames.clear ();
    spec.channelnames.push_back ("R");
    dst.reset (dst.name(), spec);
    ROI Broi = get_roi(B.spec());
    Broi.chend = 1;
    ImageBufAlgo::transpose (dst, B, Broi, nthreads);

    return true;
}



template<class Rtype, class Atype>
static bool
polar_to_complex_impl (ImageBuf &R, const ImageBuf &A, ROI roi, int nthreads)
{
    if (nthreads != 1 && roi.npixels() >= 1000) {
        // Possible multiple thread case -- recurse via parallel_image
        ImageBufAlgo::parallel_image (
            boost::bind(polar_to_complex_impl<Rtype,Atype>, boost::ref(R), boost::cref(A),
                        _1 /*roi*/, 1 /*nthreads*/),
            roi, nthreads);
        return true;
    }

    ImageBuf::ConstIterator<Atype> a (A, roi);
    for (ImageBuf::Iterator<Rtype> r (R, roi);  !r.done();  ++r, ++a) {
        float amp = a[0];
        float phase = a[1];
        float sine, cosine;
        sincos (phase, &sine, &cosine);
        r[0] = amp * cosine;
        r[1] = amp * sine;
    }
    return true;
}



template<class Rtype, class Atype>
static bool
complex_to_polar_impl (ImageBuf &R, const ImageBuf &A, ROI roi, int nthreads)
{
    if (nthreads != 1 && roi.npixels() >= 1000) {
        // Possible multiple thread case -- recurse via parallel_image
        ImageBufAlgo::parallel_image (
            boost::bind(complex_to_polar_impl<Rtype,Atype>, boost::ref(R), boost::cref(A),
                        _1 /*roi*/, 1 /*nthreads*/),
            roi, nthreads);
        return true;
    }

    ImageBuf::ConstIterator<Atype> a (A, roi);
    for (ImageBuf::Iterator<Rtype> r (R, roi);  !r.done();  ++r, ++a) {
        float real = a[0];
        float imag = a[1];
        float phase = std::atan2 (imag, real);
        if (phase < 0.0f)
            phase += float (M_TWO_PI);
        r[0] = hypotf (real, imag);
        r[1] = phase;
    }
    return true;
}



bool
ImageBufAlgo::polar_to_complex (ImageBuf &dst, const ImageBuf &src,
                             ROI roi, int nthreads)
{
    if (src.nchannels() != 2) {
        dst.error ("polar_to_complex can only be done on 2-channel");
        return false;
    }

    if (! IBAprep (roi, &dst, &src))
        return false;
    if (dst.nchannels() != 2) {
        dst.error ("polar_to_complex can only be done on 2-channel");
        return false;
    }
    bool ok;
    OIIO_DISPATCH_COMMON_TYPES2 (ok, "polar_to_complex",
                                 polar_to_complex_impl, dst.spec().format,
                                 src.spec().format, dst, src, roi, nthreads);
    return ok;
}




bool
ImageBufAlgo::complex_to_polar (ImageBuf &dst, const ImageBuf &src,
                             ROI roi, int nthreads)
{
    if (src.nchannels() != 2) {
        dst.error ("complex_to_polar can only be done on 2-channel");
        return false;
    }

    if (! IBAprep (roi, &dst, &src))
        return false;
    if (dst.nchannels() != 2) {
        dst.error ("complex_to_polar can only be done on 2-channel");
        return false;
    }
    bool ok;
    OIIO_DISPATCH_COMMON_TYPES2 (ok, "complex_to_polar",
                                 complex_to_polar_impl, dst.spec().format,
                                 src.spec().format, dst, src, roi, nthreads);
    return ok;
}




// Helper for fillholes_pp: for any nonzero alpha pixels in dst, divide
// all components by alpha.
static bool
divide_by_alpha (ImageBuf &dst, ROI roi, int nthreads)
{
    if (nthreads != 1 && roi.npixels() >= 1000) {
        // Lots of pixels and request for multi threads? Parallelize.
        ImageBufAlgo::parallel_image (
            boost::bind(divide_by_alpha, boost::ref(dst),
                        _1 /*roi*/, 1 /*nthreads*/),
            roi, nthreads);
        return true;
    }

    // Serial case
    const ImageSpec &spec (dst.spec());
    ASSERT (spec.format == TypeDesc::FLOAT);
    int nc = spec.nchannels;
    int ac = spec.alpha_channel;
    for (ImageBuf::Iterator<float> d (dst, roi);  ! d.done();  ++d) {
        float alpha = d[ac];
        if (alpha != 0.0f) {
            for (int c = 0; c < nc; ++c)
                d[c] = d[c] / alpha;
        }
    }
    return true;
}



bool
ImageBufAlgo::fillholes_pushpull (ImageBuf &dst, const ImageBuf &src,
                                  ROI roi, int nthreads)
{
    if (! IBAprep (roi, &dst, &src))
        return false;
    const ImageSpec &dstspec (dst.spec());
    if (dstspec.nchannels != src.nchannels()) {
        dst.error ("channel number mismatch: %d vs. %d", 
                   dstspec.nchannels, src.spec().nchannels);
        return false;
    }
    if (dst.spec().depth > 1 || src.spec().depth > 1) {
        dst.error ("ImageBufAlgo::fillholes_pushpull does not support volume images");
        return false;
    }
    if (dstspec.alpha_channel < 0 ||
        dstspec.alpha_channel != src.spec().alpha_channel) {
        dst.error ("Must have alpha channels");
        return false;
    }

    // We generate a bunch of temp images to form an image pyramid.
    // These give us a place to stash them and make sure they are
    // auto-deleted when the function exits.
    std::vector<boost::shared_ptr<ImageBuf> > pyramid;

    // First, make a writeable copy of the original image (converting
    // to float as a convenience) as the top level of the pyramid.
    ImageSpec topspec = src.spec();
    topspec.set_format (TypeDesc::FLOAT);
    ImageBuf *top = new ImageBuf (topspec);
    paste (*top, topspec.x, topspec.y, topspec.z, 0, src);
    pyramid.push_back (boost::shared_ptr<ImageBuf>(top));

    // Construct the rest of the pyramid by successive x/2 resizing and
    // then dividing nonzero alpha pixels by their alpha (this "spreads
    // out" the defined part of the image).
    int w = src.spec().width, h = src.spec().height;
    while (w > 1 || h > 1) {
        w = std::max (1, w/2);
        h = std::max (1, h/2);
        ImageSpec smallspec (w, h, src.nchannels(), TypeDesc::FLOAT);
        ImageBuf *small = new ImageBuf (smallspec);
        ImageBufAlgo::resize (*small, *pyramid.back(), "triangle");
        divide_by_alpha (*small, get_roi(smallspec), nthreads);
        pyramid.push_back (boost::shared_ptr<ImageBuf>(small));
        //debug small->save();
    }

    // Now pull back up the pyramid by doing an alpha composite of level
    // i over a resized level i+1, thus filling in the alpha holes.  By
    // time we get to the top, pixels whose original alpha are
    // unchanged, those with alpha < 1 are replaced by the blended
    // colors of the higher pyramid levels.
    for (int i = (int)pyramid.size()-2;  i >= 0;  --i) {
        ImageBuf &big(*pyramid[i]), &small(*pyramid[i+1]);
        ImageBuf blowup (big.spec());
        ImageBufAlgo::resize (blowup, small, "triangle");
        ImageBufAlgo::over (big, big, blowup);
        //debug big.save (Strutil::format ("after%d.exr", i));
    }

    // Now copy the completed base layer of the pyramid back to the
    // original requested output.
    paste (dst, dstspec.x, dstspec.y, dstspec.z, 0, *pyramid[0]);

    return true;
}


}
OIIO_NAMESPACE_EXIT
