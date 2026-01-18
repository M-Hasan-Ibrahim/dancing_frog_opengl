#define _USE_MATH_DEFINES
#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>


#include "Mesh.h"

#include <cmath>
#include <algorithm>
#include <iostream>
#include <fstream>
#include <exception>
#include <ios>
#include <string>
#include <memory>

#include <unordered_map>
#include <cfloat>


Mesh::~Mesh()
{
  clear();
}

void Mesh::computeBoundingSphere(glm::vec3 &center, float &radius) const
{
  center = glm::vec3(0.0);
  radius = 0.f;
  for(const auto &p : _vertexPositions)
    center += p;
  center /= _vertexPositions.size();
  for(const auto &p : _vertexPositions)
    radius = std::max(radius, distance(center, p));
}

void Mesh::recomputePerVertexNormals(bool angleBased)
{
  _vertexNormals.clear();
  // Change the following code to compute a proper per-vertex normal
  _vertexNormals.resize(_vertexPositions.size(), glm::vec3(0.0, 0.0, 0.0));

  for(unsigned int tIt=0 ; tIt < _triangleIndices.size() ; ++tIt) {
    glm::uvec3 t = _triangleIndices[tIt];
    glm::vec3 n_t = glm::cross(
      _vertexPositions[t[1]] - _vertexPositions[t[0]],
      _vertexPositions[t[2]] - _vertexPositions[t[0]]);
    _vertexNormals[t[0]] += n_t;
    _vertexNormals[t[1]] += n_t;
    _vertexNormals[t[2]] += n_t;
  }
  for(unsigned int nIt = 0 ; nIt < _vertexNormals.size() ; ++nIt) {
    // glm::normalize(_vertexNormals[nIt]);
    _vertexNormals[nIt] = glm::normalize(_vertexNormals[nIt]);
  }
}

void Mesh::recomputePerVertexTextureCoordinates()
{
  _vertexTexCoords.clear();
  // Change the following code to compute a proper per-vertex texture coordinates
  _vertexTexCoords.resize(_vertexPositions.size(), glm::vec2(0.0, 0.0));

  float xMin = FLT_MAX, xMax = FLT_MIN;
  float yMin = FLT_MAX, yMax = FLT_MIN;
  for(glm::vec3 &p : _vertexPositions) {
    xMin = std::min(xMin, p[0]);
    xMax = std::max(xMax, p[0]);
    yMin = std::min(yMin, p[1]);
    yMax = std::max(yMax, p[1]);
  }
  for(unsigned int pIt = 0 ; pIt < _vertexTexCoords.size() ; ++pIt) {
    _vertexTexCoords[pIt] = glm::vec2(
      (_vertexPositions[pIt][0] - xMin)/(xMax-xMin),
      (_vertexPositions[pIt][1] - yMin)/(yMax-yMin));
  }
}

void Mesh::addPlan(float square_half_side)
{
  _vertexPositions.push_back(glm::vec3(-square_half_side,-square_half_side, 0));
  _vertexPositions.push_back(glm::vec3(+square_half_side,-square_half_side, 0));
  _vertexPositions.push_back(glm::vec3(+square_half_side,+square_half_side, 0));
  _vertexPositions.push_back(glm::vec3(-square_half_side,+square_half_side, 0));

  _vertexTexCoords.push_back(glm::vec2(0.0, 0.0));
  _vertexTexCoords.push_back(glm::vec2(1.0, 0.0));
  _vertexTexCoords.push_back(glm::vec2(1.0, 1.0));
  _vertexTexCoords.push_back(glm::vec2(0.0, 1.0));

  _vertexNormals.push_back(glm::vec3(0,0, 1));
  _vertexNormals.push_back(glm::vec3(0,0, 1));
  _vertexNormals.push_back(glm::vec3(0,0, 1));
  _vertexNormals.push_back(glm::vec3(0,0, 1));

  _triangleIndices.push_back(
    glm::uvec3(_vertexPositions.size()-4, _vertexPositions.size()-3, _vertexPositions.size()-2));
  _triangleIndices.push_back(
    glm::uvec3(_vertexPositions.size()-4, _vertexPositions.size()-2, _vertexPositions.size()-1));
}

void Mesh::init()
{
  if (_vertexPositions.empty() || _triangleIndices.empty()) {
    throw std::runtime_error("[Mesh::init] Empty mesh (no vertices or no triangles). Did you load the file correctly?");
  }


  glCreateBuffers(1, &_posVbo); // Generate a GPU buffer to store the positions of the vertices
  size_t vertexBufferSize = sizeof(glm::vec3)*_vertexPositions.size(); // Gather the size of the buffer from the CPU-side vector
  glNamedBufferStorage(_posVbo, vertexBufferSize, _vertexPositions.data(), GL_DYNAMIC_STORAGE_BIT); // Create a data store on the GPU

  glCreateBuffers(1, &_normalVbo); // Same for normal
  glNamedBufferStorage(_normalVbo, vertexBufferSize, _vertexNormals.data(), GL_DYNAMIC_STORAGE_BIT);

  glCreateBuffers(1, &_texCoordVbo); // Same for texture coordinates
  size_t texCoordBufferSize = sizeof(glm::vec2)*_vertexTexCoords.size();
  glNamedBufferStorage(_texCoordVbo, texCoordBufferSize, _vertexTexCoords.data(), GL_DYNAMIC_STORAGE_BIT);

  glCreateBuffers(1, &_ibo); // Same for the index buffer, that stores the list of indices of the triangles forming the mesh
  size_t indexBufferSize = sizeof(glm::uvec3)*_triangleIndices.size();
  glNamedBufferStorage(_ibo, indexBufferSize, _triangleIndices.data(), GL_DYNAMIC_STORAGE_BIT);

  glCreateVertexArrays(1, &_vao); // Create a single handle that joins together attributes (vertex positions, normals) and connectivity (triangles indices)
  glBindVertexArray(_vao);

  glEnableVertexAttribArray(0);
  glBindBuffer(GL_ARRAY_BUFFER, _posVbo);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3*sizeof(GLfloat), 0);

  glEnableVertexAttribArray(1);
  glBindBuffer(GL_ARRAY_BUFFER, _normalVbo);
  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 3*sizeof(GLfloat), 0);

  glEnableVertexAttribArray(2);
  glBindBuffer(GL_ARRAY_BUFFER, _texCoordVbo);
  glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 2*sizeof(GLfloat), 0);

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, _ibo);
  glBindVertexArray(0); // Desactive the VAO just created. Will be activated at rendering time.
}

void Mesh::initOldGL()
{
  // Generate a GPU buffer to store the positions of the vertices
  size_t vertexBufferSize = sizeof(glm::vec3)*_vertexPositions.size();
  glGenBuffers(1, &_posVbo);
  glBindBuffer(GL_ARRAY_BUFFER, _posVbo);
  glBufferData(GL_ARRAY_BUFFER, vertexBufferSize, _vertexPositions.data(), GL_DYNAMIC_READ);

  // Same for normal
  glGenBuffers(1, &_normalVbo);
  glBindBuffer(GL_ARRAY_BUFFER, _normalVbo);
  glBufferData(GL_ARRAY_BUFFER, vertexBufferSize, _vertexNormals.data(), GL_DYNAMIC_READ);

  // Same for texture coordinates
  size_t texCoordBufferSize = sizeof(glm::vec2)*_vertexTexCoords.size();
  glGenBuffers(1, &_texCoordVbo);
  glBindBuffer(GL_ARRAY_BUFFER, _texCoordVbo);
  glBufferData(GL_ARRAY_BUFFER, texCoordBufferSize, _vertexTexCoords.data(), GL_DYNAMIC_READ);

  // Same for the index buffer that stores the list of indices of the triangles forming the mesh
  size_t indexBufferSize = sizeof(glm::uvec3)*_triangleIndices.size();
  glGenBuffers(1, &_ibo);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, _ibo);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, indexBufferSize, _triangleIndices.data(), GL_DYNAMIC_READ);

  // Create a single handle that joins together attributes (vertex positions, normals) and connectivity (triangles indices)
  glGenVertexArrays(1, &_vao);
  glBindVertexArray(_vao);

  glEnableVertexAttribArray(0);
  glBindBuffer(GL_ARRAY_BUFFER, _posVbo);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3*sizeof(GLfloat), 0);

  glEnableVertexAttribArray(1);
  glBindBuffer(GL_ARRAY_BUFFER, _normalVbo);
  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 3*sizeof(GLfloat), 0);

  glEnableVertexAttribArray(2);
  glBindBuffer(GL_ARRAY_BUFFER, _texCoordVbo);
  glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 2*sizeof(GLfloat), 0);

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, _ibo);

  glBindVertexArray(0); // Desactive the VAO just created. Will be activated at rendering time.
}

void Mesh::render()
{
  glBindVertexArray(_vao);      // Activate the VAO storing geometry data
  glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(_triangleIndices.size()*3), GL_UNSIGNED_INT, 0);
  // Call for rendering: stream the current GPU geometry through the current GPU program
}

void Mesh::clear()
{
  _vertexPositions.clear();
  _vertexNormals.clear();
  _vertexTexCoords.clear();
  _triangleIndices.clear();
  if(_vao) {
    glDeleteVertexArrays(1, &_vao);
    _vao = 0;
  }
  if(_posVbo) {
    glDeleteBuffers(1, &_posVbo);
    _posVbo = 0;
  }
  if(_normalVbo) {
    glDeleteBuffers(1, &_normalVbo);
    _normalVbo = 0;
  }
  if(_texCoordVbo) {
    glDeleteBuffers(1, &_texCoordVbo);
    _texCoordVbo = 0;
  }
  if(_ibo) {
    glDeleteBuffers(1, &_ibo);
    _ibo = 0;
  }
}

// Loads an OFF mesh file. See https://en.wikipedia.org/wiki/OFF_(file_format)
void loadOFF(const std::string &filename, std::shared_ptr<Mesh> meshPtr)
{
  std::cout << " > Start loading mesh <" << filename << ">" << std::endl;
  meshPtr->clear();
  std::ifstream in(filename.c_str());
  if(!in)
    throw std::ios_base::failure("[Mesh Loader][loadOFF] Cannot open " + filename);
  std::string offString;
  unsigned int sizeV, sizeT, tmp;
  in >> offString >> sizeV >> sizeT >> tmp;
  auto &P = meshPtr->vertexPositions();
  auto &T = meshPtr->triangleIndices();
  P.resize(sizeV);
  T.resize(sizeT);
  size_t tracker = (sizeV + sizeT)/20;
  std::cout << " > [" << std::flush;
  for(unsigned int i=0; i<sizeV; ++i) {
    if(i % tracker == 0)
      std::cout << "-" << std::flush;
    in >> P[i][0] >> P[i][1] >> P[i][2];
  }
  int s;
  for(unsigned int i=0; i<sizeT; ++i) {
    if((sizeV + i) % tracker == 0)
      std::cout << "-" << std::flush;
    in >> s;
    for(unsigned int j=0; j<3; ++j)
      in >> T[i][j];
  }
  std::cout << "]" << std::endl;
  in.close();
  meshPtr->vertexNormals().resize(P.size(), glm::vec3(0.f, 0.f, 1.f));
  meshPtr->vertexTexCoords().resize(P.size(), glm::vec2(0.f, 0.f));
  meshPtr->recomputePerVertexNormals();
  meshPtr->recomputePerVertexTextureCoordinates();
  std::cout << " > Mesh <" << filename << "> loaded" <<  std::endl;
}

struct Vertex {
  glm::vec3 p;
  glm::vec3 n;
  glm::vec2 uv;
};

static glm::vec3 safeNormalize(const glm::vec3& v) {
  float len = glm::length(v);
  if (len < 1e-8f) return glm::vec3(0,1,0);
  return v / len;
}

void loadOBJ(const std::string& filename, std::shared_ptr<Mesh> mesh)
{
  tinyobj::ObjReader reader;
  tinyobj::ObjReaderConfig config;
  config.triangulate = true;      // IMPORTANT
  config.vertex_color = false;

  if (!reader.ParseFromFile(filename, config)) {
    if (!reader.Error().empty())
      throw std::runtime_error(reader.Error());
    throw std::runtime_error("tinyobj failed with no message");
  }
  if (!reader.Warning().empty()) {
    std::cerr << "[tinyobj warn] " << reader.Warning() << std::endl;
  }

  const auto& attrib = reader.GetAttrib();
  const auto& shapes = reader.GetShapes();

  std::vector<Vertex> vertices;
  std::vector<unsigned int> indices;
  vertices.reserve(100000);
  indices.reserve(100000);

  // map unique (v/vt/vn) triplets to a single index
  struct Key { int v, vt, vn; };
  struct KeyHash {
    size_t operator()(Key const& k) const noexcept {
      size_t h1 = std::hash<int>{}(k.v);
      size_t h2 = std::hash<int>{}(k.vt);
      size_t h3 = std::hash<int>{}(k.vn);
      return h1 ^ (h2 << 1) ^ (h3 << 2);
    }
  };
  struct KeyEq {
    bool operator()(Key const& a, Key const& b) const noexcept {
      return a.v==b.v && a.vt==b.vt && a.vn==b.vn;
    }
  };
  std::unordered_map<Key, unsigned int, KeyHash, KeyEq> remap;

  auto getPos = [&](int vi) {
    return glm::vec3(
      attrib.vertices[3*vi+0],
      attrib.vertices[3*vi+1],
      attrib.vertices[3*vi+2]
    );
  };

  auto getNrm = [&](int ni) {
    return glm::vec3(
      attrib.normals[3*ni+0],
      attrib.normals[3*ni+1],
      attrib.normals[3*ni+2]
    );
  };

  auto getUV = [&](int ti) {
    return glm::vec2(
      attrib.texcoords[2*ti+0],
      attrib.texcoords[2*ti+1]
    );
  };

  for (const auto& shape : shapes) {
    size_t index_offset = 0;
    for (size_t f = 0; f < shape.mesh.num_face_vertices.size(); f++) {
      int fv = shape.mesh.num_face_vertices[f];
      // config.triangulate=true => fv should be 3
      for (int v = 0; v < fv; v++) {
        const tinyobj::index_t idx = shape.mesh.indices[index_offset + v];

        Key key{ idx.vertex_index, idx.texcoord_index, idx.normal_index };
        auto it = remap.find(key);
        if (it == remap.end()) {
          Vertex vert{};
          vert.p = getPos(idx.vertex_index);

          if (idx.normal_index >= 0 && !attrib.normals.empty())
            vert.n = getNrm(idx.normal_index);
          else
            vert.n = glm::vec3(0,1,0);

          if (idx.texcoord_index >= 0 && !attrib.texcoords.empty())
            vert.uv = getUV(idx.texcoord_index);
          else
            vert.uv = glm::vec2(0,0);

          unsigned int newIndex = (unsigned int)vertices.size();
          vertices.push_back(vert);
          remap[key] = newIndex;
          indices.push_back(newIndex);
        } else {
          indices.push_back(it->second);
        }
      }
      index_offset += fv;
    }
  }

  // If normals missing, compute per-vertex normals
  bool normalsMissing = attrib.normals.empty();
  if (normalsMissing) {
    std::vector<glm::vec3> acc(vertices.size(), glm::vec3(0));
    for (size_t i = 0; i + 2 < indices.size(); i += 3) {
      unsigned int i0 = indices[i], i1 = indices[i+1], i2 = indices[i+2];
      glm::vec3 p0 = vertices[i0].p, p1 = vertices[i1].p, p2 = vertices[i2].p;
      glm::vec3 n = glm::cross(p1 - p0, p2 - p0);
      acc[i0] += n; acc[i1] += n; acc[i2] += n;
    }
    for (size_t i = 0; i < vertices.size(); ++i)
      vertices[i].n = safeNormalize(acc[i]);
  }

  mesh->clear();

  auto &P  = mesh->vertexPositions();
  auto &N  = mesh->vertexNormals();
  auto &UV = mesh->vertexTexCoords();
  auto &T  = mesh->triangleIndices();

  P.clear(); N.clear(); UV.clear(); T.clear();

  P.reserve(vertices.size());
  N.reserve(vertices.size());
  UV.reserve(vertices.size());

  for (auto &v : vertices) {
    P.push_back(v.p);
    N.push_back(v.n);
    UV.push_back(v.uv);
  }

  // indices is flat: 0,1,2, 3,4,5, ...
  if (indices.size() % 3 != 0)
    throw std::runtime_error("OBJ indices not multiple of 3 (triangulation failed?)");

  T.resize(indices.size() / 3);
  for (size_t i = 0; i < T.size(); ++i) {
    T[i] = glm::uvec3(indices[3*i+0], indices[3*i+1], indices[3*i+2]);
    
  }

  mesh->recomputePerVertexNormals();


  // optional: if you want smoother normals always
  // mesh->recomputePerVertexNormals();

}

