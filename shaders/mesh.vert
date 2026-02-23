#version 450

layout(push_constant) uniform PC {
  float aspect; // width / height
} pc;

layout(location = 0) out vec3 vColor;

void main() {
  // Equilateral triangle with side length 1 in "square" space.
  // Height for side=1 is sqrt(3)/2.
  float h = 0.8660254037844386; // sqrt(3)/2

  // Centroid at origin:
  // base y = -h/3, top y = 2h/3, x = ±0.5
  vec2 pos[3] = vec2[](
    vec2(-0.5, -h / 3.0),
    vec2( 0.5, -h / 3.0),
    vec2( 0.0,  2.0 * h / 3.0)
  );

  vec3 col[3] = vec3[](
    vec3(1.0, 0.2, 0.2),
    vec3(0.2, 1.0, 0.2),
    vec3(0.2, 0.2, 1.0)
  );

  vec2 p = pos[gl_VertexIndex] * 1.2; // size
  p.x /= pc.aspect;                   // aspect correction (pixel-space isotropy)

  p.y = -p.y;
  gl_Position = vec4(p, 0.0, 1.0);
  vColor = col[gl_VertexIndex];
}
