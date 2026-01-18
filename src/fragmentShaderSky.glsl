#version 450 core
in vec2 vUV;
out vec4 outColor;

uniform sampler2D skyEquirect;
uniform mat4 invProj;
uniform mat4 invView;

const float PI = 3.14159265;

vec2 dirToUV(vec3 d) {
  d = normalize(d);
  float u = atan(d.z, d.x) / (2.0 * PI) + 0.5;
  float v = asin(clamp(d.y, -1.0, 1.0)) / PI + 0.5;
  return vec2(u, v);
}

void main() {
  vec2 ndc = vUV * 2.0 - 1.0;

  vec4 clip = vec4(ndc, 1.0, 1.0);
  vec4 viewPos = invProj * clip;
  viewPos /= viewPos.w;

  vec3 dirView  = normalize(viewPos.xyz);
  vec3 dirWorld = normalize((invView * vec4(dirView, 0.0)).xyz);

  vec3 sky = texture(skyEquirect, dirToUV(dirWorld)).rgb;

  // cheap tone mapping (prevents white blowout)
  sky = sky / (sky + vec3(1.0));

  outColor = vec4(sky, 1.0);
}
