// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "impeller/display_list/uber_sdf_batcher.h"

#include "impeller/display_list/canvas.h"
#include "impeller/entity/contents/uber_sdf_batch_contents.h"
#include "impeller/entity/entity.h"

namespace impeller {

UberSDFBatcher::UberSDFBatcher(Canvas& canvas) : canvas_(canvas) {
  instances_.reserve(kMaxBatchSize);
}

UberSDFBatcher::~UberSDFBatcher() {
  Flush();
}

bool UberSDFBatcher::AddShape(const Paint& paint,
                              const UberSDFParameters& params,
                              const Matrix& transform,
                              uint32_t shape_depth,
                              const std::optional<Rect>& clip_coverage) {
  // 1. Perspective check: 3D perspective triggers fallback to standalone draw.
  if (transform.HasPerspective2D()) {
    return false;
  }

  // 2. Reject filters, shaders, or advanced blends requiring separate passes.
  if (paint.color_source || paint.mask_blur_descriptor.has_value() ||
      paint.image_filter || paint.color_filter || paint.invert_colors ||
      paint.blend_mode > Entity::kLastPipelineBlendMode) {
    return false;
  }

  // 3. Determine Tier.
  UberSDFTier tier = UberSDFTier::kFastpath;
  if (params.type == UberSDFParameters::Type::kOval ||
      params.type == UberSDFParameters::Type::kRoundedSuperellipseSymmetric ||
      params.gradient.has_value()) {
    tier = UberSDFTier::kComplex;
    // For fastpath instancing, complex shapes and gradients fall back to
    // standalone
    return false;
  }

  if (canvas_.IsSkipping()) {
    return true;  // Culled by canvas skip state.
  }

  // 4. Evaluate Batch Key compatibility.
  BatchKey candidate_key;
  candidate_key.tier = tier;
  candidate_key.blend_mode = paint.blend_mode;
  candidate_key.clip_height = canvas_.GetClipHeight();
  candidate_key.clip_depth = canvas_.GetMaxOpDepth();
  candidate_key.texture_id = 0;  // Solid color in Fastpath

  if (!instances_.empty() && !current_key_.IsCompatible(candidate_key)) {
    Flush();
  }

  // 5. Pass-space and center folding:
  // T_pass = Translate(-global_pass_position) * transform
  // T_effective = T_pass * Translate(center)
  Matrix pass_transform =
      Matrix::MakeTranslation(Vector3(-canvas_.GetGlobalPassPosition())) *
      transform;
  Point center = params.center;
  Matrix effective_transform = pass_transform * Matrix::MakeTranslation(Vector3(
                                                    center.x, center.y, 0.0f));

  // 6. CPU 2D AABB Culling against active viewport/clip coverage.
  if (clip_coverage.has_value()) {
    Scalar stroke_pad = params.stroke ? params.stroke->width * 0.5f : 0.0f;
    Scalar aa_pad = 2.0f;
    Rect local_bounds =
        Rect::MakeOriginSize(Point(-params.size.x, -params.size.y),
                             Size(params.size.x * 2.0f, params.size.y * 2.0f))
            .Expand(Size(stroke_pad + aa_pad, stroke_pad + aa_pad));
    Rect world_bounds = local_bounds.TransformBounds(effective_transform);
    if (!clip_coverage->IntersectsWithRect(world_bounds)) {
      return true;  // Culled on CPU; safely drop without drawing.
    }
  }

  // 7. Pack into UberSDFInstanceData.
  UberSDFInstanceData instance;
  const Scalar* m = effective_transform.m;
  instance.basis[0] = m[0];  // m00
  instance.basis[1] = m[1];  // m01
  instance.basis[2] = m[4];  // m10
  instance.basis[3] = m[5];  // m11

  instance.translation_and_depth[0] = m[12];  // tx
  instance.translation_and_depth[1] = m[13];  // ty
  instance.translation_and_depth[2] = Entity::GetShaderClipDepth(shape_depth);
  instance.translation_and_depth[3] =
      params.stroke ? params.stroke->width : 0.0f;

  uint32_t flags = 0;
  if (params.stroke) {
    flags |= 1;  // bit 0: stroked
    if (params.stroke->join == Join::kBevel) {
      flags |= (1 << 1);
    }
    if (params.stroke->join == Join::kRound) {
      flags |= (2 << 1);
    }
  }

  instance.half_size[0] = params.size.x;
  instance.half_size[1] = params.size.y;
  instance.flags = static_cast<float>(flags);
  Color color = params.color;
  color.alpha *= canvas_.GetDistributedOpacity();
  instance.color = UberSDFInstanceData::PackColorRGBA8(color);

  // Unified Radii Mapping:
  if (params.type == UberSDFParameters::Type::kCircle) {
    instance.radii[0] = params.size.x;
    instance.radii[1] = params.size.x;
    instance.radii[2] = params.size.x;
    instance.radii[3] = params.size.x;
  } else if (params.type == UberSDFParameters::Type::kRect) {
    instance.radii[0] = 0.0f;
    instance.radii[1] = 0.0f;
    instance.radii[2] = 0.0f;
    instance.radii[3] = 0.0f;
  } else {
    instance.radii[0] = params.radii.x;
    instance.radii[1] = params.radii.y;
    instance.radii[2] = params.radii.z;
    instance.radii[3] = params.radii.w;
  }

  if (instances_.empty()) {
    current_key_ = candidate_key;
  }
  instances_.push_back(instance);

  if (instances_.size() >= kMaxBatchSize) {
    Flush();
  }
  return true;
}

void UberSDFBatcher::Flush() {
  if (instances_.empty()) {
    return;
  }

  auto batch_contents = std::make_unique<UberSDFBatchContents>(
      std::move(instances_), current_key_.tier, current_key_.blend_mode);

  Entity entity;
  entity.SetBlendMode(current_key_.blend_mode);
  entity.SetClipDepth(current_key_.clip_height);
  entity.SetContents(std::move(batch_contents));

  // Render the batched entity directly into the current pass.
  RenderPass& pass = canvas_.GetCurrentRenderPass();
  pass.SetCommandLabel("UberSDFBatch");
  entity.Render(canvas_.GetRenderer(), pass);

  instances_.clear();
}

}  // namespace impeller
