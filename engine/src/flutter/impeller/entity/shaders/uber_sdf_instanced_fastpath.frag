// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

precision mediump float;

#include <impeller/color.glsl>
#include <impeller/constants.glsl>

in vec2 v_position;
in vec2 v_half_size;
in vec4 v_radii;
in vec4 v_color;
in float v_half_stroke;
in float v_stroke_flags;
in float v_aa_pixels;
in vec2 v_pixel_size;

out vec4 frag_color;

// Fast perceptual gamma approximation matching current UberSDF
float gammaCorrectedAlpha(float alpha, vec3 foreground_rgb) {
  float alpha_dark = 1.0 - sqrt(1.0 - alpha);
  float alpha_light = sqrt(alpha);
  float luma = dot(foreground_rgb, vec3(0.2126, 0.7152, 0.0722));
  return mix(alpha_dark, alpha_light, luma);
}

void main() {
  // 1. Quadrant Corner Radius Selection (Branch-Free Ternary / Conditional Move):
  vec4 r = v_radii;
  r.xy = (v_position.x > 0.0) ? r.xy : r.zw;
  float radius = (v_position.y > 0.0) ? r.x : r.y;

  // 2. Zero-Branch Unified Analytical Distance Formula:
  // Exactly evaluates Rectangles (r=0), Circles (b=r, r=r), and RRects without branching.
  vec2 q = abs(v_position) - v_half_size + radius;
  float sdf = min(max(q.x, q.y), 0.0) + length(max(q, 0.0)) - radius;

  // 3. Stroking:
  if (v_half_stroke > 0.0) {
    sdf = abs(sdf) - v_half_stroke;
  }

  // 4. Directional Pixel-Size Normalization:
  vec2 d = abs(abs(v_position) - v_half_size);
  float pixel_size = (d.x < d.y) ? v_pixel_size.x : v_pixel_size.y;
  if (radius > 0.0) {
    vec2 corner_center = v_half_size - radius;
    vec2 cq = abs(v_position) - corner_center;
    if (cq.x > 0.0 && cq.y > 0.0) {
      pixel_size = length(normalize(cq) * v_pixel_size);
    }
  }

  // 5. Antialiased Coverage:
  float alpha = clamp(0.5 - sdf / max(v_aa_pixels * pixel_size, 1e-4), 0.0, 1.0);

  // 6. Perceptual Gamma Correction:
  if (alpha < 1.0) {
    alpha = gammaCorrectedAlpha(alpha, v_color.rgb);
  }

  frag_color = IPPremultiply(vec4(v_color.rgb, v_color.a * alpha));
}
