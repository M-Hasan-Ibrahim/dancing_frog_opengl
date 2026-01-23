#include "EnvMap.h"
#include <cmath>
#include <algorithm>

#include "stb_image.h"

static inline float clamp01(float x){ return std::max(0.f, std::min(1.f, x)); }
static inline float wrap01(float x){ return x - std::floor(x); }

bool EnvMap::loadHDR(const std::string& filename) {
  int c=0;

  float* img = stbi_loadf(filename.c_str(), &_w, &_h, &c, 3);
  
  if(!img) return false;
  _rgb.assign(img, img + (_w*_h*3));
  stbi_image_free(img);
  return true;
}

glm::vec3 EnvMap::texel(int x, int y) const {
  x = (x % _w + _w) % _w;
  y = std::max(0, std::min(_h-1, y));
  int idx = (y*_w + x)*3;
  return glm::vec3(_rgb[idx+0], _rgb[idx+1], _rgb[idx+2]);
}

glm::vec3 EnvMap::sample(const glm::vec3& dir) const {
  if(_w==0 || _h==0) return glm::vec3(0.2f,0.3f,0.4f);

  glm::vec3 d = glm::normalize(dir);

  float u = std::atan2(d.z, d.x) / (2.0f * (float)M_PI) + 0.5f;
  float v = std::acos(std::max(-1.f, std::min(1.f, d.y))) / (float)M_PI;

  u = wrap01(u);
  v = clamp01(1.0f - v);

  float fx = u * (_w - 1);
  float fy = v * (_h - 1);
  int x0 = (int)std::floor(fx), y0 = (int)std::floor(fy);
  int x1 = x0 + 1, y1 = y0 + 1;
  float tx = fx - x0, ty = fy - y0;

  glm::vec3 c00 = texel(x0,y0), c10 = texel(x1,y0);
  glm::vec3 c01 = texel(x0,y1), c11 = texel(x1,y1);
  glm::vec3 c0 = (1-tx)*c00 + tx*c10;
  glm::vec3 c1 = (1-tx)*c01 + tx*c11;
  return (1-ty)*c0 + ty*c1;
}
