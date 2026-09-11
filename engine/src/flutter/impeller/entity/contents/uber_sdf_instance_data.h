// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FLUTTER_IMPELLER_ENTITY_CONTENTS_UBER_SDF_INSTANCE_DATA_H_
#define FLUTTER_IMPELLER_ENTITY_CONTENTS_UBER_SDF_INSTANCE_DATA_H_

#include <cstdint>

namespace impeller {

enum class UberSDFTier : uint32_t {
  kFastpath = 0,  // Rect, RRect, Circle, Line (Zero-branch ALU)
  kComplex = 1,   // Oval, Rounded Superellipse (Fallback or standalone)
};

struct alignas(16) UberSDFInstanceData {
  // Slot 0: 2D Affine Basis Matrix (16 bytes)
  // Maps local unit dimensions into pass space (scale, rotation, shear).
  // basis[0] = m00, basis[1] = m01 (basis X)
  // basis[2] = m10, basis[3] = m11 (basis Y)
  float basis[4];

  // Slot 1: Effective 2D Affine Translation & Depth (16 bytes)
  // [tx_effective, ty_effective, depth, flags]
  // tx/ty: entity translation with shape center already folded in.
  // depth: clip depth scaled by Entity::kDepthEpsilon.
  // flags: bit 0 = stroked (0=fill, 1=stroke), bit 1-2 = join style.
  float translation_and_depth[4];

  // Slot 2: Local Half-Size & Stroke Dimensions (16 bytes)
  // [half_width, half_height, stroke_width, antialias_pixels]
  float size_and_stroke[4];

  // Slot 3: Corner Radii (16 bytes)
  // [r_top_left, r_top_right, r_bottom_right, r_bottom_left]
  // Rect: (0, 0, 0, 0)
  // Circle: (radius, radius, radius, radius)
  // RRect: (r_tl, r_tr, r_br, r_bl)
  float radii[4];

  // Slot 4: Premultiplied / Unpremultiplied RGBA Color (16 bytes)
  // [red, green, blue, alpha]
  float color[4];

  // Slot 5: Pipeline & Gradient Control / Reserved (16 bytes)
  // [tier, gradient_id, miter_limit, unused]
  float extra_params[4];
};

static_assert(sizeof(UberSDFInstanceData) == 96,
              "Instance size must be exactly 96 bytes");
static_assert(alignof(UberSDFInstanceData) == 16,
              "Instance alignment must be exactly 16 bytes");

}  // namespace impeller

#endif  // FLUTTER_IMPELLER_ENTITY_CONTENTS_UBER_SDF_INSTANCE_DATA_H_
