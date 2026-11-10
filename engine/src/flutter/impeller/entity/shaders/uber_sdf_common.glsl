// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Shared body of the UberSDF shaders, included by `uber_sdf.frag` and
// `uber_sdf_ssbo.frag`. Each entrypoint supplies only `getGradientColor()`.

precision mediump float;

#include <impeller/color.glsl>
#include <impeller/constants.glsl>
#include <impeller/texture.glsl>
#include <impeller/types.glsl>

#include "rse_sdf.glsl"
#include "sdf_functions.glsl"
#include "sdf_utils.glsl"

/// The width in device pixels over which to apply antialiasing.
///
/// Must match `UberSDFParameters::kAntialiasPixels`, which pads the drawn
/// geometry to make room for it.
const float kAntialiasPixels = 1.0;

uniform FragInfo {
  // FragInfo fields are sorted by size (vec4 -> vec2 -> float) to optimize
  // uniform register usage.
  //
  // Fields that hold local coordinates (or that are combined with them before
  // the result becomes small) are highp. mediump (fp16) only has 11 bits of
  // precision and overflows past 65504, so it can't resolve sub-pixel distances
  // at typical local coordinate magnitudes, and squaring values above ~256
  // overflows. The rest of the shader stays mediump where it's safe.

  // ===========================================================================
  // vec4 fields
  // ===========================================================================

  /// The RGBA color of the shape (or paint opacity in color.a for gradients).
  vec4 color;
  /// Corner radii for rounded rects (top-left, top-right, bottom-left,
  /// bottom-right), or the circular cap radii for rounded superellipses in
  /// radii.xy (top octant in x, right octant in y).
  vec4 radii;
  /// Gradient parameters:
  ///   - Linear gradient:
  ///       xy: Start point in local coordinates, relative to the shape's
  ///           center.
  ///       zw: Delta vector (end - start) in local coordinates.
  ///   - Radial gradient:
  ///       xy: Center point in local coordinates, relative to the shape's
  ///           center.
  ///       zw: Unused (0.0).
  highp vec4 gradient_coords;

  // ===========================================================================
  // vec2 fields
  // ===========================================================================

  // --- General Shape Geometry ---
  /// The half-dimensions of the shape (half-width, half-height).
  highp vec2 size;
  /// The size of a device pixel in local coordinates.
  vec2 pixel_size;

  // --- Superellipse Parameters ---
  /// The exponent degree (n_x, n_y) of the superellipse curvature.
  vec2 superellipse_degree;
  /// The angular span of the corner circular arc transitions for rounded
  /// superellipses.
  vec2 angle_span;
  /// The center of the corner transition circle for the top octant of a
  /// rounded superellipse.
  vec2 circle_center_top;
  /// The center of the corner transition circle for the right octant of a
  /// rounded superellipse.
  vec2 circle_center_right;

  // ===========================================================================
  // float fields
  // ===========================================================================

  // --- General Configuration ---
  /// The shape type:
  ///   0: Circle
  ///   1: Rect
  ///   2: Oval
  ///   3: RoundRect
  ///   4: Rounded Superellipse (must have uniform circular corner radii)
  float type;
  /// The type of color source:
  ///   0: Solid color
  ///   1: Linear gradient
  ///   2: Radial gradient
  float color_source_type;

  // --- Stroke Parameters ---
  /// Whether the shape is stroked (1.0) or filled (0.0).
  float stroked;
  /// The width of the stroke.
  float stroke_width;
  /// The join style for the stroke:
  ///   0: Miter
  ///   1: Bevel
  ///   2: Round
  float stroke_join;

  // --- Gradient Parameters ---
  /// The tile mode for gradient sampling:
  ///   0: Clamp
  ///   1: Repeat
  ///   2: Mirror
  ///   3: Decal
  float tile_mode;
  /// Inverse gradient length:
  ///   - Linear gradient: 1.0 / dot(delta, delta)
  ///   - Radial gradient: 1.0 / radius
  highp float inv_gradient_length;

  // --- Variant-Specific Gradient Parameters ---
  // Exactly one of these is used by any given entrypoint; the other is unset.

  /// Half the size of a single gradient texel in normalized texture
  /// coordinates along the gradient ramp (x axis).
  ///
  /// Only used by the gradient ramp texture variant.
  float half_texel;
  /// The number of gradient stops in the color data storage buffer.
  ///
  /// Only used by the storage buffer variant.
  float colors_length;
}
frag_info;

out vec4 frag_color;

// The fragment position in local coordinates, relative to the shape's center.
// `UberSDFGeometry::GetCenteredPositionBuffer` folds the translation to the
// center into the vertex transform, so this is interpolated directly in
// center-relative coordinates.
highp in vec2 v_position;

// Looks up the gradient color at gradient position `t`.
//
// `t` is untiled, so implementations apply frag_info.tile_mode themselves.
// Returns an unpremultiplied color, ignoring frag_info.color.a, which the
// calling getColor() applies below.
vec4 getGradientColor(float t);

// Gets the color to use at v_position based on frag_info properties.
vec4 getColor() {
  vec4 color;
  if (frag_info.color_source_type < 0.5) {
    // Solid color
    color = frag_info.color;
  } else {
    // Computed in highp rather than with the mediump shader library helpers:
    // the position is in local coordinates, and the products below overflow
    // mediump for gradients longer than ~256 units.
    highp vec2 start_to_position = v_position - frag_info.gradient_coords.xy;
    highp float t;
    if (frag_info.color_source_type < 1.5) {
      // Linear gradient
      t = dot(start_to_position, frag_info.gradient_coords.zw) *
          frag_info.inv_gradient_length;
    } else {
      // Radial gradient
      t = length(start_to_position) * frag_info.inv_gradient_length;
    }
    vec4 gradient_color = getGradientColor(t);
    color = vec4(gradient_color.rgb, gradient_color.a * frag_info.color.a);
  }
  return color;
}

// The distance functions below take highp positions and sizes and do their
// first subtraction or length() in highp. Near the edge, the result is a small
// difference of large local coordinate values, which mediump can't resolve
// (and squaring those values overflows mediump). The returned distances are
// small near the edge, so they can be mediump.

float distanceFromCircle(highp vec2 p, highp float radius) {
  return length(p) - radius;
}

float distanceFromRect(highp vec2 p, highp vec2 b) {
  highp vec2 d = abs(p) - b;
  return length(max(d, 0.0)) + min(max(d.x, d.y), 0.0);
}

float distanceFromOval(highp vec2 position, highp vec2 semi_axes) {
  position = abs(position);

  // Find the angle `w` of the closest point on the ellipse with Newton's
  // method. The iteration is scale invariant, so the first steps run in
  // mediump on coordinates normalized by the larger semi-axis, where their
  // intermediate products can't overflow.
  highp float scale = 1.0 / max(semi_axes.x, semi_axes.y);
  highp vec2 p_normalized = position * scale;
  highp vec2 ab_normalized = semi_axes * scale;
  vec2 p = p_normalized;
  vec2 ab = ab_normalized;
  vec2 q = ab * (p - ab);
  float w = (q.x < q.y) ? 1.570796327 : 0.0;
  for (int i = 0; i < 4; i++) {
    vec2 cs = vec2(cos(w), sin(w));
    vec2 u = ab * vec2(cs.x, cs.y);
    vec2 v = ab * vec2(-cs.y, cs.x);
    w = w + dot(p - u, v) / (dot(p - u, u) + dot(v, v));
  }

  // A final Newton step in highp refines the angle to full precision.
  highp float hw = w;
  highp vec2 hcs = vec2(cos(hw), sin(hw));
  highp vec2 hu = ab_normalized * hcs;
  highp vec2 hv = ab_normalized * vec2(-hcs.y, hcs.x);
  hw = hw +
       dot(p_normalized - hu, hv) / (dot(p_normalized - hu, hu) + dot(hv, hv));

  // Near the edge, both the distance and the inside test are small differences
  // of large values, so they are computed in highp as well. The inside test is
  // (x/a)^2 + (y/b)^2 > 1 without the divisions.
  highp float d = length(position - semi_axes * vec2(cos(hw), sin(hw)));
  highp vec2 scaled_position = position * semi_axes.yx;
  highp float axes_product = semi_axes.x * semi_axes.y;
  return (dot(scaled_position, scaled_position) > axes_product * axes_product)
             ? d
             : -d;
}

float distanceFromRoundedRect(in highp vec2 p, in highp vec2 b, in vec4 r) {
  r.xy = (p.x > 0.0) ? r.xy : r.zw;
  r.x = (p.y > 0.0) ? r.x : r.y;
  highp vec2 q = abs(p) - b + r.x;
  return min(max(q.x, q.y), 0.0) + length(max(q, 0.0)) - r.x;
}

float distanceFromChamferRect(highp vec2 p,
                              highp vec2 half_size,
                              float chamfer_size) {
  p = abs(p);
  float d1 = max(p.x - half_size.x, p.y - half_size.y);
  float d2 =
      (p.x + p.y - half_size.x - half_size.y + chamfer_size) * kHalfSqrtTwo;
  return max(d1, d2);
}

float distanceFromRoundedSuperellipse(highp vec2 p,
                                      vec2 degree,
                                      highp vec2 size,
                                      vec2 radii,
                                      vec2 cos_angle_span,
                                      vec2 circle_center_top,
                                      vec2 circle_center_right) {
  // Do work in the first quadrant to simply things.
  p = abs(p);

  // Transition line offset dividing top and right octants.
  highp float c = size.x - size.y;

  // Declare all RSE params for a single octant.
  float se_degree, cos_span, radius, axis_length;
  vec2 circle_center;

  // 'p' in the coordinate system of the octant.
  highp vec2 p_oct;

  // We split the quadrant along the diagonal of the transition (p.y + c ==
  // p.x). This allows us to grab the correct set of parameters for the
  // "top" and "right" halves of the corner.
  if (p.y + c > p.x) {
    p_oct = p + vec2(0.0, c);
    se_degree = degree.x;
    cos_span = cos_angle_span.x;
    radius = radii.x;
    circle_center = circle_center_top;
    axis_length = size.x;
  } else {
    // For the 'right' octant, we flip the point and shift it according to
    // the CPU's OctantContains/Flip logic.
    p_oct = p.yx - vec2(0.0, c);
    se_degree = degree.y;
    cos_span = cos_angle_span.y;
    radius = radii.y;
    circle_center = circle_center_right;
    axis_length = size.y;
  }

  return distanceFromRSEOctant(p_oct, circle_center, radius, cos_span,
                               axis_length, se_degree);
}

// Calculates pixel size for rectangles using frag_info.pixel_size.
float rectPixelSize(highp vec2 p) {
  // Get pixel size in the direction perpendicular to the closest edge of the
  // rectangle: frag_info.pixel_size.x when closer to a vertical edge, and
  // frag_info.pixel_size.y when closer to a horizontal edge.
  vec2 distance = abs(abs(p) - frag_info.size);
  return (distance.x < distance.y) ? frag_info.pixel_size.x
                                   : frag_info.pixel_size.y;
}

// Special case pixel size calculation for rounded rectangles, similar to
// `rectPixelSize` for regular rectangles.
float roundRectPixelSize(highp vec2 p) {
  // The change in local coordinates per horizontal device pixel (device_dx)
  // and vertical device pixel (device_dy).
  vec2 device_dx = dFdx(v_position);
  vec2 device_dy = dFdy(v_position);
  // The size of a device pixel in terms of local coordinates.
  vec2 device_pixel_size = vec2(length(vec2(device_dx.x, device_dy.x)),
                                length(vec2(device_dx.y, device_dy.y)));

  // Select the corner radius for the quadrant of p.
  vec4 r = frag_info.radii;
  r.xy = (p.x > 0.0) ? r.xy : r.zw;
  float radius = (p.y > 0.0) ? r.x : r.y;

  // Vector from corner circle center to abs(p).
  highp vec2 corner_center = frag_info.size - radius;
  highp vec2 q = abs(p) - corner_center;

  float pixel_size;
  // If in the rounded corner arc, blend X and Y pixel sizes along the normal.
  if (q.x > 0.0 && q.y > 0.0) {
    pixel_size = length(normalize(q) * device_pixel_size);
  } else {
    // Otherwise, we are closer to a straight edge. Get pixel size in the
    // direction perpendicular to the closer edge.
    pixel_size = (q.x > q.y) ? device_pixel_size.x : device_pixel_size.y;
  }
  return pixel_size;
}

float pixelSize(float sdf) {
  vec2 gradient = vec2(dFdx(sdf), dFdy(sdf));
  return length(gradient);
}

// Evaluates the SDF for the shape selected by frag_info.type.
// Returns vec2(sdf, pixel_size).
vec2 filledSDF(highp vec2 p) {
  float sdf;
  float pixel_size;
  if (frag_info.type < 0.5) {  // Circle
    sdf = distanceFromCircle(p, frag_info.size.x);
    pixel_size = pixelSize(sdf);
  } else if (frag_info.type < 1.5) {  // Rect
    sdf = distanceFromRect(p, frag_info.size);
    // Rect has its own separate logic for calculating pixel size.
    pixel_size = rectPixelSize(p);
  } else if (frag_info.type < 2.5) {  // Oval
    sdf = distanceFromOval(p, frag_info.size);
    pixel_size = pixelSize(sdf);
  } else if (frag_info.type < 3.5) {  // Rounded Rect
    sdf = distanceFromRoundedRect(p, frag_info.size, frag_info.radii);
    // RoundRect has its own separate logic for calculating pixel size.
    pixel_size = roundRectPixelSize(p);
  } else {  // Symmetric Rounded Superellipse
    // cos(angle_span) only depends on uniforms, so it is evaluated once per
    // draw rather than per fragment.
    sdf = distanceFromRoundedSuperellipse(
        p, frag_info.superellipse_degree, frag_info.size, frag_info.radii.xy,
        cos(frag_info.angle_span), frag_info.circle_center_top,
        frag_info.circle_center_right);
    pixel_size = pixelSize(sdf);
  }
  return vec2(sdf, pixel_size);
}

// Evaluates the stroked SDF for the shape selected by frag_info.type.
// Returns vec2(sdf, pixel_size).
vec2 strokedSDF(highp vec2 p) {
  vec2 base_sdf_and_pixel_size = filledSDF(p);
  float base_sdf = base_sdf_and_pixel_size.x;
  float pixel_size = base_sdf_and_pixel_size.y;

  float half_stroke = max(frag_info.stroke_width, pixel_size) * 0.5;

  float sdf;
  if (frag_info.type >= 0.5 && frag_info.type < 1.5 &&
      frag_info.stroke_join < 0.5) {
    // Rect with Miter join
    float outer = distanceFromRect(p, frag_info.size + half_stroke);
    float inner = base_sdf + half_stroke;
    sdf = max(outer, -inner);
  } else if (frag_info.type >= 0.5 && frag_info.type < 1.5 &&
             frag_info.stroke_join >= 0.5 && frag_info.stroke_join < 1.5) {
    // Rect with Bevel join
    float outer =
        distanceFromChamferRect(p, frag_info.size + half_stroke, half_stroke);
    float inner = base_sdf + half_stroke;
    sdf = max(outer, -inner);
  } else {
    // All other shapes
    sdf = SDFStroke(base_sdf, pixel_size, frag_info.stroke_width).x;
  }
  return vec2(sdf, pixel_size);
}

// Converts linear coverage alpha to perceptual alpha.
float gammaCorrectedAlpha(float alpha, vec3 foreground_rgb) {
  // Gamma corrected alpha used for dark colors.
  // Fast approximation for `1.0 - pow(1.0 - alpha, 1.0 / 2.2)`.
  float alpha_dark = 1.0 - sqrt(1.0 - alpha);

  // Gamma corrected alpha used for light colors.
  // Fast approximation for `pow(alpha, 1.0 / 2.2)`.
  float alpha_light = sqrt(alpha);

  // Interpolate between the dark and light gamma corrected alphas based on the
  // foreground luma.
  float luma = dot(foreground_rgb, vec3(0.2126, 0.7152, 0.0722));
  return mix(alpha_dark, alpha_light, luma);
}

void main() {
  vec4 color = getColor();

  highp vec2 p = v_position;

  vec2 sdf_and_pixel_size =
      (frag_info.stroked < 0.5) ? filledSDF(p) : strokedSDF(p);
  float sdf = sdf_and_pixel_size.x;
  float pixel_size = sdf_and_pixel_size.y;

  float alpha = SDFAlpha(sdf, pixel_size, kAntialiasPixels);
  // Clamp alpha in case floating point precision errors cause it to be outside
  // [0.0, 1.0].
  alpha = clamp(alpha, 0.0, 1.0);
  if (alpha < 1.0) {
    alpha = gammaCorrectedAlpha(alpha, color.rgb);
  }

  frag_color = vec4(color.rgb, color.a * alpha);
  frag_color = IPPremultiply(frag_color);
}
