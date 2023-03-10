// Copyright 2011 Google Inc. All Rights Reserved.
//
// This code is licensed under the same terms as WebM:
//  Software License Agreement:  http://www.webmproject.org/license/software/
//  Additional IP Rights Grant:  http://www.webmproject.org/license/additional/
// -----------------------------------------------------------------------------
//
// Quantize levels for specified number of quantization-levels ([2, 256]).
// Min and max values are preserved (usual 0 and 255 for alpha plane).
//
// Author: skal@google.com (Pascal Massimino)

#include <assert.h>
#include <math.h>    // for sqrt()

#include "./alpha.h"

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

#define NUM_SYMBOLS     256

#define MAX_ITER  6             // Maximum number of convergence steps.
#define ERROR_THRESHOLD 1e-4    // MSE stopping criterion.

// -----------------------------------------------------------------------------
// Quantize levels.

int QuantizeLevels(uint8_t* data, int width, int height,
                   int num_levels, float* mse) {
  int freq[NUM_SYMBOLS] = { 0 };
  int q_level[NUM_SYMBOLS] = { 0 };
  double inv_q_level[NUM_SYMBOLS] = { 0 };
  int min_s = 255, max_s = 0;
  const size_t data_size = height * width;
  size_t n = 0;
  int s, num_levels_in, iter;
  double last_err = 1.e38, err = 0.;

  if (data == NULL) {
    return 0;
  }

  if (width <= 0 || height <= 0) {
    return 0;
  }

  if (num_levels < 2 || num_levels > 256) {
    return 0;
  }

  num_levels_in = 0;
  for (n = 0; n < data_size; ++n) {
    num_levels_in += (freq[data[n]] == 0);
    if (min_s > data[n]) min_s = data[n];
    if (max_s < data[n]) max_s = data[n];
    ++freq[data[n]];
  }

  if (num_levels_in <= num_levels) {
    if (mse) *mse = 0.;
    return 1;   // nothing to do !
  }

  // Start with uniformly spread centroids.
  for (s = 0; s < num_levels; ++s) {
    inv_q_level[s] = min_s + (double)(max_s - min_s) * s / (num_levels - 1);
  }

  // Fixed values. Won't be changed.
  q_level[min_s] = 0;
  q_level[max_s] = num_levels - 1;
  assert(inv_q_level[0] == min_s);
  assert(inv_q_level[num_levels - 1] == max_s);

  // k-Means iterations.
  for (iter = 0; iter < MAX_ITER; ++iter) {
    double err_count;
    double q_sum[NUM_SYMBOLS] = { 0 };
    double q_count[NUM_SYMBOLS] = { 0 };
    int slot = 0;

    // Assign classes to representatives.
    for (s = min_s; s <= max_s; ++s) {
      // Keep track of the nearest neighbour 'slot'
      while (slot < num_levels - 1 &&
             2 * s > inv_q_level[slot] + inv_q_level[slot + 1]) {
        ++slot;
      }
      if (freq[s] > 0) {
        q_sum[slot] += s * freq[s];
        q_count[slot] += freq[s];
      }
      q_level[s] = slot;
    }

    // Assign new representatives to classes.
    if (num_levels > 2) {
      for (slot = 1; slot < num_levels - 1; ++slot) {
        const double count = q_count[slot];
        if (count > 0.) {
          inv_q_level[slot] = q_sum[slot] / count;
        }
      }
    }

    // Compute convergence error.
    err = 0.;
    err_count = 0.;
    for (s = min_s; s <= max_s; ++s) {
      const double error = s - inv_q_level[q_level[s]];
      err += freq[s] * error * error;
      err_count += freq[s];
    }
    if (err_count > 0.) err /= err_count;

    // Check for convergence: we stop as soon as the error is no
    // longer improving.
    if (last_err - err < ERROR_THRESHOLD) break;
    last_err = err;
  }

  // Remap the alpha plane to quantized values.
  {
    // double->int rounding operation can be costly, so we do it
    // once for all before remapping. We also perform the data[] -> slot
    // mapping, while at it (avoid one indirection in the final loop).
    uint8_t map[NUM_SYMBOLS];
    int s;
    for (s = min_s; s <= max_s; ++s) {
      const int slot = q_level[s];
      map[s] = (uint8_t)(inv_q_level[slot] + .5);
    }
    // Final pass.
    for (n = 0; n < data_size; ++n) {
      data[n] = map[data[n]];
    }
  }

  // Compute final mean squared error if needed.
  if (mse) {
    *mse = (float)sqrt(err);
  }

  return 1;
}

#if defined(__cplusplus) || defined(c_plusplus)
}    // extern "C"
#endif
