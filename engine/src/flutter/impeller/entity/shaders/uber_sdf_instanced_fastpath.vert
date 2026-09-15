// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <impeller/types.glsl>

uniform FrameInfo {
  mat4 projection;  // Pass orthographic projection matrix
}
frame_info;

// Binding 0: Static unit quad geometry (mesh rate)
in vec2 unit_position;  // Spans [-1.0, 1.0]

// Binding 1: Compact 64-byte instance attributes (instance rate)
in vec4 instance_basis;              // [m00, m01, m10, m11]
in vec4 instance_translation_depth;  // [tx, ty, depth, stroke_width]
in vec4 instance_size_flags_color;   // [half_w, half_h, flags, packed_color]
in vec4 instance_radii;              // [r_tl, r_tr, r_br, r_bl]

out vec2 v_position;
out vec2 v_half_size;
out vec4 v_radii;
out vec4 v_color;
out float v_half_stroke;
out float v_aa_pixels;
out vec2 v_pixel_size;

void main() {
  vec2 half_size = instance_size_flags_color.xy;
  float stroke_width = instance_translation_depth.w;
  float flags = instance_size_flags_color.z;
  bool is_stroked = (int(flags) & 1) != 0;
  float aa_pixels = 2.0;

  // 1. Analytical Device Pixel Size:
  // Derived directly from the affine basis matrix columns.
  vec2 basis_x = instance_basis.xy;
  vec2 basis_y = instance_basis.zw;
  float scale_x = length(basis_x);
  float scale_y = length(basis_y);
  vec2 pixel_size = vec2(scale_x > 0.0 ? 1.0 / scale_x : 1.0,
                         scale_y > 0.0 ? 1.0 / scale_y : 1.0);

  // 2. Bounding Quad Expansion:
  // Account for stroke width (clamped to 1 device pixel minimum) and AA margin.
  vec2 stroke_padding = vec2(0.0);
  if (is_stroked) {
    stroke_padding = 0.5 * max(vec2(stroke_width), pixel_size);
  }
  vec2 aa_padding = aa_pixels * pixel_size;
  vec2 local_expansion = half_size + stroke_padding + aa_padding;

  // 3. Local Vertex Coordinates:
  vec2 local_pos = unit_position * local_expansion;

  // 4. Pass-Space Screen Coordinates:
  mat2 linear_basis = mat2(basis_x, basis_y);
  vec2 pass_pos = linear_basis * local_pos + instance_translation_depth.xy;

  // 5. Normalized Device Coordinates (NDC) with Depth:
  // Projection converts screen-space (x, y) to NDC [-1, 1].
  // instance_translation_depth.z is clip_depth * kDepthEpsilon.
  vec4 projected = frame_info.projection * vec4(pass_pos, 0.0, 1.0);
  projected.z = instance_translation_depth.z * projected.w;
  gl_Position = projected;

  // 6. Color Unpacking:
#if defined(IMPELLER_TARGET_OPENGLES) && !defined(IMPELLER_TARGET_OPENGLES3)
  v_color = vec4(1.0);
#else
  uint color_bits = floatBitsToUint(instance_size_flags_color.w);
  v_color = unpackUnorm4x8(color_bits);
#endif

  // 7. Export Varyings:
  v_position = local_pos;
  v_half_size = half_size;
  v_radii = instance_radii;
  v_half_stroke = is_stroked ? (stroke_width * 0.5) : 0.0;
  v_aa_pixels = aa_pixels;
  v_pixel_size = pixel_size;
}
