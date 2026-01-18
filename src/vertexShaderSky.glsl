#version 450 core
out vec2 vUV;

// fullscreen triangle
void main() {
  vec2 p;
  if (gl_VertexID == 0) p = vec2(-1.0, -1.0);
  if (gl_VertexID == 1) p = vec2( 3.0, -1.0);
  if (gl_VertexID == 2) p = vec2(-1.0,  3.0);

  vUV = 0.5 * (p + vec2(1.0));
  gl_Position = vec4(p, 0.0, 1.0);
}
