// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FLUTTER_IMPELLER_ENTITY_CONTENTS_UBER_SDF_INSTANCE_DATA_H_
#define FLUTTER_IMPELLER_ENTITY_CONTENTS_UBER_SDF_INSTANCE_DATA_H_

#include <algorithm>
#include <cstdint>
#include "impeller/geometry/color.h"

namespace impeller {

enum class UberSDFTier : uint32_t {
  kFastpath = 0,  // Rect, RRect, Circle, Line (Zero-branch ALU)
  kComplex = 1,   // Oval, Rounded Superellipse (Fallback or standalone)
};

/// @brief Cache-line aligned (64 bytes) per-instance data for UberSDF
/// instancing.
///
/// Exactly 64 bytes in size and 16-byte aligned, matching the L1 cache line
/// size of modern mobile and desktop GPUs.
struct alignas(16) UberSDFInstanceData {
  // Slot 0: 2D Affine Linear Basis Matrix (16 bytes)
  // [m00, m01, m10, m11]
  float basis[4];

  // Slot 1: Effective 2D Affine Translation, Depth, and Stroke Width (16 bytes)
  // [tx_effective, ty_effective, depth_normalized, stroke_width]
  float translation_and_depth[4];

  // Slot 2: Local Dimensions, Flags, and Packed RGBA8 Color (16 bytes)
  // [half_width, half_height, flags (as float), packed_color (as uint32)]
  float half_size[2];
  float flags;
  uint32_t color;

  // Slot 3: Quadrant Corner Radii (16 bytes)
  // [r_top_left, r_top_right, r_bottom_right, r_bottom_left]
  float radii[4];

  static inline uint32_t PackColorRGBA8(const Color& c) {
    uint32_t r =
        static_cast<uint32_t>(std::clamp(c.red * 255.0f, 0.0f, 255.0f));
    uint32_t g =
        static_cast<uint32_t>(std::clamp(c.green * 255.0f, 0.0f, 255.0f));
    uint32_t b =
        static_cast<uint32_t>(std::clamp(c.blue * 255.0f, 0.0f, 255.0f));
    uint32_t a =
        static_cast<uint32_t>(std::clamp(c.alpha * 255.0f, 0.0f, 255.0f));
    return (a << 24) | (b << 16) | (g << 8) | r;
  }
};

static_assert(sizeof(UberSDFInstanceData) == 64,
              "UberSDFInstanceData must be exactly 64 bytes (1 cache line)");
static_assert(alignof(UberSDFInstanceData) == 16,
              "UberSDFInstanceData must be 16-byte aligned");

}  // namespace impeller

#endif  // FLUTTER_IMPELLER_ENTITY_CONTENTS_UBER_SDF_INSTANCE_DATA_H_
