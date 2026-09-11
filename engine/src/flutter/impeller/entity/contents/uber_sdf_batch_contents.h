// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FLUTTER_IMPELLER_ENTITY_CONTENTS_UBER_SDF_BATCH_CONTENTS_H_
#define FLUTTER_IMPELLER_ENTITY_CONTENTS_UBER_SDF_BATCH_CONTENTS_H_

#include <vector>

#include "impeller/entity/contents/contents.h"
#include "impeller/entity/contents/uber_sdf_instance_data.h"

namespace impeller {

class UberSDFBatchContents : public Contents {
 public:
  UberSDFBatchContents(std::vector<UberSDFInstanceData> instances,
                       UberSDFTier tier,
                       BlendMode blend_mode);

  ~UberSDFBatchContents() override;

  // |Contents|
  bool Render(const ContentContext& renderer,
              const Entity& entity,
              RenderPass& pass) const override;

  // |Contents|
  std::optional<Rect> GetCoverage(const Entity& entity) const override;

  // |Contents|
  bool IsOpaque(const Matrix& transform) const override;

  size_t GetInstanceCount() const { return instances_.size(); }
  const std::vector<UberSDFInstanceData>& GetInstances() const {
    return instances_;
  }
  UberSDFTier GetTier() const { return tier_; }
  BlendMode GetBlendMode() const { return blend_mode_; }

 private:
  std::vector<UberSDFInstanceData> instances_;
  UberSDFTier tier_;
  BlendMode blend_mode_;
};

}  // namespace impeller

#endif  // FLUTTER_IMPELLER_ENTITY_CONTENTS_UBER_SDF_BATCH_CONTENTS_H_
