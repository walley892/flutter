// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/display_list/benchmarking/dl_ubersdf_benchmarks.h"

#include "flutter/display_list/dl_builder.h"
#include "flutter/display_list/dl_paint.h"
#include "flutter/display_list/effects/dl_color_source.h"
#include "flutter/display_list/geometry/dl_geometry_types.h"
#include "flutter/display_list/testing/dl_test_surface_provider.h"
#include "flutter/fml/logging.h"
#include "third_party/benchmark/include/benchmark/benchmark.h"

namespace flutter {
namespace testing {

namespace {

DlPoint GetPrimitivePosition(size_t index) {
  // Disperse primitives across the 1024x1024 surface using coprime factors.
  DlScalar x = static_cast<DlScalar>((index * 37) % 960) + 10.0f;
  DlScalar y = static_cast<DlScalar>((index * 59) % 960) + 10.0f;
  return DlPoint(x, y);
}

DlPaint MakePaint(UberSDFStyle style, DlColor color = DlColor::kBlue()) {
  DlPaint paint;
  paint.setAntiAlias(true);
  paint.setColor(color);
  if (style == UberSDFStyle::kStroke) {
    paint.setDrawStyle(DlDrawStyle::kStroke);
    paint.setStrokeWidth(2.0f);
  } else {
    paint.setDrawStyle(DlDrawStyle::kFill);
  }
  return paint;
}

void DrawShape(DisplayListBuilder& builder,
               UberSDFShape shape,
               const DlPoint& pos,
               DlScalar size,
               const DlPaint& paint) {
  DlRect bounds = DlRect::MakeXYWH(pos.x, pos.y, size, size);
  switch (shape) {
    case UberSDFShape::kRect:
      builder.DrawRect(bounds, paint);
      break;
    case UberSDFShape::kCircle:
      builder.DrawCircle(bounds.GetCenter(), size * 0.5f, paint);
      break;
    case UberSDFShape::kRRect:
      builder.DrawRoundRect(DlRoundRect::MakeRectXY(bounds, 8.0f, 8.0f), paint);
      break;
    case UberSDFShape::kRSE:
      builder.DrawRoundSuperellipse(
          DlRoundSuperellipse::MakeRectXY(bounds, 8.0f, 8.0f), paint);
      break;
  }
}

}  // namespace

void BM_UberSDF_Homogeneous(benchmark::State& state,
                            BackendType backend_type,
                            UberSDFShape shape,
                            UberSDFStyle style) {
  auto surface_provider = DlSurfaceProvider::Create(backend_type);
  if (!surface_provider) {
    state.SkipWithError("Surface provider creation failed");
    return;
  }

  constexpr size_t kCanvasSize = 1024;
  surface_provider->InitializeSurface(kCanvasSize, kCanvasSize);
  auto surface = surface_provider->GetPrimarySurface();
  surface->Clear(DlColor::kTransparent());
  surface->FlushSubmitCpuSync();

  size_t count = state.range(0);
  DisplayListBuilder builder;
  DlPaint paint = MakePaint(style);

  for (size_t i = 0; i < count; i++) {
    DlPoint pos = GetPrimitivePosition(i);
    DrawShape(builder, shape, pos, 40.0f, paint);
  }

  auto display_list = builder.Build();
  state.counters["DrawCallCount"] = count;

  size_t items_processed = 0;
  for ([[maybe_unused]] auto _ : state) {
    surface->RenderDisplayList(display_list);
    items_processed += count;
    surface->FlushSubmitCpuSync();
  }
  state.SetItemsProcessed(items_processed);
}

void BM_UberSDF_Heterogeneous(benchmark::State& state,
                              BackendType backend_type,
                              HeteroMixMode mix_mode) {
  auto surface_provider = DlSurfaceProvider::Create(backend_type);
  if (!surface_provider) {
    state.SkipWithError("Surface provider creation failed");
    return;
  }

  constexpr size_t kCanvasSize = 1024;
  surface_provider->InitializeSurface(kCanvasSize, kCanvasSize);
  auto surface = surface_provider->GetPrimarySurface();
  surface->Clear(DlColor::kTransparent());
  surface->FlushSubmitCpuSync();

  size_t count = state.range(0);
  DisplayListBuilder builder;

  DlPaint fill_paint = MakePaint(UberSDFStyle::kFill, DlColor::kBlue());
  DlPaint stroke_paint = MakePaint(UberSDFStyle::kStroke, DlColor::kRed());

  constexpr UberSDFShape kShapes[] = {
      UberSDFShape::kRect,
      UberSDFShape::kCircle,
      UberSDFShape::kRRect,
      UberSDFShape::kRSE,
  };

  for (size_t i = 0; i < count; i++) {
    UberSDFShape shape = kShapes[i % 4];
    DlPoint pos = GetPrimitivePosition(i);

    const DlPaint& paint =
        (mix_mode == HeteroMixMode::kAlternatingShapesAndStyles ||
         mix_mode == HeteroMixMode::kFullyMixed)
            ? ((i % 2 == 0) ? fill_paint : stroke_paint)
            : fill_paint;

    if (mix_mode == HeteroMixMode::kVariedTransforms ||
        mix_mode == HeteroMixMode::kFullyMixed) {
      builder.Save();
      DlScalar cx = pos.x + 20.0f;
      DlScalar cy = pos.y + 20.0f;
      builder.Translate(cx, cy);
      builder.Rotate(static_cast<DlScalar>((i * 15) % 360));
      DlScalar scale = 0.8f + static_cast<DlScalar>(i % 5) * 0.1f;
      builder.Scale(scale, scale);
      builder.Translate(-20.0f, -20.0f);
      DrawShape(builder, shape, DlPoint(0.0f, 0.0f), 40.0f, paint);
      builder.Restore();
    } else {
      DrawShape(builder, shape, pos, 40.0f, paint);
    }
  }

  auto display_list = builder.Build();
  state.counters["DrawCallCount"] = count;

  size_t items_processed = 0;
  for ([[maybe_unused]] auto _ : state) {
    surface->RenderDisplayList(display_list);
    items_processed += count;
    surface->FlushSubmitCpuSync();
  }
  state.SetItemsProcessed(items_processed);
}

void BM_UberSDF_Shading(benchmark::State& state,
                        BackendType backend_type,
                        ShadingType shading_type) {
  auto surface_provider = DlSurfaceProvider::Create(backend_type);
  if (!surface_provider) {
    state.SkipWithError("Surface provider creation failed");
    return;
  }

  constexpr size_t kCanvasSize = 1024;
  surface_provider->InitializeSurface(kCanvasSize, kCanvasSize);
  auto surface = surface_provider->GetPrimarySurface();
  surface->Clear(DlColor::kTransparent());
  surface->FlushSubmitCpuSync();

  size_t count = state.range(0);
  DisplayListBuilder builder;

  DlPaint solid_paint = MakePaint(UberSDFStyle::kFill, DlColor::kBlue());

  const DlColor linear_colors[2] = {DlColor::kBlue(), DlColor::kCyan()};
  const float stops[2] = {0.0f, 1.0f};
  auto linear_gradient =
      DlColorSource::MakeLinear(DlPoint(0.0f, 0.0f), DlPoint(40.0f, 40.0f), 2,
                                linear_colors, stops, DlTileMode::kClamp);
  DlPaint linear_paint = MakePaint(UberSDFStyle::kFill);
  linear_paint.setColorSource(linear_gradient);

  const DlColor radial_colors[2] = {DlColor::kRed(), DlColor::kYellow()};
  auto radial_gradient =
      DlColorSource::MakeRadial(DlPoint(20.0f, 20.0f), 20.0f, 2, radial_colors,
                                stops, DlTileMode::kClamp);
  DlPaint radial_paint = MakePaint(UberSDFStyle::kFill);
  radial_paint.setColorSource(radial_gradient);

  for (size_t i = 0; i < count; i++) {
    DlPoint pos = GetPrimitivePosition(i);
    const DlPaint* paint = &solid_paint;

    switch (shading_type) {
      case ShadingType::kSolidColor:
        paint = &solid_paint;
        break;
      case ShadingType::kLinearGradient:
        paint = &linear_paint;
        break;
      case ShadingType::kRadialGradient:
        paint = &radial_paint;
        break;
      case ShadingType::kMixedShading:
        if (i % 3 == 0) {
          paint = &solid_paint;
        } else if (i % 3 == 1) {
          paint = &linear_paint;
        } else {
          paint = &radial_paint;
        }
        break;
    }

    builder.Save();
    builder.Translate(pos.x, pos.y);
    DrawShape(builder, UberSDFShape::kRRect, DlPoint(0.0f, 0.0f), 40.0f,
              *paint);
    builder.Restore();
  }

  auto display_list = builder.Build();
  state.counters["DrawCallCount"] = count;

  size_t items_processed = 0;
  for ([[maybe_unused]] auto _ : state) {
    surface->RenderDisplayList(display_list);
    items_processed += count;
    surface->FlushSubmitCpuSync();
  }
  state.SetItemsProcessed(items_processed);
}

void BM_UberSDF_BatchBreaking(benchmark::State& state,
                              BackendType backend_type,
                              BatchBreakType break_type) {
  auto surface_provider = DlSurfaceProvider::Create(backend_type);
  if (!surface_provider) {
    state.SkipWithError("Surface provider creation failed");
    return;
  }

  constexpr size_t kCanvasSize = 1024;
  surface_provider->InitializeSurface(kCanvasSize, kCanvasSize);
  auto surface = surface_provider->GetPrimarySurface();
  surface->Clear(DlColor::kTransparent());
  surface->FlushSubmitCpuSync();

  size_t count = state.range(0);
  DisplayListBuilder builder;
  DlPaint default_paint = MakePaint(UberSDFStyle::kFill, DlColor::kBlue());

  constexpr DlBlendMode kBlendModes[] = {
      DlBlendMode::kSrcOver,
      DlBlendMode::kScreen,
      DlBlendMode::kOverlay,
      DlBlendMode::kDarken,
  };

  for (size_t i = 0; i < count; i++) {
    DlPoint pos = GetPrimitivePosition(i);
    DlRect rect = DlRect::MakeXYWH(pos.x, pos.y, 40.0f, 40.0f);

    switch (break_type) {
      case BatchBreakType::kUnbroken:
        builder.DrawRect(rect, default_paint);
        break;

      case BatchBreakType::kSaveRestoreOnly:
        builder.Save();
        builder.DrawRect(rect, default_paint);
        builder.Restore();
        break;

      case BatchBreakType::kTransformChurn:
        builder.Save();
        builder.Translate(pos.x + 20.0f, pos.y + 20.0f);
        builder.Rotate(static_cast<DlScalar>((i * 30) % 360));
        builder.Scale(0.9f + static_cast<DlScalar>(i % 3) * 0.1f,
                      0.9f + static_cast<DlScalar>(i % 3) * 0.1f);
        builder.Translate(-20.0f, -20.0f);
        builder.DrawRect(DlRect::MakeXYWH(0.0f, 0.0f, 40.0f, 40.0f),
                         default_paint);
        builder.Restore();
        break;

      case BatchBreakType::kClipStackEveryOp:
        builder.Save();
        builder.ClipRect(rect.Expand(2.0f, 2.0f));
        builder.DrawRect(rect, default_paint);
        builder.Restore();
        break;

      case BatchBreakType::kClipStackEvery16:
        if (i % 16 == 0) {
          if (i > 0) {
            builder.Restore();
          }
          builder.Save();
          builder.ClipRect(DlRect::MakeXYWH(0.0f, 0.0f, 1024.0f, 1024.0f));
        }
        builder.DrawRect(rect, default_paint);
        if (i == count - 1) {
          builder.Restore();
        }
        break;

      case BatchBreakType::kBlendModeShift: {
        DlPaint paint = default_paint;
        paint.setBlendMode(kBlendModes[i % 4]);
        builder.DrawRect(rect, paint);
        break;
      }
    }
  }

  auto display_list = builder.Build();
  state.counters["DrawCallCount"] = count;

  size_t items_processed = 0;
  for ([[maybe_unused]] auto _ : state) {
    surface->RenderDisplayList(display_list);
    items_processed += count;
    surface->FlushSubmitCpuSync();
  }
  state.SetItemsProcessed(items_processed);
}

// clang-format off

#define REGISTER_UBERSDF_HOMOGENEOUS(NAME, BACKEND, SHAPE, STYLE)       \
  BENCHMARK_CAPTURE(BM_UberSDF_Homogeneous, NAME/BACKEND,               \
                    BackendType::k##BACKEND,                            \
                    UberSDFShape::k##SHAPE,                             \
                    UberSDFStyle::k##STYLE)                             \
      ->Arg(100)                                                        \
      ->Arg(1000)                                                       \
      ->Arg(10000)                                                      \
      ->UseRealTime()                                                   \
      ->Unit(benchmark::kMillisecond);

#define REGISTER_UBERSDF_HETEROGENEOUS(NAME, BACKEND, MODE)             \
  BENCHMARK_CAPTURE(BM_UberSDF_Heterogeneous, NAME/BACKEND,             \
                    BackendType::k##BACKEND,                            \
                    HeteroMixMode::k##MODE)                             \
      ->Arg(100)                                                        \
      ->Arg(1000)                                                       \
      ->Arg(10000)                                                      \
      ->UseRealTime()                                                   \
      ->Unit(benchmark::kMillisecond);

#define REGISTER_UBERSDF_SHADING(NAME, BACKEND, TYPE)                   \
  BENCHMARK_CAPTURE(BM_UberSDF_Shading, NAME/BACKEND,                   \
                    BackendType::k##BACKEND,                            \
                    ShadingType::k##TYPE)                               \
      ->Arg(100)                                                        \
      ->Arg(1000)                                                       \
      ->Arg(10000)                                                      \
      ->UseRealTime()                                                   \
      ->Unit(benchmark::kMillisecond);

#define REGISTER_UBERSDF_BREAKING(NAME, BACKEND, TYPE)                  \
  BENCHMARK_CAPTURE(BM_UberSDF_BatchBreaking, NAME/BACKEND,              \
                    BackendType::k##BACKEND,                            \
                    BatchBreakType::k##TYPE)                            \
      ->Arg(100)                                                        \
      ->Arg(1000)                                                       \
      ->Arg(10000)                                                      \
      ->UseRealTime()                                                   \
      ->Unit(benchmark::kMillisecond);

#define REGISTER_UBERSDF_SUITE(BACKEND)                                              \
  REGISTER_UBERSDF_HOMOGENEOUS(Rect_Fill, BACKEND, Rect, Fill)                        \
  REGISTER_UBERSDF_HOMOGENEOUS(Rect_Stroke, BACKEND, Rect, Stroke)                    \
  REGISTER_UBERSDF_HOMOGENEOUS(Circle_Fill, BACKEND, Circle, Fill)                    \
  REGISTER_UBERSDF_HOMOGENEOUS(Circle_Stroke, BACKEND, Circle, Stroke)                \
  REGISTER_UBERSDF_HOMOGENEOUS(RRect_Fill, BACKEND, RRect, Fill)                      \
  REGISTER_UBERSDF_HOMOGENEOUS(RRect_Stroke, BACKEND, RRect, Stroke)                  \
  REGISTER_UBERSDF_HOMOGENEOUS(RSE_Fill, BACKEND, RSE, Fill)                          \
  REGISTER_UBERSDF_HOMOGENEOUS(RSE_Stroke, BACKEND, RSE, Stroke)                      \
  REGISTER_UBERSDF_HETEROGENEOUS(AlternatingShapes_Fill, BACKEND, AlternatingShapes)  \
  REGISTER_UBERSDF_HETEROGENEOUS(MixedStyles, BACKEND, AlternatingShapesAndStyles)    \
  REGISTER_UBERSDF_HETEROGENEOUS(VariedTransforms, BACKEND, VariedTransforms)        \
  REGISTER_UBERSDF_HETEROGENEOUS(FullyMixed, BACKEND, FullyMixed)                     \
  REGISTER_UBERSDF_SHADING(SolidColor, BACKEND, SolidColor)                           \
  REGISTER_UBERSDF_SHADING(LinearGradient, BACKEND, LinearGradient)                   \
  REGISTER_UBERSDF_SHADING(RadialGradient, BACKEND, RadialGradient)                   \
  REGISTER_UBERSDF_SHADING(MixedShading, BACKEND, MixedShading)                       \
  REGISTER_UBERSDF_BREAKING(Unbroken, BACKEND, Unbroken)                              \
  REGISTER_UBERSDF_BREAKING(SaveRestoreOnly, BACKEND, SaveRestoreOnly)                \
  REGISTER_UBERSDF_BREAKING(TransformChurn, BACKEND, TransformChurn)                  \
  REGISTER_UBERSDF_BREAKING(ClipStackEveryOp, BACKEND, ClipStackEveryOp)              \
  REGISTER_UBERSDF_BREAKING(ClipStackEvery16, BACKEND, ClipStackEvery16)              \
  REGISTER_UBERSDF_BREAKING(BlendModeShift, BACKEND, BlendModeShift)

#ifdef ENABLE_METAL_BENCHMARKS
REGISTER_UBERSDF_SUITE(ImpellerMetalSDF)
REGISTER_UBERSDF_SUITE(ImpellerMetal)
#endif

// clang-format on

}  // namespace testing
}  // namespace flutter
