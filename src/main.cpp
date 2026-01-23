// ----------------------------------------------------------------------------
// main.cpp
//
//  Created on: Wed Oct 21 11:32:52 2020
//      Author: Kiwon Um (originally designed by Tamy Boubekeur)
//        Mail: kiwon.um@telecom-paris.fr
//
// Description: IGR202 - Practical - Shadow (DO NOT distribute!)
//
// http://www.opengl-tutorial.org/intermediate-tutorials/tutorial-16-shadow-mapping/
//
// Copyright 2020 Kiwon Um and Tamy Boubekeur
//
// The copyright to the computer program(s) herein is the property of Kiwon Um.
// The program(s) may be used and/or copied only with the written permission of
// Kiwon Um or in accordance with the terms and conditions stipulated in the
// agreement/contract under which the program(s) have been supplied.
// ----------------------------------------------------------------------------

#define _USE_MATH_DEFINES

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/ext.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include <cstdlib>
#include <cstdio>
#include <iostream>
#include <fstream>
#include <ios>
#include <vector>
#include <string>
#include <cmath>
#include <memory>
#include <algorithm>
#include <exception>

#include "Error.h"
#include "ShaderProgram.h"
#include "Camera.h"
#include "Mesh.h"

#include "RayTracer.h"
#include "EnvMap.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

const std::string DEFAULT_MESH_FILENAME("data/frog1.obj");

// window parameters
GLFWwindow *g_window = nullptr;
int g_windowWidth = 1024;
int g_windowHeight = 768;

// pointer to the current camera model
std::shared_ptr<Camera> g_cam;

// camera control variables
float g_meshScale = 1.0; // to update based on the mesh size, so that navigation runs at scale
bool g_rotatingP = false;
bool g_panningP = false;
bool g_zoomingP = false;
double g_baseX = 0.0, g_baseY = 0.0;
glm::vec3 g_baseTrans(0.0);
glm::vec3 g_baseRot(0.0);

// timer
float g_appTimer = 0.0;
float g_appTimerLastColckTime;
bool g_appTimerStoppedP = true;

// TODO: textures
unsigned int g_availableTextureSlot = 0;

//sky view
GLuint g_skyTex = 0;
GLuint g_skyVao = 0;
std::shared_ptr<ShaderProgram> g_skyShader = nullptr;

bool g_doRayTrace = false;



GLuint loadTextureFromFileToGPU(const std::string &filename)
{
  int width, height, numComponents;
  // Loading the image in CPU memory using stbd_image
  stbi_set_flip_vertically_on_load(true);

  unsigned char *data = stbi_load(
    filename.c_str(),
    &width,
    &height,
    &numComponents, // 1 for a 8 bit greyscale image, 3 for 24bits RGB image, 4 for 32bits RGBA image
    0);

  // Create a texture in GPU memory
  GLuint texID;
  glGenTextures(1, &texID);
  glBindTexture(GL_TEXTURE_2D, texID);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  // Uploading the image data to GPU memory
  glTexImage2D(
    GL_TEXTURE_2D,
    0,
    (numComponents == 1 ? GL_RED : numComponents == 3 ? GL_RGB : GL_RGBA), // For greyscale images, we store them in the RED channel
    width,
    height,
    0,
    (numComponents == 1 ? GL_RED : numComponents == 3 ? GL_RGB : GL_RGBA), // For greyscale images, we store them in the RED channel
    GL_UNSIGNED_BYTE,
    data);

  // Generating mipmaps for filtered texture fetch
  glGenerateMipmap(GL_TEXTURE_2D);

  // Freeing the now useless CPU memory
  stbi_image_free(data);
  glBindTexture(GL_TEXTURE_2D, 0); // unbind the texture
  return texID;
}

GLuint loadHDRTexture2D(const std::string& filename) {
  stbi_set_flip_vertically_on_load(true);
  int w, h, n;
  float* data = stbi_loadf(filename.c_str(), &w, &h, &n, 0);
  if(!data) {
    throw std::runtime_error(std::string("Failed to load HDR: ") + filename);
  }

  GLenum format = (n == 4) ? GL_RGBA : GL_RGB;

  GLuint tex;
  glGenTextures(1, &tex);
  glBindTexture(GL_TEXTURE_2D, tex);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, w, h, 0, format, GL_FLOAT, data);

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  stbi_image_free(data);
  glBindTexture(GL_TEXTURE_2D, 0);
  return tex;
}

class FboShadowMap {
public:
  GLuint getTextureId() const { return _depthMapTexture; }

  bool allocate(unsigned int width=1024, unsigned int height=768)
  {
    glGenFramebuffers(1, &_depthMapFbo);
    glBindFramebuffer(GL_FRAMEBUFFER, _depthMapFbo);

    _depthMapTextureWidth = width;
    _depthMapTextureHeight = height;

    // Depth texture. Slower than a depth buffer, but you can sample it later in your shader
    glGenTextures(1, &_depthMapTexture);
    glBindTexture(GL_TEXTURE_2D, _depthMapTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT16, width, height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, _depthMapTexture, 0);

    glDrawBuffer(GL_NONE);      // No color buffers are written.

    if(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE) {
      glBindFramebuffer(GL_FRAMEBUFFER, 0);
      return true;
    } else {
      std::cout << "PROBLEM IN FBO FboShadowMap::allocate(): FBO NOT successfully created" << std::endl;
      glBindFramebuffer(GL_FRAMEBUFFER, 0);
      return false;
    }
  }

  void bindFbo()
  {
    glViewport(0, 0, _depthMapTextureWidth, _depthMapTextureHeight);
    glBindFramebuffer(GL_FRAMEBUFFER, _depthMapFbo);
    glClear(GL_DEPTH_BUFFER_BIT);

    // you can now render the geometry, assuming you have set the view matrix
    // according to the light viewpoint
  }

  void free() { glDeleteFramebuffers(1, &_depthMapFbo); }

  void savePpmFile(std::string const &filename)
  {
    std::ofstream output_image(filename.c_str());

    // READ THE PIXELS VALUES from FBO AND SAVE TO A .PPM FILE
    int i, j, k;
    float *pixels = new float[_depthMapTextureWidth*_depthMapTextureHeight];

    // READ THE CONTENT FROM THE FBO
    glReadBuffer(GL_COLOR_ATTACHMENT0);
    glReadPixels(0, 0, _depthMapTextureWidth, _depthMapTextureHeight, GL_DEPTH_COMPONENT , GL_FLOAT, pixels);

    output_image << "P3" << std::endl;
    output_image << _depthMapTextureWidth << " " << _depthMapTextureHeight << std::endl;
    output_image << "255" << std::endl;

    k = 0;
    for(i=0; i<_depthMapTextureWidth; ++i) {
      for(j=0; j<_depthMapTextureHeight; ++j) {
        output_image <<
          static_cast<unsigned int>(255*pixels[k]) << " " <<
          static_cast<unsigned int>(255*pixels[k]) << " " <<
          static_cast<unsigned int>(255*pixels[k]) << " ";
        k = k+1;
      }
      output_image << std::endl;
    }
    delete [] pixels;
    output_image.close();
  }

private:
  GLuint _depthMapFbo;
  GLuint _depthMapTexture;
  unsigned int _depthMapTextureWidth;
  unsigned int _depthMapTextureHeight;
};


struct Light {
  FboShadowMap shadowMap;
  glm::mat4 depthMVP;
  unsigned int shadowMapTexOnGPU;

  glm::vec3 position;
  glm::vec3 color;
  float intensity;

  void setupCameraForShadowMapping(
    std::shared_ptr<ShaderProgram> shader_shadow_map_Ptr,
    const glm::vec3 scene_center,
    const float scene_radius)
  {
    // TODO: compute the MVP matrix from the light's point of view
  }

  void allocateShadowMapFbo(unsigned int w=800, unsigned int h=600)
  {
    shadowMap.allocate(w, h);
  }
  void bindShadowMap()
  {
    shadowMap.bindFbo();
  }
};


struct Scene {
  std::vector<Light> lights;
  std::shared_ptr<Mesh> back_rock = nullptr;
  std::shared_ptr<Mesh> stage = nullptr;
  std::shared_ptr<Mesh> rock = nullptr;
  std::shared_ptr<Mesh> frog = nullptr;

  //texture
  GLuint back_rockTexture = 0;
  GLuint stageTexture = 0;


  // meshes

  // transformation matrices
  glm::mat4 backRockMat = glm::mat4(1.0);
  glm::mat4 stageMat = glm::mat4(1.0);
  glm::mat4 frogMat = glm::mat4(1.0);

  glm::mat4 rockMat1 = glm::mat4(1.0);
  glm::mat4 rockMat2 = glm::mat4(1.0);
  glm::mat4 rockMat3 = glm::mat4(1.0);
  

  glm::vec3 scene_center = glm::vec3(0);
  float scene_radius = 1.f;

  // shaders to render the meshes and shadow maps
  std::shared_ptr<ShaderProgram> mainShader, shadomMapShader;

  // useful for debug
  bool saveShadowMapsPpm = false;

  void render()
  {

    //<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
    // TODO: first, render the shadow maps
    glEnable(GL_CULL_FACE);

    // shadomMapShader->use();
    // for(int i=0; i<lights.size(); ++i) {
    //   Light &light = lights[i];
    //   light.setupCameraForShadowMapping(shadomMapShader, scene_center, scene_radius*1.5f);
    //   light.bindShadowMap();

    //   // TODO: render the objects in the scene

    //   if(saveShadowMapsPpm) {
    //     light.shadowMap.savePpmFile(std::string("shadom_map_")+std::to_string(i)+std::string(".ppm"));
    //   }
    // }
    // shadomMapShader->stop();
    // saveShadowMapsPpm = false;
    //>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>

    //<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
    // TODO: second, render the scene
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, g_windowWidth, g_windowHeight);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); // Erase the color and z buffers.
    glCullFace(GL_BACK);

    //SKY draw
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);

    g_skyShader->use();
    g_skyShader->set("invView", glm::inverse(g_cam->computeViewMatrix()));
    g_skyShader->set("invProj", glm::inverse(g_cam->computeProjectionMatrix()));

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, g_skyTex);
    g_skyShader->set("skyEquirect", 0);

    glBindVertexArray(g_skyVao);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);

    g_skyShader->stop();

    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);
    // glClear(GL_DEPTH_BUFFER_BIT);



    mainShader->use();

    // camera
    mainShader->set("camPos", g_cam->getPosition());
    mainShader->set("viewMat", g_cam->computeViewMatrix());
    mainShader->set("projMat", g_cam->computeProjectionMatrix());

    // lights
    // for(int i=0; i<lights.size(); ++i) {
    //   Light &light = lights[i];
    //   mainShader->set(std::string("lightSources[")+std::to_string(i)+std::string("].position"), light.position);
    //   mainShader->set(std::string("lightSources[")+std::to_string(i)+std::string("].color"), light.color);
    //   mainShader->set(std::string("lightSources[")+std::to_string(i)+std::string("].intensity"), light.intensity);
    //   mainShader->set(std::string("lightSources[")+std::to_string(i)+std::string("].isActive"), 1);
    // }
    
    Light &L = lights[0];
    mainShader->set("light.position",  L.position);
    mainShader->set("light.color",     L.color);
    mainShader->set("light.intensity", L.intensity);




    // back-wall
    mainShader->set("material.albedo", glm::vec3(0.29, 0.51, 0.82)); // default value if the texture was not loaded
    mainShader->set("modelMat", backRockMat);
    mainShader->set("normMat", glm::mat3(glm::inverseTranspose(backRockMat)));

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, back_rockTexture);
    mainShader->set("material.useTexture", 1);
    mainShader->set("material.albedoTex", 0);

    glDisable(GL_CULL_FACE);
    back_rock->render();
    glEnable(GL_CULL_FACE);
    
    mainShader->set("material.useTexture", 0);
    glBindTexture(GL_TEXTURE_2D, 0);
    

    // stage
    mainShader->set("material.albedo", glm::vec3(0.6f, 0.6f, 0.6f));
    mainShader->set("modelMat", stageMat);
    mainShader->set("normMat", glm::mat3(glm::inverseTranspose(stageMat)));

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, stageTexture);
    mainShader->set("material.useTexture", 1);
    mainShader->set("material.albedoTex", 0);

    stage->render();

    mainShader->set("material.useTexture", 0);
    glBindTexture(GL_TEXTURE_2D, 0);

    //rocks
    auto drawRock = [&](const glm::mat4& M){
      mainShader->set("material.albedo", glm::vec3(0.6f, 0.6f, 0.6f));
      mainShader->set("modelMat", M);
      mainShader->set("normMat", glm::mat3(glm::inverseTranspose(M)));
      // glDisable(GL_CULL_FACE);
      rock->render();
      // glEnable(GL_CULL_FACE);
    };
    drawRock(rockMat1);
    drawRock(rockMat2);
    drawRock(rockMat3);

    // frog
    mainShader->set("material.albedo", glm::vec3(0.6f, 0.6f, 0.6f));
    mainShader->set("modelMat", frogMat);
    mainShader->set("normMat", glm::mat3(glm::inverseTranspose(frogMat)));

    frog->render();
    // std::cout << "frog rendered" << std::endl;

    mainShader->stop();
    //>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
  }
};

Scene g_scene;

void printHelp()
{
  std::cout <<
    "> Help:" << std::endl <<
    "    Mouse commands:" << std::endl <<
    "    * Left button: rotate camera" << std::endl <<
    "    * Middle button: zoom" << std::endl <<
    "    * Right button: pan camera" << std::endl <<
    "    Keyboard commands:" << std::endl <<
    "    * H: print this help" << std::endl <<
    "    * T: toggle animation" << std::endl <<
    "    * S: save shadow maps into PPM files" << std::endl <<
    "    * F1: toggle wireframe/surface rendering" << std::endl <<
    "    * R: ray trace once (writes raytrace.ppm)" << std::endl <<
    "    * ESC: quit the program" << std::endl;
}

// Executed each time the window is resized. Adjust the aspect ratio and the rendering viewport to the current window.
void windowSizeCallback(GLFWwindow *window, int width, int height)
{
  g_windowWidth = width;
  g_windowHeight = height;
  g_cam->setAspectRatio(static_cast<float>(width)/static_cast<float>(height));
  glViewport(0, 0, (GLint)width, (GLint)height); // Dimension of the rendering region in the window
}

// Executed each time a key is entered.
void keyCallback(GLFWwindow *window, int key, int scancode, int action, int mods)
{
  if(action == GLFW_PRESS && key == GLFW_KEY_H) {
    printHelp();
  } else if(action == GLFW_PRESS && key == GLFW_KEY_S) {
    g_scene.saveShadowMapsPpm = true;
  } else if(action == GLFW_PRESS && key == GLFW_KEY_T) {
    g_appTimerStoppedP = !g_appTimerStoppedP;
    if(!g_appTimerStoppedP)
      g_appTimerLastColckTime = static_cast<float>(glfwGetTime());
  } else if(action == GLFW_PRESS && key == GLFW_KEY_F1) {
    GLint mode[2];
    glGetIntegerv(GL_POLYGON_MODE, mode);
    glPolygonMode(GL_FRONT_AND_BACK, mode[1] == GL_FILL ? GL_LINE : GL_FILL);
  } else if(action == GLFW_PRESS && key == GLFW_KEY_ESCAPE) {
    glfwSetWindowShouldClose(window, true); // Closes the application if the escape key is pressed
  } else if(action == GLFW_PRESS && key == GLFW_KEY_R) {
    g_doRayTrace = true;
  }
}

// Called each time the mouse cursor moves
void cursorPosCallback(GLFWwindow *window, double xpos, double ypos)
{
  int width, height;
  glfwGetWindowSize(window, &width, &height);
  const float normalizer = static_cast<float>((width + height)/2);
  const float dx = static_cast<float>((g_baseX - xpos) / normalizer);
  const float dy = static_cast<float>((ypos - g_baseY) / normalizer);
  if(g_rotatingP) {
    const glm::vec3 dRot(-dy*M_PI, dx*M_PI, 0.0);
    g_cam->setRotation(g_baseRot + dRot);
  } else if(g_panningP) {
    g_cam->setPosition(g_baseTrans + g_meshScale*glm::vec3(dx, dy, 0.0));
  } else if(g_zoomingP) {
    g_cam->setPosition(g_baseTrans + g_meshScale*glm::vec3(0.0, 0.0, dy));
  }
}

// Called each time a mouse button is pressed
void mouseButtonCallback(GLFWwindow *window, int button, int action, int mods)
{
  if(button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
    if(!g_rotatingP) {
      g_rotatingP = true;
      glfwGetCursorPos(window, &g_baseX, &g_baseY);
      g_baseRot = g_cam->getRotation();
    }
  } else if(button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_RELEASE) {
    g_rotatingP = false;
  } else if(button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_PRESS) {
    if(!g_panningP) {
      g_panningP = true;
      glfwGetCursorPos(window, &g_baseX, &g_baseY);
      g_baseTrans = g_cam->getPosition();
    }
  } else if(button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_RELEASE) {
    g_panningP = false;
  } else if(button == GLFW_MOUSE_BUTTON_MIDDLE && action == GLFW_PRESS) {
    if(!g_zoomingP) {
      g_zoomingP = true;
      glfwGetCursorPos(window, &g_baseX, &g_baseY);
      g_baseTrans = g_cam->getPosition();
    }
  } else if(button == GLFW_MOUSE_BUTTON_MIDDLE && action == GLFW_RELEASE) {
    g_zoomingP = false;
  }
}

void initGLFW()
{
  // Initialize GLFW, the library responsible for window management
  if(!glfwInit()) {
    std::cerr << "ERROR: Failed to init GLFW" << std::endl;
    std::exit(EXIT_FAILURE);
  }

  // Before creating the window, set some option flags
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_RESIZABLE, GL_TRUE);

  // Create the window
  g_window = glfwCreateWindow(g_windowWidth, g_windowHeight, "IGR202 - Practical - Shadow", nullptr, nullptr);
  if(!g_window) {
    std::cerr << "ERROR: Failed to open window" << std::endl;
    glfwTerminate();
    std::exit(EXIT_FAILURE);
  }

  // Load the OpenGL context in the GLFW window using GLAD OpenGL wrangler
  glfwMakeContextCurrent(g_window);

  // not mandatory for all, but MacOS X
  glfwGetFramebufferSize(g_window, &g_windowWidth, &g_windowHeight);

  // Connect the callbacks for interactive control
  glfwSetWindowSizeCallback(g_window, windowSizeCallback);
  glfwSetKeyCallback(g_window, keyCallback);
  glfwSetCursorPosCallback(g_window, cursorPosCallback);
  glfwSetMouseButtonCallback(g_window, mouseButtonCallback);
}

void clear();
void exitOnCriticalError(const std::string &message)
{
  std::cerr << "> [Critical error]" << message << std::endl;
  std::cerr << "> [Clearing resources]" << std::endl;
  clear();
  std::cerr << "> [Exit]" << std::endl;
  std::exit(EXIT_FAILURE);
}

void initOpenGL()
{
  // Load extensions for modern OpenGL
  if(!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    exitOnCriticalError("[Failed to initialize OpenGL context]");

  glEnable(GL_DEBUG_OUTPUT);    // Modern error callback functionality
  glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS); // For recovering the line where the error occurs, set a debugger breakpoint in DebugMessageCallback
  glDebugMessageCallback(debugMessageCallback, 0); // Specifies the function to call when an error message is generated.

  glCullFace(GL_BACK); // Specifies the faces to cull (here the ones pointing away from the camera)
  glEnable(GL_CULL_FACE); // Enables face culling (based on the orientation defined by the CW/CCW enumeration).
  glDepthFunc(GL_LESS);   // Specify the depth test for the z-buffer
  glEnable(GL_DEPTH_TEST);      // Enable the z-buffer test in the rasterization
  glClearColor(1.0f, 1.0f, 1.0f, 1.0f); // specify the background color, used any time the framebuffer is cleared

  // Loads and compile the programmable shader pipeline
  try {
    g_scene.mainShader = ShaderProgram::genBasicShaderProgram("src/vertexShader.glsl", "src/fragmentShader.glsl");

    // sky shader + texture
    g_skyShader = ShaderProgram::genBasicShaderProgram("src/vertexShaderSky.glsl",
                                                   "src/fragmentShaderSky.glsl");
    g_skyTex = loadHDRTexture2D("data/farmland_overcast_4k.hdr");
    glGenVertexArrays(1, &g_skyVao);



    g_scene.mainShader->stop();

  } catch(std::exception &e) {
    exitOnCriticalError(std::string("[Error loading shader program]") + e.what());
  }
  // try {
  //   g_scene.shadomMapShader = ShaderProgram::genBasicShaderProgram("src/vertexShaderShadowMap.glsl", "src/fragmentShaderShadowMap.glsl");
  //   g_scene.shadomMapShader->stop();

  // } catch(std::exception &e) {
  //   exitOnCriticalError(std::string("[Error loading shader program]") + e.what());
  // }
}

void initScene(const std::string &meshFilename)
{
  // Init camera
  int width, height;
  glfwGetWindowSize(g_window, &width, &height);
  g_cam = std::make_shared<Camera>();
  g_cam->setAspectRatio(static_cast<float>(width)/static_cast<float>(height));

  // Load meshes in the scene
  {

    // rock back-wall
    g_scene.back_rock = std::make_shared<Mesh>();
    try {
      loadOBJ("data/rock_back.obj", g_scene.back_rock);
    } catch(std::exception &e) {
      exitOnCriticalError(std::string("[Error loading back_rock mesh]") + e.what());
    }
    g_scene.back_rock->init();

    // stage
    g_scene.stage = std::make_shared<Mesh>();
    try{
      loadOBJ("data/stage.obj", g_scene.stage);
    }catch(std::exception &e){
      exitOnCriticalError(std::string("[Error loading stage mesh]") + e.what());
    }
    g_scene.stage->init();

    //rocks
    g_scene.rock = std::make_shared<Mesh>();
    try{
      loadOBJ("data/rock.obj", g_scene.rock);
    }catch(std::exception &e){
      exitOnCriticalError(std::string("[Error loading rock mesh]") + e.what());
    }
    g_scene.rock->init();

    //frog
    g_scene.frog = std::make_shared<Mesh>();
    try{
      loadOBJ("data/frog_decimated.obj", g_scene.frog);
    }catch(std::exception &e){
      exitOnCriticalError(std::string("[Error loading frog mesh]") + e.what());
    }
    g_scene.frog->init();

    
    glm::vec3 Stage_Position(-0.05f, -0.55f, -5.5f);
    glm::vec3 Full_Object_Scale(0.129f);

    glm::mat4 stageTranslate = glm::translate(glm::mat4(1.0f), Stage_Position);
    glm::mat4 stageRotate = glm::rotate(glm::mat4(1.0f), glm::radians(-35.0f), glm::vec3(0,1,0));
    glm::mat4 stageScale = glm::scale(glm::mat4(1.0f), Full_Object_Scale);

    g_scene.stageMat = 
        stageTranslate *
        stageRotate *
        stageScale;

    glm::vec3 Wall_Position = Stage_Position + glm::vec3(0.0f, 0.05f, 0.0f);
    glm::mat4 wallTranslate = glm::translate(glm::mat4(1.0f), Wall_Position);

    g_scene.backRockMat =
        wallTranslate *
        stageRotate *
        stageScale;

    glm::mat4 rockScale = glm::scale(glm::mat4(1.0f), glm::vec3(0.2f));
    g_scene.rockMat1 = stageTranslate * glm::translate(glm::mat4(1.0f), glm::vec3(-0.2f, -0.7f, 1.8f)) * rockScale;
    g_scene.rockMat2 = stageTranslate * glm::translate(glm::mat4(1.0f), glm::vec3(-1.2f, -0.7f, 2.0f)) * rockScale;
    g_scene.rockMat3 = stageTranslate * glm::translate(glm::mat4(1.0f), glm::vec3(-0.2f, -10.7f, 1.5f)) * rockScale;

    glm::mat4 frogRotate = glm::rotate(glm::mat4(1.0f), glm::radians(-125.0f), glm::vec3(0,1,0));

    g_scene.frogMat = stageTranslate * glm::translate(glm::mat4(1.0f), glm::vec3(-1.2f, -0.46f, 1.95f)) * frogRotate * glm::scale(glm::mat4(1.0f), glm::vec3(0.015f));
  }

  // TODO: Load and setup textures
  GLuint back_rockTex = loadTextureFromFileToGPU("data/rock_back_texture.png");
  g_scene.back_rockTexture = back_rockTex;

  GLuint stageTex = loadTextureFromFileToGPU("data/wood_table_diff_2k.jpg");
  g_scene.stageTexture = stageTex;




  // // Setup lights
  // const glm::vec3 pos[3] = {
  //   glm::vec3(0.0, 1.0, 1.0),
  //   glm::vec3(0.3, 2.0, 0.4),
  //   glm::vec3(0.2, 0.4, 2.0),
  // };
  // const glm::vec3 col[3] = {
  //   glm::vec3(1.0, 1.0, 1.0),
  //   glm::vec3(1.0, 1.0, 0.8),
  //   glm::vec3(1.0, 1.0, 0.8),
  // };
  // unsigned int shadow_map_width=2000, shadow_map_height=2000; // play with these parameters
  
  
  // for(int i=0; i<3; ++i) {
  //   g_scene.lights.push_back(Light());
  //   Light &a_light = g_scene.lights[g_scene.lights.size() - 1];
  //   a_light.position = pos[i];
  //   a_light.color = col[i];
  //   a_light.intensity = 0.5f;
    // a_light.shadowMapTexOnGPU = g_availableTextureSlot;
    // glActiveTexture(GL_TEXTURE0 + a_light.shadowMapTexOnGPU);
    // a_light.allocateShadowMapFbo(shadow_map_width, shadow_map_height);
    // ++g_availableTextureSlot;
  // }

  g_scene.lights.clear();
  g_scene.lights.push_back(Light());
  Light &L = g_scene.lights[0];

  L.position  = glm::vec3(-2.0f, 1.1f, -3.5f);
  L.color     = glm::vec3(1.0f, 1.0f, 1.0f);
  L.intensity = 1.0f;


  // Adjust the camera to the mesh
  g_scene.scene_center = glm::vec3(0.0f);
  g_scene.scene_radius = 1.0f;
  g_meshScale = g_scene.scene_radius;


  g_cam->setPosition(g_scene.scene_center + glm::vec3(0.0, 0.0, 3.0*g_meshScale));
  g_cam->setNear(g_meshScale/100.f);
  g_cam->setFar(12.0*g_meshScale);
}

static void appendMeshToRTScene(RTScene& rt, const Mesh& mesh, const glm::mat4& modelMat, int matId){
  glm::mat3 normalMat = glm::transpose(glm::inverse(glm::mat3(modelMat)));

  const auto& P = mesh.vertexPositions();
  const auto& N = mesh.vertexNormals();
  const auto& T = mesh.triangleIndices();

  for (size_t i = 0; i < T.size(); ++i) {
    glm::uvec3 triIdx = T[i];

    auto xformP = [&](uint32_t idx){
      return glm::vec3(modelMat * glm::vec4(P[idx], 1.f));
    };
    auto xformN = [&](uint32_t idx){
      return glm::normalize(normalMat * N[idx]);
    };

    RTTriangle tri;
    tri.p0 = xformP(triIdx[0]);
    tri.p1 = xformP(triIdx[1]);
    tri.p2 = xformP(triIdx[2]);
    tri.n0 = xformN(triIdx[0]);
    tri.n1 = xformN(triIdx[1]);
    tri.n2 = xformN(triIdx[2]);
    tri.matId = matId;

    rt.tris.push_back(tri);
  }

}


void init(const std::string &meshFilename)
{
  initGLFW();                   // Windowing system
  initOpenGL();                 // OpenGL Context and shader pipeline
  initScene(meshFilename);      // Actual g_scene to render
}

void clear()
{
  g_cam.reset();
  g_scene.mainShader.reset();
  g_scene.shadomMapShader.reset();
  glfwDestroyWindow(g_window);
  glfwTerminate();
}

// The main rendering call
void render()
{
  g_scene.render();
}

// Update any accessible variable based on the current time
void update(float currentTime)
{
  if(!g_appTimerStoppedP) {
    // Animate any entity of the program here
    float dt = currentTime - g_appTimerLastColckTime;
    g_appTimerLastColckTime = currentTime;
    g_appTimer += dt;
    // <---- Update here what needs to be animated over time ---->
  }
}

void usage(const char *command)
{
  std::cerr << "Usage : " << command << " [<file.off>]" << std::endl;
  std::exit(EXIT_FAILURE);
}

int main(int argc, char **argv)
{
  if(argc > 2) usage(argv[0]);
  // Your initialization code (user interface, OpenGL states, scene with geometry, material, lights, etc)
  init(argc==1 ? DEFAULT_MESH_FILENAME : argv[1]);
  while(!glfwWindowShouldClose(g_window)) {
    update(static_cast<float>(glfwGetTime()));
    render();

    if (g_doRayTrace) {
      g_doRayTrace = false;
      int W = 800, H = 600;

      RTScene rt;
      
      rt.mats.clear();

      rt.mats.push_back(RTMaterial());
      rt.mats.back().albedo = glm::vec3(0.8f);
      rt.mats.back().shadowCatcher = false;

      int matRock = (int)rt.mats.size();
      rt.mats.push_back(RTMaterial());
      rt.mats.back().albedo = glm::vec3(0.65f);
      rt.mats.back().shadowCatcher = false;

      int matWood = (int)rt.mats.size();
      rt.mats.push_back(RTMaterial());
      rt.mats.back().albedo = glm::vec3(0.6f, 0.45f, 0.25f);
      rt.mats.back().shadowCatcher = false;

      int matFrog = (int)rt.mats.size();
      rt.mats.push_back(RTMaterial());
      rt.mats.back().albedo = glm::vec3(0.35f, 0.75f, 0.35f);
      rt.mats.back().shadowCatcher = false;


      appendMeshToRTScene(rt, *g_scene.back_rock, g_scene.backRockMat, matRock);
      appendMeshToRTScene(rt, *g_scene.stage, g_scene.stageMat, matWood);
      appendMeshToRTScene(rt, *g_scene.rock, g_scene.rockMat1, matRock);
      appendMeshToRTScene(rt, *g_scene.rock, g_scene.rockMat2, matRock);
      appendMeshToRTScene(rt, *g_scene.frog, g_scene.frogMat, matFrog);

      RTCamera cam;
      cam.pos = g_cam->getPosition();
      cam.invView = glm::inverse(g_cam->computeViewMatrix());
      cam.fovYDegrees = g_cam->getFov();
      cam.aspect = g_cam->getAspectRatio();

      glm::mat4 invV = cam.invView;
      glm::vec3 right = glm::vec3(invV[0]);
      glm::vec3 up = glm::vec3(invV[1]);
      glm::vec3 forward = -glm::vec3(invV[2]);

      RTLight L;
      L.position  = cam.pos + (-1.5f)*right + (1.1f)*up + (3.0f)*forward;
      L.color     = glm::vec3(1.f);
      L.intensity = 20.0f;
      
      
      EnvMap env;
      env.loadHDR("data/farmland_overcast_4k.hdr");


      RayTracer tracer(W, H);
      tracer.setEnvMap(&env);
      tracer.buildBVH(rt);

      auto pixels = tracer.render(rt, cam, L);
      RayTracer::savePPM("raytrace.ppm", pixels, W, H);

      std::cout << "[RayTrace] wrote raytrace.ppm\n";
    }


    glfwSwapBuffers(g_window);
    glfwPollEvents();
  }
  clear();
  std::cout << " > Quit" << std::endl;
  return EXIT_SUCCESS;
}
