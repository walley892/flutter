// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <emscripten/console.h>

#include "flutter/fml/mapping.h"
#include "flutter/impeller/display_list/dl_runtime_effect_impeller.h"
#include "flutter/impeller/runtime_stage/runtime_stage.h"
#include "impeller/runtime_stage/runtime_stage_flatbuffers.h"
#include "third_party/skia/include/core/SkString.h"

namespace Skwasm {
sk_sp<flutter::DlRuntimeEffect> CreateRuntimeEffect(SkString* source) {
  if (!source || source->isEmpty()) {
    return flutter::DlRuntimeEffectImpeller::Make(nullptr);
  }

  auto payload = std::make_shared<fml::NonOwnedMapping>(
      reinterpret_cast<const uint8_t*>(source->c_str()), source->size());

  auto stages_or = impeller::RuntimeStage::DecodeRuntimeStages(payload);
  if (!stages_or.ok()) {
    emscripten_console_error(
        "Failed to decode FlatBuffers runtime stages for Wimp");
    return flutter::DlRuntimeEffectImpeller::Make(nullptr);
  }
  auto stage = (*stages_or)[impeller::RuntimeStageBackend::kOpenGLES3];
  if (!stage) {
    emscripten_console_error(
        "FlatBuffers runtime stages missing kOpenGLES3 stage for Wimp");
    return flutter::DlRuntimeEffectImpeller::Make(nullptr);
  }
  return flutter::DlRuntimeEffectImpeller::Make(std::move(stage));
}
}  // namespace Skwasm
