#pragma once
#include <glm/glm.hpp>
#include <vector>
#include <string>
#include "EnvMap.h"



struct RTMaterial {
  glm::vec3 albedo = glm::vec3(0.8f);
  bool shadowCatcher = false;
};

struct RTTriangle {
  glm::vec3 p0, p1, p2;
  glm::vec3 n0, n1, n2;
  int matId = 0;
};

struct RTScene {
  std::vector<RTTriangle> tris;
  std::vector<RTMaterial> mats;
};

struct RTCamera {
  glm::vec3 pos;
  glm::mat4 invView;
  float fovYDegrees = 45.f;
  float aspect = 1.f;
};

struct RTLight {
  glm::vec3 position;
  glm::vec3 color = glm::vec3(1.f);
  float intensity = 1.f;
};

class EnvMap;

class RayTracer {
public:
  RayTracer(int w, int h) : _w(w), _h(h) {}

  std::vector<glm::vec3> render(const RTScene& scene, const RTCamera& cam, const RTLight& light) const;

  static void savePPM(const std::string& filename, const std::vector<glm::vec3>& pixels, int w, int h);

  void buildBVH(const RTScene& scene);

  void setEnvMap(const EnvMap* env) { _env = env; }

private:
  int _w = 0, _h = 0;

  const EnvMap* _env = nullptr;

  struct Hit {
    float t = 1e30f;
    glm::vec3 p;
    glm::vec3 n;
    int matId = -1;
  };

  static bool intersectTriangle(const glm::vec3& ro, const glm::vec3& rd, const RTTriangle& tri, float& t, float& u, float& v);

  bool intersectScene(const RTScene& scene, const glm::vec3& ro, const glm::vec3& rd, Hit& hit, float tMaxLimit) const;

  glm::vec3 background(const glm::vec3& rd) const;

  bool isOccluded(const RTScene& scene, const glm::vec3& p, const glm::vec3& n, const glm::vec3& lightPos) const;

  struct AABB {
    glm::vec3 bmin = glm::vec3( 1e30f);
    glm::vec3 bmax = glm::vec3(-1e30f);
  };

  struct BVHNode {
    AABB box;
    int left = -1;
    int right = -1;
    int start = 0;   
    int count = 0;   
  };

  mutable std::vector<BVHNode> bvhNodes;
  mutable std::vector<int> bvhTriIndices;
  mutable bool bvhBuilt = false;

  static AABB triAABB(const RTTriangle& t);
  static AABB mergeAABB(const AABB& a, const AABB& b);
  static glm::vec3 triCentroid(const RTTriangle& t);

  static bool intersectAABB(const glm::vec3& ro, const glm::vec3& rd, const AABB& box, float& tminOut, float& tmaxOut);

  int buildBVHRecursive(const RTScene& scene, int start, int count);
  
  bool intersectBVH(const RTScene& scene, const glm::vec3& ro, const glm::vec3& rd, Hit& hit, float tMaxLimit) const;


};
