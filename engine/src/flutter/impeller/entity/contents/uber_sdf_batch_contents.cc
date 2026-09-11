// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "impeller/entity/contents/uber_sdf_batch_contents.h"

#include "impeller/entity/contents/content_context.h"
#include "impeller/entity/contents/pipelines.h"
#include "impeller/renderer/render_pass.h"

namespace impeller {

using VS = InstancedUberSDFFastpathPipeline::VertexShader;
using FS = InstancedUberSDFFastpathPipeline::FragmentShader;

UberSDFBatchContents::UberSDFBatchContents(
    std::vector<UberSDFInstanceData> instances,
    UberSDFTier tier,
    BlendMode blend_mode)
    : instances_(std::move(instances)), tier_(tier), blend_mode_(blend_mode) {}

UberSDFBatchContents::~UberSDFBatchContents() = default;

bool UberSDFBatchContents::Render(const ContentContext& renderer,
                                  const Entity& entity,
                                  RenderPass& pass) const {
  if (instances_.empty()) {
    return true;
  }

  size_t instance_count = instances_.size();
  HostBuffer& host_buffer = renderer.GetTransientsDataBuffer();

  // 1. Emplace uniform FrameInfo (Pass Orthographic Projection)
  VS::FrameInfo frame_info;
  frame_info.projection = pass.GetOrthographicTransform();
  BufferView uniform_view = host_buffer.EmplaceUniform(frame_info);
  VS::BindFrameInfo(pass, uniform_view);

  // 2. Slice Instance Buffer from HostBuffer
  BufferView instance_view = host_buffer.Emplace(
      instances_.data(), instance_count * sizeof(UberSDFInstanceData),
      alignof(UberSDFInstanceData));

  // 3. Fetch Static Unit Quad Mesh from ContentContext
  const VertexBuffer& mesh = renderer.GetStaticUnitQuadVertexBuffer();

  // 4. Bind Buffers (Binding 0 = Mesh, Binding 1 = Instances)
  BufferView vertex_buffers[2] = {mesh.vertex_buffer, instance_view};
  pass.SetVertexBuffer(vertex_buffers, 2);
  pass.SetIndexBuffer(mesh.index_buffer, mesh.index_type);
  pass.SetElementCount(mesh.vertex_count);  // 6 indices
  pass.SetInstanceCount(instance_count);

  // 5. Configure Pipeline Options (Depth write disabled for AA fringes)
  ContentContextOptions options = OptionsFromPassAndEntity(pass, entity);
  options.primitive_type = PrimitiveType::kTriangle;
  options.depth_write_enabled = false;

  pass.SetPipeline(renderer.GetInstancedUberSDFFastpathPipeline(options));
  return pass.Draw().ok();
}

std::optional<Rect> UberSDFBatchContents::GetCoverage(
    const Entity& entity) const {
  return std::nullopt;
}

bool UberSDFBatchContents::IsOpaque(const Matrix& transform) const {
  return false;  // AA fringes require non-opaque blend semantics
}

}  // namespace impeller
