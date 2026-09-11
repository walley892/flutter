// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FLUTTER_IMPELLER_DISPLAY_LIST_UBER_SDF_BATCHER_H_
#define FLUTTER_IMPELLER_DISPLAY_LIST_UBER_SDF_BATCHER_H_

#include <cstdint>
#include <optional>
#include <vector>

#include "impeller/display_list/paint.h"
#include "impeller/entity/contents/uber_sdf_instance_data.h"
#include "impeller/entity/contents/uber_sdf_parameters.h"
#include "impeller/geometry/matrix.h"
#include "impeller/geometry/rect.h"

namespace impeller {

class Canvas;

class UberSDFBatcher {
 public:
  static constexpr size_t kMaxBatchSize = 512;

  explicit UberSDFBatcher(Canvas& canvas);
  ~UberSDFBatcher();

  // Attempts to append an SDF shape to the active batch.
  // Returns true if appended or culled, false if rejected (requiring fallback).
  bool AddShape(const Paint& paint,
                const UberSDFParameters& params,
                const Matrix& transform,
                uint32_t shape_depth,
                const std::optional<Rect>& clip_coverage);

  // Flushes the accumulated batch to the current RenderPass.
  void Flush();

  bool IsEmpty() const { return instances_.empty(); }

  size_t GetInstanceCount() const { return instances_.size(); }

 private:
  struct BatchKey {
    UberSDFTier tier = UberSDFTier::kFastpath;
    BlendMode blend_mode = BlendMode::kSrcOver;
    size_t clip_height = 0;
    uint32_t clip_depth = 0;
    uintptr_t texture_id = 0;

    bool IsCompatible(const BatchKey& other) const {
      return tier == other.tier && blend_mode == other.blend_mode &&
             clip_height == other.clip_height &&
             clip_depth == other.clip_depth && texture_id == other.texture_id;
    }
  };

  Canvas& canvas_;
  BatchKey current_key_;
  std::vector<UberSDFInstanceData> instances_;
};

}  // namespace impeller

#endif  // FLUTTER_IMPELLER_DISPLAY_LIST_UBER_SDF_BATCHER_H_
