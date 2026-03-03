#version 450

layout(location = 0) in vec3 vNormal;
layout(location = 1) in vec2 vUV;

layout(location = 0) out vec4 outColor;

void main() {
  vec3 n = normalize(vNormal);
  outColor = vec4(abs(n), 1.0);
}
