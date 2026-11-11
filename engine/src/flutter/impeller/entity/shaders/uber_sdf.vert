// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Vertex shader for the UberSDF fragment shaders (`uber_sdf.frag` and
// `uber_sdf_ssbo.frag`).
//
// The math here involves absolute local coordinates, so it is done per vertex
// in highp rather than per fragment, where values default to mediump (fp16 on
// many mobile GPUs). Everything computed here is affine in `position`, so
// interpolating it is exact.

#include <impeller/types.glsl>

uniform FrameInfo {
  mat4 mvp;
  /// Rows of the 2x2 matrix that maps (position - gradient_origin) to gradient
  /// space (see `v_gradient`).
  ///   - Linear gradient: xy = delta / dot(delta, delta), zw = 0.
  ///   - Radial gradient: xy = (1 / radius, 0), zw = (0, 1 / radius).
  ///   - Solid color: unused (0).
  vec4 gradient_matrix;
  /// The center of the shape in local coordinates.
  vec2 center;
  /// The linear gradient start point or radial gradient center in local
  /// coordinates.
  vec2 gradient_origin;
}
frame_info;

in vec2 position;

/// The local position relative to the shape's center.
out vec2 v_position;
/// The position in gradient space.
///   - Linear gradient: x is the gradient parameter `t`, y is 0.
///   - Radial gradient: the offset from the gradient center in units of the
///     radius, so `t` is its length.
out vec2 v_gradient;

void main() {
  gl_Position = frame_info.mvp * vec4(position, 0.0, 1.0);
  v_position = position - frame_info.center;
  vec2 from_origin = position - frame_info.gradient_origin;
  v_gradient = vec2(dot(from_origin, frame_info.gradient_matrix.xy),
                    dot(from_origin, frame_info.gradient_matrix.zw));
}
