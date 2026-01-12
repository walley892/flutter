#version 100 core

// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

precision highp float;

layout(location = 0) out vec4 oColor;

layout(location = 0) uniform float floatArray[2];
layout(location = 2) uniform vec2 vec2Array[2];
layout(location = 6) uniform vec3 vec3Array[2];
layout(location = 12) uniform mat2 mat2Array[2];

void main() {
  vec4 badColor = vec4(1.0, 0, 0, 1.0);
  vec4 goodColor = vec4(0, 1.0, 0, 1.0);

  // The test populates the uniforms with strictly increasing values, so if
  // out-of-order values are read out of the uniforms, then the bad color that
  // causes the test to fail is returned.

  if (floatArray[0] == 0) {
    oColor = badColor;
    return;
  }

  if (floatArray[1] == 0) {
    oColor = badColor;
    return;
  }
  if (vec2Array[0].x == 0) {
    oColor = badColor;
    return;
  }
  if (vec2Array[0].y == 0) {
    oColor = badColor;
    return;
  }
  if (vec2Array[1].x == 0) {
    oColor = badColor;
    return;
  }
  if (vec2Array[1].y == 0) {
    oColor = badColor;
    return;
  }
  if (vec3Array[0].x == 0) {
    oColor = badColor;
    return;
  }

  if (vec3Array[0].y == 0) {
    oColor = badColor;
    return;
  }
  if (vec3Array[0].z == 0) {
    oColor = badColor;
    return;
  }

  if (vec3Array[1].x == 0) {
    oColor = badColor;
    return;
  }

  if (vec3Array[1].y == 0) {
    oColor = badColor;
    return;
  }

  if (vec3Array[1].z == 0) {
    oColor = badColor;
    return;
  }

  if (mat2Array[0][0][0] == 0) {
    oColor = badColor;
    return;
  }
  if (mat2Array[0][0][1] == 0) {
    oColor = badColor;
    return;
  }
  if (mat2Array[0][1][0] == 0) {
    oColor = badColor;
    return;
  }
  if (mat2Array[0][1][1] == 0) {
    oColor = badColor;
    return;
  }
  if (mat2Array[1][0][0] == 0) {
    oColor = badColor;
    return;
  }
  if (mat2Array[1][0][1] == 0) {
    oColor = badColor;
    return;
  }
  if (mat2Array[1][1][0] == 0) {
    oColor = badColor;
    return;
  }
  if (mat2Array[1][1][1] == 0) {
    oColor = badColor;
    return;
  }

  oColor = goodColor;
}
