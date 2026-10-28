// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FLUTTER_SHELL_PLATFORM_ANDROID_ANDROID_SURFACE_VK_IMPELLER_H_
#define FLUTTER_SHELL_PLATFORM_ANDROID_ANDROID_SURFACE_VK_IMPELLER_H_

#include "flutter/fml/concurrent_message_loop.h"
#include "flutter/fml/macros.h"
#include "flutter/impeller/display_list/aiks_context.h"
#include "flutter/impeller/renderer/backend/vulkan/surface_context_vk.h"
#include "flutter/shell/platform/android/android_context_vk_impeller.h"
#include "flutter/shell/platform/android/surface/android_native_window.h"
#include "flutter/shell/platform/android/surface/android_surface.h"
#include "shell/gpu/gpu_surface_vulkan_impeller.h"

namespace flutter {

class AndroidSurfaceVKImpeller : public AndroidSurface {
 public:
  explicit AndroidSurfaceVKImpeller(
      const std::shared_ptr<AndroidContextVKImpeller>& android_context);

  ~AndroidSurfaceVKImpeller() override;

  // |AndroidSurface|
  bool IsValid() const override;

  // |AndroidSurface|
  std::unique_ptr<Surface> CreateGPUSurface(
      GrDirectContext* gr_context) override;

  // |AndroidSurface|
  void TeardownOnScreenContext() override;

  // |AndroidSurface|
  bool OnScreenSurfaceResize(const DlISize& size) override;

  // |AndroidSurface|
  bool ResourceContextMakeCurrent() override;

  // |AndroidSurface|
  bool ResourceContextClearCurrent() override;

  // |AndroidSurface|
  std::shared_ptr<impeller::Context> GetImpellerContext() override;

  // |AndroidSurface|
  bool SetNativeWindow(
      fml::RefPtr<AndroidNativeWindow> window,
      const std::shared_ptr<PlatformViewAndroidJNI>& jni_facade) override;

  // |AndroidSurface|
  void PrewarmRendering() override;

 private:
  /// Returns the warm AiksContext, or the one a live GPU surface is using, or
  /// else a new one that this surface does not retain. Returns nullptr if a
  /// new AiksContext could not be created.
  std::shared_ptr<impeller::AiksContext> GetOrCreateAiksContext() const;

  std::shared_ptr<impeller::SurfaceContextVK> surface_context_vk_;
  fml::RefPtr<AndroidNativeWindow> native_window_;
  bool is_valid_ = false;

  // |PrewarmRendering| creates the AiksContext, and with it the
  // ContentContext that queues the default pipeline compiles, before the first
  // GPU surface exists. Until a GPU surface takes the AiksContext, this
  // surface holds a strong reference to it. After that it only holds a weak
  // reference, so the AiksContext lives exactly as long as the GPU surface, as
  // it did before warm-up existed: it is freed when the rasterizer tears the
  // GPU surface down (for example, when the app is backgrounded) and the next
  // |CreateGPUSurface| creates a new one, whose pipelines are cache hits.
  //
  // Only accessed on the raster thread.
  std::shared_ptr<impeller::AiksContext> warm_aiks_context_;
  std::weak_ptr<impeller::AiksContext> handed_off_aiks_context_;

  FML_DISALLOW_COPY_AND_ASSIGN(AndroidSurfaceVKImpeller);
};

}  // namespace flutter

#endif  // FLUTTER_SHELL_PLATFORM_ANDROID_ANDROID_SURFACE_VK_IMPELLER_H_
