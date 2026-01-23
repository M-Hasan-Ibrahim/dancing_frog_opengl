#include "RayTracer.h"

#include <fstream>
#include <iostream>
#include <algorithm>
#include <cmath>
#include <limits>

static inline float clamp01(float x) { return std::max(0.f, std::min(1.f, x)); }



bool RayTracer::intersectTriangle(const glm::vec3& ro, const glm::vec3& rd,
                                  const RTTriangle& tri, float& t, float& u, float& v)
{
  const float EPS = 1e-7f;
  glm::vec3 e1 = tri.p1 - tri.p0;
  glm::vec3 e2 = tri.p2 - tri.p0;
  glm::vec3 pvec = glm::cross(rd, e2);
  float det = glm::dot(e1, pvec);

  if (std::fabs(det) < EPS) return false;
  float invDet = 1.f / det;

  glm::vec3 tvec = ro - tri.p0;
  u = glm::dot(tvec, pvec) * invDet;
  if (u < 0.f || u > 1.f) return false;

  glm::vec3 qvec = glm::cross(tvec, e1);
  v = glm::dot(rd, qvec) * invDet;
  if (v < 0.f || (u + v) > 1.f) return false;

  t = glm::dot(e2, qvec) * invDet;
  return t > EPS;
}

// bool RayTracer::intersectScene(const RTScene& scene, const glm::vec3& ro, const glm::vec3& rd, Hit& hit) const {
//   bool any = false;

//   for (const auto& tri : scene.tris) {
//     float t, u, v;
//     if (intersectTriangle(ro, rd, tri, t, u, v)) {
//       if (t < hit.t) {
//         any = true;
//         hit.t = t;
//         hit.p = ro + t * rd;

//         float w = 1.f - u - v;
//         glm::vec3 n = w * tri.n0 + u * tri.n1 + v * tri.n2;
//         hit.n = glm::normalize(n);

//         hit.matId = tri.matId;
//       }
//     }
//   }
//   return any;
// }

// bool RayTracer::intersectScene(const RTScene& scene,
//                                const glm::vec3& ro, const glm::vec3& rd,
//                                Hit& hit, float tMaxLimit) const
// {
//   bool any = false;

//   for (const auto& tri : scene.tris) {
//     float t, u, v;
//     if (intersectTriangle(ro, rd, tri, t, u, v)) {
//       if (t < hit.t && t < tMaxLimit) {
//         any = true;
//         hit.t = t;
//         hit.p = ro + t * rd;

//         float w = 1.f - u - v;
//         glm::vec3 n = w*tri.n0 + u*tri.n1 + v*tri.n2;
//         hit.n = glm::normalize(n);
//         hit.matId = tri.matId;
//       }
//     }
//   }

//   return any;
// }


bool RayTracer::intersectScene(const RTScene& scene, const glm::vec3& ro, const glm::vec3& rd, Hit& hit, float tMaxLimit) const {
  return intersectBVH(scene, ro, rd, hit, tMaxLimit);
}


bool RayTracer::isOccluded(const RTScene& scene, const glm::vec3& p, const glm::vec3& n,
                           const glm::vec3& lightPos) const
{
  const float EPS = 1e-4f;
  glm::vec3 ro = p + EPS * n;
  glm::vec3 toL = lightPos - ro;
  float distToL = glm::length(toL);
  glm::vec3 rd = toL / distToL;

  Hit h;
  return intersectScene(scene, ro, rd, h, distToL - 1e-3f);

//   if (intersectScene(scene, ro, rd, h)) {
//     return h.t < distToL;
//   }
//   return false;
}

std::vector<glm::vec3> RayTracer::render(const RTScene& scene, const RTCamera& cam, const RTLight& light) const {
  std::vector<glm::vec3> img(_w * _h, glm::vec3(0));

  float tanHalf = std::tan(glm::radians(cam.fovYDegrees) * 0.5f);

  for (int y = 0; y < _h; ++y) {
    for (int x = 0; x < _w; ++x) {

      float px = ( (x + 0.5f) / float(_w) ) * 2.f - 1.f;
      float py = 1.f - ( (y + 0.5f) / float(_h) ) * 2.f;

      px *= cam.aspect * tanHalf;
      py *= tanHalf;

      
      glm::vec3 dirCam = glm::normalize(glm::vec3(px, py, -1.f));
      glm::vec3 rd = glm::normalize(glm::vec3(cam.invView * glm::vec4(dirCam, 0.f)));
      glm::vec3 ro = cam.pos;

      Hit hit;
      glm::vec3 col = background(rd);

      if (intersectScene(scene, ro, rd, hit, 1e30f)) {
        const RTMaterial& mat = scene.mats[std::max(0, hit.matId)];

        glm::vec3 Lvec = light.position - hit.p;
        float dist2 = glm::dot(Lvec, Lvec);
        float dist  = std::sqrt(dist2);
        glm::vec3 wi = Lvec / std::max(dist, 1e-6f);

        float ndotl = std::max(0.f, glm::dot(hit.n, wi));

        float atten = 1.0f / std::max(dist2, 1e-4f);

        glm::vec3 Li = light.color * (light.intensity * atten);

        float vis = isOccluded(scene, hit.p, hit.n, light.position) ? 0.f : 1.f;

        // small ambient to avoid pure black (temporary; later replaced by env lighting)
        glm::vec3 ambient = 0.03f * mat.albedo;

        col = ambient + mat.albedo * Li * ndotl * vis;

      }

      img[y * _w + x] = col;

    }
  }
  return img;
}

void RayTracer::savePPM(const std::string& filename, const std::vector<glm::vec3>& pixels, int w, int h) {
  std::ofstream out(filename, std::ios::binary);
  out << "P6\n" << w << " " << h << "\n255\n";

  for (int i = 0; i < w * h; ++i) {
    
        glm::vec3 c = pixels[i];

        float exposure = 1.0f;
        c *= exposure;

        c = c / (glm::vec3(1.0f) + c);
        
        c = glm::vec3(std::pow(std::max(c.x, 0.0f), 1.f/2.2f),
                    std::pow(std::max(c.y, 0.0f), 1.f/2.2f),
                    std::pow(std::max(c.z, 0.0f), 1.f/2.2f));

        c = glm::clamp(c, glm::vec3(0.0f), glm::vec3(1.0f));


        unsigned char r = (unsigned char)(255.f * c.x);
        unsigned char g = (unsigned char)(255.f * c.y);
        unsigned char b = (unsigned char)(255.f * c.z);
        out.write((char*)&r, 1);
        out.write((char*)&g, 1);
        out.write((char*)&b, 1);
  }
}

RayTracer::AABB RayTracer::triAABB(const RTTriangle& t) {
  AABB b;
  b.bmin = glm::min(t.p0, glm::min(t.p1, t.p2));
  b.bmax = glm::max(t.p0, glm::max(t.p1, t.p2));
  return b;
}

RayTracer::AABB RayTracer::mergeAABB(const AABB& a, const AABB& b) {
  AABB o;
  o.bmin = glm::min(a.bmin, b.bmin);
  o.bmax = glm::max(a.bmax, b.bmax);
  return o;
}

glm::vec3 RayTracer::triCentroid(const RTTriangle& t) {
  return (t.p0 + t.p1 + t.p2) * (1.0f/3.0f);
}

// bool RayTracer::intersectAABB(const glm::vec3& ro, const glm::vec3& rd,
//                               const AABB& box, float& tminOut, float& tmaxOut)
// {
//   float tmin = -1e30f, tmax = 1e30f;

//   for(int a=0; a<3; ++a) {
//     float invD = 1.0f / rd[a];
//     float t0 = (box.bmin[a] - ro[a]) * invD;
//     float t1 = (box.bmax[a] - ro[a]) * invD;
//     if(t0 > t1) std::swap(t0, t1);
//     tmin = std::max(tmin, t0);
//     tmax = std::min(tmax, t1);
//     if(tmax < tmin) return false;
//   }

//   tminOut = tmin;
//   tmaxOut = tmax;
//   return tmax > 0.0f;
// }

bool RayTracer::intersectAABB(const glm::vec3& ro, const glm::vec3& rd, const AABB& box, float& tminOut, float& tmaxOut)
{
  const float INF = std::numeric_limits<float>::infinity();
  float tmin = -INF;
  float tmax = +INF;

  for (int a = 0; a < 3; ++a) {
    const float o = ro[a];
    const float d = rd[a];
    const float mn = box.bmin[a];
    const float mx = box.bmax[a];

    if (std::fabs(d) < 1e-12f) {
      // Ray parallel to slab: must be inside the slab to have intersection
      if (o < mn || o > mx) { tminOut = tmin; tmaxOut = tmax; return false; }
      continue;
    }

    float invD = 1.0f / d;
    float t0 = (mn - o) * invD;
    float t1 = (mx - o) * invD;
    if (t0 > t1) std::swap(t0, t1);

    tmin = std::max(tmin, t0);
    tmax = std::min(tmax, t1);

    if (tmax < tmin) { tminOut = tmin; tmaxOut = tmax; return false; }
  }

  tminOut = tmin;
  tmaxOut = tmax;
  return true;
}


void RayTracer::buildBVH(const RTScene& scene) {
  bvhNodes.clear();
  bvhTriIndices.resize(scene.tris.size());
  for (int i = 0; i < (int)scene.tris.size(); ++i) bvhTriIndices[i] = i;

  if(scene.tris.empty()) {
    bvhBuilt = true;
    return;
  }


  bvhNodes.reserve(scene.tris.size() * 2);

  buildBVHRecursive(scene, 0, (int)scene.tris.size());
  bvhBuilt = true;
}

int RayTracer::buildBVHRecursive(const RTScene& scene, int start, int count) {
  int nodeIdx = (int)bvhNodes.size();
  bvhNodes.push_back(BVHNode());

  AABB bounds;
  AABB centroidBounds;
  for(int i=0; i<count; ++i) {
    const RTTriangle& tri = scene.tris[bvhTriIndices[start + i]];
    bounds = mergeAABB(bounds, triAABB(tri));

    glm::vec3 c = triCentroid(tri);
    AABB cb; cb.bmin = cb.bmax = c;
    centroidBounds = mergeAABB(centroidBounds, cb);
  }

  bvhNodes[nodeIdx].box = bounds;

  const int LEAF_TRI_COUNT = 4;
  glm::vec3 ext = centroidBounds.bmax - centroidBounds.bmin;

  // leaf conditions
  if(count <= LEAF_TRI_COUNT || ext.x < 1e-6f && ext.y < 1e-6f && ext.z < 1e-6f) {
    bvhNodes[nodeIdx].start = start;
    bvhNodes[nodeIdx].count = count;
    return nodeIdx;
  }

  // choose split axis
  int axis = 0;
  if(ext.y > ext.x) axis = 1;
  if(ext.z > ext[axis]) axis = 2;

  int mid = start + count/2;

  std::nth_element(
    bvhTriIndices.begin() + start,
    bvhTriIndices.begin() + mid,
    bvhTriIndices.begin() + start + count,
    [&](int ia, int ib) {
      return triCentroid(scene.tris[ia])[axis] < triCentroid(scene.tris[ib])[axis];
    }
  );

  int leftCount  = mid - start;
  int rightCount = count - leftCount;

  int left  = buildBVHRecursive(scene, start, leftCount);
  int right = buildBVHRecursive(scene, mid, rightCount);

  bvhNodes[nodeIdx].left  = left;
  bvhNodes[nodeIdx].right = right;
  bvhNodes[nodeIdx].count = 0; // internal
  return nodeIdx;
}


bool RayTracer::intersectBVH(const RTScene& scene, const glm::vec3& ro, const glm::vec3& rd, Hit& hit, float tMaxLimit) const
{
  if(!bvhBuilt) return false;
  if(bvhNodes.empty()) return false;

  bool any = false;
  
  int stack[128];
  int sp = 0;
  stack[sp++] = 0;

  while(sp) {
    int ni = stack[--sp];
    const BVHNode& node = bvhNodes[ni];

    float tmin, tmax;
    if(!intersectAABB(ro, rd, node.box, tmin, tmax)) continue;
    if(tmin > hit.t) continue;
    if(tmin > tMaxLimit) continue;

    if(node.count > 0) {
        
      for(int i=0; i<node.count; ++i) {
        const RTTriangle& tri = scene.tris[bvhTriIndices[node.start + i]];
        float t, u, v;
        if(intersectTriangle(ro, rd, tri, t, u, v)) {
          if(t < hit.t && t < tMaxLimit) {
            any = true;
            hit.t = t;
            hit.p = ro + t * rd;
            float w = 1.f - u - v;
            glm::vec3 n = w*tri.n0 + u*tri.n1 + v*tri.n2;
            hit.n = glm::normalize(n);
            hit.matId = tri.matId;
          }
        }
      }
    } else {
      if(node.left  >= 0) stack[sp++] = node.left;
      if(node.right >= 0) stack[sp++] = node.right;
    }
  }

  return any;
}

glm::vec3 RayTracer::background(const glm::vec3& rd) const {
  if(_env) return _env->sample(rd);

  float t = 0.5f * (rd.y + 1.f);
  return (1.f - t)*glm::vec3(0.08f,0.10f,0.15f) + t*glm::vec3(0.6f,0.75f,1.0f);
}
