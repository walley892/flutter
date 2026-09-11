// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FLUTTER_DISPLAY_LIST_BENCHMARKING_DL_UBERSDF_BENCHMARKS_H_
#define FLUTTER_DISPLAY_LIST_BENCHMARKING_DL_UBERSDF_BENCHMARKS_H_

#include "flutter/display_list/dl_builder.h"
#include "flutter/display_list/effects/dl_color_source.h"
#include "flutter/display_list/testing/dl_test_surface_provider.h"
#include "third_party/benchmark/include/benchmark/benchmark.h"

namespace flutter {
namespace testing {

using BackendType = DlSurfaceProvider::BackendType;

enum class UberSDFShape {
  kRect,
  kCircle,
  kRRect,
  kLine,
  kRSE,
};

enum class UberSDFStyle {
  kFill,
  kStroke,
};

enum class HeteroMixMode {
  kAlternatingShapes,
  kAlternatingShapesAndStyles,
  kVariedTransforms,
  kFullyMixed,
};

enum class ShadingType {
  kSolidColor,
  kLinearGradient,
  kRadialGradient,
  kMixedShading,
};

enum class BatchBreakType {
  kUnbroken,
  kSaveRestoreOnly,
  kTransformChurn,
  kClipStackEveryOp,
  kClipStackEvery16,
  kBlendModeShift,
};

void BM_UberSDF_Homogeneous(benchmark::State& state,
                            BackendType backend_type,
                            UberSDFShape shape,
                            UberSDFStyle style);

void BM_UberSDF_Heterogeneous(benchmark::State& state,
                              BackendType backend_type,
                              HeteroMixMode mix_mode);

void BM_UberSDF_Shading(benchmark::State& state,
                        BackendType backend_type,
                        ShadingType shading_type);

void BM_UberSDF_BatchBreaking(benchmark::State& state,
                              BackendType backend_type,
                              BatchBreakType break_type);

}  // namespace testing
}  // namespace flutter

#endif  // FLUTTER_DISPLAY_LIST_BENCHMARKING_DL_UBERSDF_BENCHMARKS_H_
