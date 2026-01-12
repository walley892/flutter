// shaders/my_gradient.frag
#version 460 core

// Uniforms are inputs from the Dart code.
uniform vec2 uSize; // The size of the area being drawn
uniform float uTime;
uniform float[3] uColorArray;

uniform Foo {
  float x;
  float y;
} foo;


out vec4 fragColor;

uniform vec3 uColoruyvyutyug;

void main() {
  vec2 uv = vec2(1.0,1.0);

  // Define two colors for the gradient.
  vec4 color1 = vec4(1.0, 0.0, 0.0, 1.0); // Dark Blue
  vec4 color2 = vec4(uColorArray[0], uColorArray[1], uColorArray[2], 1.0); // Orange/Red

  fragColor = mix(color1, color2, sin(-uTime));
}
