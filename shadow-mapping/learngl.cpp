#define UNUSED(expr) (void)(expr)

#include <fstream>
#include <iostream>
#include <math.h>
#include <sstream>
#include <string>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

extern "C" {
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <SOIL/SOIL.h>
}

#include "shader.h"
#include "perspectivecamera.h"

// Window constants for the initial window size.
const GLuint kWindowWidth = 800;
const GLuint kWindowHeight = 600;

// Number of default samples to use with MSAA.
const GLuint kMSAASamples = 8;

// Shadow depth map size.
const GLuint kShadowWidth = 1024;
const GLuint kShadowHeight = 1024;

// Positions all containers
const glm::vec3 cubePositions[] = {
  glm::vec3( 0.0f,  0.0f,  0.0f),
  glm::vec3( 2.0f,  5.0f, -15.0f),
  glm::vec3(-1.5f, -2.2f, -2.5f),
  glm::vec3(-3.8f, -2.0f, -12.3f),
  glm::vec3( 2.4f, -0.4f, -3.5f),
  glm::vec3(-1.7f,  3.0f, -7.5f),
  glm::vec3( 1.3f, -2.0f, -2.5f),
  glm::vec3( 1.5f,  2.0f, -2.5f),
  glm::vec3( 1.5f,  0.2f, -1.5f),
  glm::vec3(-1.3f,  1.0f, -1.5f)
};

const glm::vec3 directionalLightDir(0.0f, 1.0f, 0.0f);

const glm::vec3 pointLightPositions[] = {
  glm::vec3( 0.7f,  0.2f,  2.0f),
  glm::vec3( 2.3f, -3.3f, -4.0f),
  glm::vec3(-4.0f,  2.0f, -12.0f),
  glm::vec3( 0.0f,  0.0f, -3.0f)
};

glm::mat4 model;
glm::mat3 normal;

// Perspecitve camera for 3D fun.
PerspectiveCamera camera;

// Aquire the initial x and y positions for mouse tracking (centered).
GLfloat lastX = kWindowWidth / 2, lastY = kWindowHeight / 2;

// Camera field of view.
GLfloat startFov = camera.fov;
GLfloat targetFov = camera.fov;
GLfloat fovTime = 0.0f;

// Screen width and height for calculating the projection matrix. The window
// width and height cannot be used since the size of the framebuffer may vary
// on HiDPi screens (retina).
GLfloat screenWidth, screenHeight;

// State to prevent the FPS camera from snapping in an odd direction when the
// window is focused for the first time.
bool firstMouseEvent = true;

// Keyboard state.
bool keys[1024];

// Global shaders (compiled later).
Shader shader, postShader;

// Global textures (loaded later).
GLuint containerTexture, containerSpecular, containerEmission;

void setupMatrices();
void drawContainers(GLuint VAO, Shader shader, bool shadowMap);

// Utility functions.
GLuint loadTexture(std::string filepath);
void move(GLfloat delta);
GLfloat easeOutQuart(GLfloat t, GLfloat b, GLfloat c, GLfloat d);

// Callbacks.
void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mode);
void mouseCallback(GLFWwindow* window, double xpos, double ypos);
void scrollCallback(GLFWwindow* window, double xoffset, double yoffset);

int main() {
  // Initialize GLFW and set some hints that will create an OpenGL 3.3 context
  // using core profile.
  glfwInit();
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
  glfwWindowHint(GLFW_SAMPLES, 4);
  glfwWindowHint(GLFW_RESIZABLE, GL_FALSE);

  // Create a fixed 800x600 window that is not resizable.
  GLFWwindow* window = glfwCreateWindow(kWindowWidth, kWindowHeight, "LearnGL", nullptr, nullptr);
  if (window == nullptr) {
    std::cerr << "Failed to created GLFW window" << std::endl;
    glfwTerminate();
    return 1;
  }

  // Create an OpenGL context and pass callbacks to GLFW.
  glfwMakeContextCurrent(window);
  glfwSetKeyCallback(window, keyCallback);
  glfwSetCursorPosCallback(window, mouseCallback);
  glfwSetScrollCallback(window, scrollCallback);

  // Lock the mouse in the window.
  glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

  glewExperimental = GL_TRUE;
  if (glewInit() != GLEW_OK) {
    std::cerr << "Failed to initialize GLEW" << std::endl;
    glfwTerminate();
    return 1;
  }

  // Create a viewport the same size as the window. glfwGetFramebufferSize is
  // used rather than the size constants since some windowing systems have a
  // discrepancy between window size and framebuffer size
  // (e.g HiDPi screen coordinates),
  int fbWidth, fbHeight;
  glfwGetFramebufferSize(window, &fbWidth, &fbHeight);
  glViewport(0, 0, fbWidth, fbHeight);

  // Enable multisampling for using MSAA.
  glEnable(GL_MULTISAMPLE);

  // Enabled SRGB for gamma correction.
  glEnable(GL_FRAMEBUFFER_SRGB);

  // Enable use of the depth buffer since we're working on 3D and want to
  // prevent overlapping polygon artifacts.
  glEnable(GL_DEPTH_TEST);

  // Read and compile the vertex and fragment shaders using
  // the shader helper class.
  shader = Shader("glsl/vertex.glsl", "glsl/fragment.glsl", "glsl/geometry.glsl");
  postShader = Shader("glsl/post_vert.glsl", "glsl/post_frag.glsl");

  containerTexture  = loadTexture("assets/container2.png");
  containerSpecular = loadTexture("assets/container2_specular.png");
  containerEmission = loadTexture("assets/matrix.jpg");

  // Container mesh data.
  GLfloat vertices[] = {
    // Vertices          // Normals           // UVs
    -0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  0.0f, 0.0f,
     0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  1.0f, 0.0f,
     0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  1.0f, 1.0f,
     0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  1.0f, 1.0f,
    -0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  0.0f, 1.0f,
    -0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  0.0f, 0.0f,

    -0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  0.0f, 0.0f,
     0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  1.0f, 0.0f,
     0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  1.0f, 1.0f,
     0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  1.0f, 1.0f,
    -0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  0.0f, 1.0f,
    -0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  0.0f, 0.0f,

    -0.5f,  0.5f,  0.5f, -1.0f,  0.0f,  0.0f,  1.0f, 0.0f,
    -0.5f,  0.5f, -0.5f, -1.0f,  0.0f,  0.0f,  1.0f, 1.0f,
    -0.5f, -0.5f, -0.5f, -1.0f,  0.0f,  0.0f,  0.0f, 1.0f,
    -0.5f, -0.5f, -0.5f, -1.0f,  0.0f,  0.0f,  0.0f, 1.0f,
    -0.5f, -0.5f,  0.5f, -1.0f,  0.0f,  0.0f,  0.0f, 0.0f,
    -0.5f,  0.5f,  0.5f, -1.0f,  0.0f,  0.0f,  1.0f, 0.0f,

     0.5f,  0.5f,  0.5f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f,
     0.5f,  0.5f, -0.5f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f,
     0.5f, -0.5f, -0.5f,  1.0f,  0.0f,  0.0f,  0.0f, 1.0f,
     0.5f, -0.5f, -0.5f,  1.0f,  0.0f,  0.0f,  0.0f, 1.0f,
     0.5f, -0.5f,  0.5f,  1.0f,  0.0f,  0.0f,  0.0f, 0.0f,
     0.5f,  0.5f,  0.5f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f,

    -0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,  0.0f, 1.0f,
     0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,  1.0f, 1.0f,
     0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,  1.0f, 0.0f,
     0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,  1.0f, 0.0f,
    -0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,  0.0f, 0.0f,
    -0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,  0.0f, 1.0f,

    -0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,  0.0f, 1.0f,
     0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,  1.0f, 1.0f,
     0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,  1.0f, 0.0f,
     0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,  1.0f, 0.0f,
    -0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,  0.0f, 0.0f,
    -0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,  0.0f, 1.0f
  };

  // Points for the geometry shader tutorial.
  GLfloat points[] = {
    -0.5f,  0.5f, 1.0f, 0.0f, 0.0f, // Top-left
     0.5f,  0.5f, 0.0f, 1.0f, 0.0f, // Top-right
     0.5f, -0.5f, 0.0f, 0.0f, 1.0f, // Bottom-right
    -0.5f, -0.5f, 1.0f, 1.0f, 0.0f  // Bottom-left
  };

  // Create and bind a framebuffer.
  GLuint FBO;
  glGenFramebuffers(1, &FBO);
  glBindFramebuffer(GL_FRAMEBUFFER, FBO);

  // Create an empty texture to be attached to the framebuffer.
  // Give a null pointer to glTexImage2D since we want an empty texture.
  GLuint frameColorBufferMultiSampled;
  glGenTextures(1, &frameColorBufferMultiSampled);
  glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, frameColorBufferMultiSampled);
  glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, kMSAASamples, GL_RGB,
      fbWidth, fbHeight, GL_TRUE);
  glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, 0);

  // Attach the texture to the framebuffer.
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
      GL_TEXTURE_2D_MULTISAMPLE, frameColorBufferMultiSampled, 0);

  // Create a renderbuffer to hold our depth and stencil buffers with a size
  // of the window's framebuffer size.
  GLuint RBO;
  glGenRenderbuffers(1, &RBO);
  glBindRenderbuffer(GL_RENDERBUFFER, RBO);
  glRenderbufferStorageMultisample(GL_RENDERBUFFER, kMSAASamples,
      GL_DEPTH24_STENCIL8, fbWidth, fbHeight);
  glBindRenderbuffer(GL_RENDERBUFFER, 0);

  // Attach the render buffer (provides depth and stencil) to the framebuffer.
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
      GL_RENDERBUFFER, RBO);

  // Panic if the framebuffer is somehow incomplete at this stage. This should
  // never happen if we attached the texture but it's good practice to check.
  if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
    std::cerr << "ERROR: Framebuffer is not complete!" << std::endl;
    glfwTerminate();
    return 1;
  }
  glBindFramebuffer(GL_FRAMEBUFFER, 0);

  // Generate texture for intermediate stage.
  GLuint screenTexture;
  glGenTextures(1, &screenTexture);
  glBindTexture(GL_TEXTURE_2D, screenTexture);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, fbWidth, fbHeight, 0, GL_RGB,
      GL_UNSIGNED_BYTE, nullptr);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glBindTexture(GL_TEXTURE_2D, 0);

  // Second framebuffer for post.
  GLuint intermediateFBO;
  glGenFramebuffers(1, &intermediateFBO);
  glBindFramebuffer(GL_FRAMEBUFFER, intermediateFBO);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
      screenTexture, 0);

  // Panic if the framebuffer is somehow incomplete at this stage. This should
  // never happen if we attached the texture but it's good practice to check.
  if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
    std::cerr << "ERROR: Framebuffer is not complete!" << std::endl;
    glfwTerminate();
    return 1;
  }
  glBindFramebuffer(GL_FRAMEBUFFER, 0);

  // Create a texture to hold the depth map data.
  // The depth map is used for shadow mapping.
  GLuint depthMap;
  glGenTextures(1, &depthMap);
  glBindTexture(GL_TEXTURE_2D, depthMap);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, kShadowWidth,
      kShadowHeight, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  glBindTexture(GL_TEXTURE_2D, 0);

  GLuint depthMapFBO;
  glGenFramebuffers(1, &depthMapFBO);
  glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthMap, 0);
  glDrawBuffer(GL_NONE);
  glReadBuffer(GL_NONE);

  // Panic if the framebuffer is somehow incomplete at this stage. This should
  // never happen if we attached the texture but it's good practice to check.
  if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
    std::cerr << "ERROR: Framebuffer is not complete!" << std::endl;
    glfwTerminate();
    return 1;
  }
  glBindFramebuffer(GL_FRAMEBUFFER, 0);

  // Create a VBO to store the vertex data, an EBO to store indice data, and
  // create a VAO to retain our vertex attribute pointers.
  GLuint VBO, VAO;
  glGenVertexArrays(1, &VAO);
  glGenBuffers(1, &VBO);

  // Fill the VBO and set vertex attributes.
  glBindVertexArray(VAO);
  glBindBuffer(GL_ARRAY_BUFFER, VBO);
  glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(GLfloat), (GLvoid*)0);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(GLfloat), (GLvoid*)(3 * sizeof(GLfloat)));
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(GLfloat), (GLvoid*)(6 * sizeof(GLfloat)));
  glEnableVertexAttribArray(2);
  glBindVertexArray(0);

  // Create a lamp box thing using the existing container VBO.
  GLuint lightVAO;
  glGenVertexArrays(1, &lightVAO);

  // Use the container's VBO and set vertex attributes.
  glBindVertexArray(lightVAO);
  glBindBuffer(GL_ARRAY_BUFFER, VBO);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(GLfloat), (GLvoid*)0);
  glEnableVertexAttribArray(0);
  glBindVertexArray(0);

  // Vertex attributes for the frame quad in NDC.
  GLfloat frameVertices[] = {
    // Positions  // UVs
    -1.0f,  1.0f,  0.0f, 1.0f,
    -1.0f, -1.0f,  0.0f, 0.0f,
     1.0f, -1.0f,  1.0f, 0.0f,

    -1.0f,  1.0f,  0.0f, 1.0f,
     1.0f, -1.0f,  1.0f, 0.0f,
     1.0f,  1.0f,  1.0f, 1.0f
  };

  // Create a VBO and VAO for the post-processing step.
  GLuint frameVBO, frameVAO;
  glGenVertexArrays(1, &frameVAO);
  glGenBuffers(1, &frameVBO);

  glBindVertexArray(frameVAO);
  glBindBuffer(GL_ARRAY_BUFFER, frameVBO);
  glBufferData(GL_ARRAY_BUFFER, sizeof(frameVertices), frameVertices,
      GL_STATIC_DRAW);
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat),
      (GLvoid*)0);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat),
      (GLvoid*)(2 * sizeof(GLfloat)));
  glEnableVertexAttribArray(1);
  glBindVertexArray(0);

  // Create a VBO and VAO for the geometry shader test. The VBO will contain
  // only the position.
  GLuint pointsVBO, pointsVAO;
  glGenVertexArrays(1, &pointsVAO);
  glGenBuffers(1, &pointsVBO);

  glBindVertexArray(pointsVAO);
  glBindBuffer(GL_ARRAY_BUFFER, pointsVBO);
  glBufferData(GL_ARRAY_BUFFER, sizeof(points), points, GL_STATIC_DRAW);
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat),
      (GLvoid*)0);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat),
      (GLvoid*)(2 * sizeof(GLfloat)));
  glEnableVertexAttribArray(1);
  glBindVertexArray(0);

  // Create a perspective camera to fit the viewport.
  screenWidth = (GLfloat)fbWidth;
  screenHeight = (GLfloat)fbHeight;
  camera = PerspectiveCamera(
    glm::vec3(0.0f, 0.0f, 3.0f),
    glm::vec3(0.0f, glm::radians(-90.0f), 0.0f),
    glm::radians(45.0f),
    screenWidth / screenHeight,
    0.1f,
    100.0f
  );

  GLfloat delta = 0.0f;
  GLfloat lastFrame = 0.0f;

  // Render loop.
  while (!glfwWindowShouldClose(window)) {
    GLfloat currentFrame = glfwGetTime();
    delta = currentFrame - lastFrame;
    lastFrame = currentFrame;

    // Check and call events.
    glfwPollEvents();
    move(delta);

    // Render to the depth map for shadow mapping.
    glViewport(0, 0, kShadowWidth, kShadowHeight);
    glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
    glClear(GL_DEPTH_BUFFER_BIT);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // Bind the off screen framebuffer (for post-processing) and clear the
    // screen to a nice blue color.
    glViewport(0, 0, fbWidth, fbHeight);
    glBindFramebuffer(GL_FRAMEBUFFER, FBO);
    glClearColor(0.1f, 0.15f, 0.15f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);

    // Update the time counter for the camera zoom.
    const GLfloat limitTime = 1.0f;
    fovTime += delta;
    if (fovTime > limitTime) {
      fovTime = limitTime;
    }

    // Update the perspective to account for changes in fov.
    // Used by the scroll to zoom feature.
    camera.fov = easeOutQuart(fovTime, startFov, (startFov - targetFov) * -1, limitTime);
    camera.update();

    shader.use();
    setupMatrices();
    drawContainers(VBO, shader, false);

    glBindFramebuffer(GL_READ_FRAMEBUFFER, FBO);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, intermediateFBO);
    glBlitFramebuffer(0, 0, fbWidth, fbHeight, 0, 0, fbWidth, fbHeight,
        GL_COLOR_BUFFER_BIT, GL_NEAREST);

    // Unbind the offscreen framebuffer containing the unprocessed frame.
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glDisable(GL_DEPTH_TEST);

    postShader.use();
    glBindVertexArray(frameVAO);

    // Send the texture sampler to the shader.
    GLuint frameTexture = glGetUniformLocation(postShader.program, "frameTexture");
    glUniform1i(frameTexture, 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, screenTexture);

    // Render the color buffer in the framebuffer to the quad with post shader.
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);

    // Swap buffers used for double buffering.
    glfwSwapBuffers(window);
  }

  // Destroy the off screen framebuffer.
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glDeleteFramebuffers(1, &FBO);

  // Properly deallocate the VBO and VAO.
  glDeleteVertexArrays(1, &VAO);
  glDeleteBuffers(1, &VBO);

  // Terminate GLFW and clean any resources before exiting.
  glfwTerminate();

  return 0;
}

void setupMatrices() {
  // Pass the view and projection matrices from the camera.
  GLuint viewMatrix = glGetUniformLocation(shader.program, "view");
  glUniformMatrix4fv(viewMatrix, 1, GL_FALSE, glm::value_ptr(camera.view));
  GLuint projectionMatrix = glGetUniformLocation(shader.program, "projection");
  glUniformMatrix4fv(projectionMatrix, 1, GL_FALSE, glm::value_ptr(camera.projection));
}

void drawContainers(GLuint VAO, Shader shader, bool shadowMap) {
  // Bind the VAO and shader.
  glBindVertexArray(VAO);

  if (!shadowMap) {
    // Generate light colors.
    glm::vec3 lightColor(1.0f, 1.0f, 1.0f);

    // Directional light
    glUniform3f(glGetUniformLocation(shader.program, "dirLight.direction"), 0.0f, -1.0f, 0.0f);
    glUniform3f(glGetUniformLocation(shader.program, "dirLight.ambient"), 0.05f, 0.05f, 0.05f);
    glUniform3f(glGetUniformLocation(shader.program, "dirLight.diffuse"), 1.0f, 1.0f, 1.0f);
    glUniform3f(glGetUniformLocation(shader.program, "dirLight.specular"), 0.0f, 0.0f, 0.0f);

    for (int i = 0; i < 4; i++) {
      // Uniform arrays can have values set using an index via string
      // representation.
      std::string index = std::to_string(i);

      // Set the point light uniform for the current index.
      glUniform3f(glGetUniformLocation(shader.program, ("pointLights[" + index + "].position").c_str()), pointLightPositions[i].x, pointLightPositions[i].y, pointLightPositions[i].z);
      glUniform3f(glGetUniformLocation(shader.program, ("pointLights[" + index + "].ambient").c_str()), 0.05f, 0.05f, 0.05f);
      glUniform3f(glGetUniformLocation(shader.program, ("pointLights[" + index + "].diffuse").c_str()), 1.0f, 1.0f, 1.0f);
      glUniform3f(glGetUniformLocation(shader.program, ("pointLights[" + index + "].specular").c_str()), 1.0f, 1.0f, 1.0f);
      glUniform1f(glGetUniformLocation(shader.program, ("pointLights[" + index + "].constant").c_str()), 1.0f);
      glUniform1f(glGetUniformLocation(shader.program, ("pointLights[" + index + "].linear").c_str()), 0.09);
      glUniform1f(glGetUniformLocation(shader.program, ("pointLights[" + index + "].quadratic").c_str()), 0.032);
    }

    // Pass material values.
    GLuint materialShininess = glGetUniformLocation(shader.program, "material.shininess");
    GLuint materialDiffuse   = glGetUniformLocation(shader.program, "material.diffuse");
    GLuint materialSpecular  = glGetUniformLocation(shader.program, "material.specular");
    GLuint materialEmission  = glGetUniformLocation(shader.program, "material.emission");
    glUniform1f(materialShininess, 32.0f);
    glUniform1i(materialDiffuse, 0);
    glUniform1i(materialSpecular, 1);
    glUniform1i(materialEmission, 2);

    // Bind the textures.
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, containerTexture);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, containerSpecular);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, containerEmission);
  }

  // Misc values.
  GLuint viewPos = glGetUniformLocation(shader.program, "viewPos");
  glUniform3f(viewPos, camera.position.x, camera.position.y, camera.position.z);

  // Draw multiple containers!
  GLuint modelMatrix = glGetUniformLocation(shader.program, "model");
  GLuint normalMatrix = glGetUniformLocation(shader.program, "normalMatrix");
  for (GLuint i = 0; i < 10; i++) {
    // Apply world transformations.
    model = glm::mat4();
    model = glm::translate(model, cubePositions[i]);
    model = glm::rotate(model, i * 20.0f, glm::vec3(1.0f, 0.3f, 0.5f));
    glUniformMatrix4fv(modelMatrix, 1, GL_FALSE, glm::value_ptr(model));
    // Calculate the normal matrix on the CPU (keep them normals perpendicular).
    normal = glm::mat3(glm::transpose(glm::inverse(model)));
    glUniformMatrix3fv(normalMatrix, 1, GL_FALSE, glm::value_ptr(normal));
    // Draw the container.
    glDrawArrays(GL_TRIANGLES, 0, 36);
  }

  // Draw a scaled container under the camera to act as a floor.
  model = glm::mat4();
  model = glm::translate(model, glm::vec3(0.0f, -1.0f, 0.0f));
  model = glm::scale(model, glm::vec3(15.0f, 0.001f, 15.0f));
  glUniformMatrix4fv(modelMatrix, 1, GL_FALSE, glm::value_ptr(model));
  normal = glm::mat3(glm::transpose(glm::inverse(model)));
  glUniformMatrix3fv(normalMatrix, 1, GL_FALSE, glm::value_ptr(normal));
  glDrawArrays(GL_TRIANGLES, 0, 36);

  // We're done drawing containers.
  glBindVertexArray(0);
}

GLuint loadTexture(std::string filepath) {
  // Generate the texture on the OpenGL side and bind it.
  GLuint texture;
  glGenTextures(1, &texture);
  glBindTexture(GL_TEXTURE_2D, texture);

  // Set some parameters for the bound texture. Use GL_LINEAR to get a gaussian
  // blur like effect on upscaled textures.
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

  // Load in the container texture using the soil library.
  int width, height;
  unsigned char* image = SOIL_load_image(filepath.c_str(), &width, &height, 0, SOIL_LOAD_RGB);

  // Load the image data to the currently bound texture. Also create some
  // mipmaps for it for perf.
  glTexImage2D(GL_TEXTURE_2D, 0, GL_SRGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, image);
  glGenerateMipmap(GL_TEXTURE_2D);

  // Free the image and unbind the texture.
  SOIL_free_image_data(image);
  glBindTexture(GL_TEXTURE_2D, 0);

  return texture;
}

void move(GLfloat delta) {
  GLfloat cameraSpeed = 5.0f * delta;

  // Movement keys.
  if (keys[GLFW_KEY_W]) camera.position += cameraSpeed * camera.front;
  if (keys[GLFW_KEY_S]) camera.position -= cameraSpeed * camera.front;
  if (keys[GLFW_KEY_A]) camera.position -= glm::normalize(glm::cross(camera.front, camera.up)) * cameraSpeed;
  if (keys[GLFW_KEY_D]) camera.position += glm::normalize(glm::cross(camera.front, camera.up)) * cameraSpeed;
}

GLfloat easeOutQuart(GLfloat t, GLfloat b, GLfloat c, GLfloat d) {
  t /= d;
  t--;
  return -c * (t*t*t*t - 1) + b;
}

void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mode) {
  UNUSED(scancode);
  UNUSED(mode);

  // Keep key state using a key buffer of size 1024.
  if (action == GLFW_PRESS) {
    keys[key] = true;
  } else if (action == GLFW_RELEASE) {
    keys[key] = false;
  }

  // Close the application when escape is pressed.
  if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
    glfwSetWindowShouldClose(window, GL_TRUE);
  }
}

void mouseCallback(GLFWwindow* window, double xpos, double ypos) {
  UNUSED(window);

  if (firstMouseEvent) {
    lastX = xpos;
    lastY = ypos;
    firstMouseEvent = false;
  }

  // Find the position difference of the mouse.
  GLfloat xoffset = xpos - lastX;
  GLfloat yoffset = lastY - ypos; // Reversed since positive y is up.

  // Set the history values to the current values.
  lastX = xpos;
  lastY = ypos;

  // Multiply the offset by the mouse sensitivity to prevent rapidly unplanned
  // seizures and the like.
  GLfloat mouseSensitivity = 0.12f / (45.0f / camera.fov);
  xoffset *= mouseSensitivity;
  yoffset *= mouseSensitivity;

  camera.rotation.y += xoffset;
  camera.rotation.x += yoffset;

  // Constrain the pitch so the FPS camera does not do sick backflips.
  if (camera.rotation.x > glm::radians(89.0f)) camera.rotation.x = glm::radians(89.0f);
  if (camera.rotation.x < glm::radians(-89.0f)) camera.rotation.x = glm::radians(-89.0f);

  camera.update();
}

void scrollCallback(GLFWwindow* window, double xoffset, double yoffset) {
  UNUSED(window);
  UNUSED(xoffset);

  fovTime = 0.0f;
  startFov = camera.fov;
  targetFov -= glm::radians(yoffset * 3);

  // Constrain the fov (zoom).
  if (targetFov < glm::radians(1.0f)) targetFov = glm::radians(1.0f);
  if (targetFov > glm::radians(45.0f)) targetFov = glm::radians(45.0f);
}
