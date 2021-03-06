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
#include "model.h"

#define UNUSED(expr) (void)(expr)

// Window constants for the initial window size.
const GLuint kWindowWidth = 800;
const GLuint kWindowHeight = 600;

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

  // Enable use of the depth buffer since we're working on 3D and want to
  // prevent overlapping polygon artifacts.
  glEnable(GL_DEPTH_TEST);
  glEnable(GL_STENCIL_TEST);
  glEnable(GL_CULL_FACE);

  // Read and compile the vertex and fragment shaders using
  // the shader helper class.
  Shader shader("glsl/vertex.glsl", "glsl/fragment.glsl");
  Shader outlineShader("glsl/outline_vertex.glsl", "glsl/outline_fragment.glsl");
  Shader lampShader("glsl/lampvertex.glsl", "glsl/lampfragment.glsl");

  // Read 3D models.
  Model crysisModel("assets/nanosuit.obj");

  GLuint containerTexture = loadTexture("assets/container2.png");
  GLuint containerSpecular = loadTexture("assets/container2_specular.png");
  GLuint containerEmission = loadTexture("assets/matrix.jpg");

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
  // Positions all containers
  glm::vec3 cubePositions[] = {
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

  // Light information.
  const glm::vec3 directionalLightDir(0.0f, 1.0f, 0.0f);
  const glm::vec3 pointLightPositions[] = {
    glm::vec3( 0.7f,  0.2f,  2.0f),
    glm::vec3( 2.3f, -3.3f, -4.0f),
    glm::vec3(-4.0f,  2.0f, -12.0f),
    glm::vec3( 0.0f,  0.0f, -3.0f)
  };

  // Render loop.
  while (!glfwWindowShouldClose(window)) {
    GLfloat currentFrame = glfwGetTime();
    delta = currentFrame - lastFrame;
    lastFrame = currentFrame;

    // Check and call events.
    glfwPollEvents();
    move(delta);

    glEnable(GL_DEPTH_TEST);
    glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
    glStencilMask(0xFF); // Enable writing to the stencil buffer.

    // Clear the screen to a nice blue color.
    glClearColor(0.1f, 0.15f, 0.15f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    const GLfloat limitTime = 1.0f;
    fovTime += delta;
    if (fovTime > limitTime) {
      fovTime = limitTime;
    }

    // Update the perspective to account for changes in fov.
    camera.fov = easeOutQuart(fovTime, startFov, (startFov - targetFov) * -1, limitTime);
    camera.update();

    shader.use();

    // Pass the view and projection matrices from the camera.
    GLuint viewMatrix = glGetUniformLocation(shader.program, "view");
    glUniformMatrix4fv(viewMatrix, 1, GL_FALSE, glm::value_ptr(camera.view));
    GLuint projectionMatrix = glGetUniformLocation(shader.program, "projection");
    glUniformMatrix4fv(projectionMatrix, 1, GL_FALSE, glm::value_ptr(camera.projection));

    // Generate light colors.
    glm::vec3 lightColor(1.0f, 1.0f, 1.0f);

    // Directional light
    glUniform3f(glGetUniformLocation(shader.program, "dirLight.direction"), -0.2f, -1.0f, -0.3f);
    glUniform3f(glGetUniformLocation(shader.program, "dirLight.ambient"), 0.05f, 0.05f, 0.05f);
    glUniform3f(glGetUniformLocation(shader.program, "dirLight.diffuse"), 1.0f, 1.0f, 1.0f);
    glUniform3f(glGetUniformLocation(shader.program, "dirLight.specular"), 1.0f, 1.0f, 1.0f);

    // Point light 1
    glUniform3f(glGetUniformLocation(shader.program, "pointLights[0].position"), pointLightPositions[0].x, pointLightPositions[0].y, pointLightPositions[0].z);
    glUniform3f(glGetUniformLocation(shader.program, "pointLights[0].ambient"), 0.05f, 0.05f, 0.05f);
    glUniform3f(glGetUniformLocation(shader.program, "pointLights[0].diffuse"), 1.0f, 1.0f, 1.0f);
    glUniform3f(glGetUniformLocation(shader.program, "pointLights[0].specular"), 1.0f, 1.0f, 1.0f);
    glUniform1f(glGetUniformLocation(shader.program, "pointLights[0].constant"), 1.0f);
    glUniform1f(glGetUniformLocation(shader.program, "pointLights[0].linear"), 0.09);
    glUniform1f(glGetUniformLocation(shader.program, "pointLights[0].quadratic"), 0.032);

    // Point light 2
    glUniform3f(glGetUniformLocation(shader.program, "pointLights[1].position"), pointLightPositions[1].x, pointLightPositions[1].y, pointLightPositions[1].z);
    glUniform3f(glGetUniformLocation(shader.program, "pointLights[1].ambient"), 0.05f, 0.05f, 0.05f);
    glUniform3f(glGetUniformLocation(shader.program, "pointLights[1].diffuse"), 1.0f, 1.0f, 1.0f);
    glUniform3f(glGetUniformLocation(shader.program, "pointLights[1].specular"), 1.0f, 1.0f, 1.0f);
    glUniform1f(glGetUniformLocation(shader.program, "pointLights[1].constant"), 1.0f);
    glUniform1f(glGetUniformLocation(shader.program, "pointLights[1].linear"), 0.09);
    glUniform1f(glGetUniformLocation(shader.program, "pointLights[1].quadratic"), 0.032);

    // Point light 3
    glUniform3f(glGetUniformLocation(shader.program, "pointLights[2].position"), pointLightPositions[2].x, pointLightPositions[2].y, pointLightPositions[2].z);
    glUniform3f(glGetUniformLocation(shader.program, "pointLights[2].ambient"), 0.05f, 0.05f, 0.05f);
    glUniform3f(glGetUniformLocation(shader.program, "pointLights[2].diffuse"), 1.0f, 1.0f, 1.0f);
    glUniform3f(glGetUniformLocation(shader.program, "pointLights[2].specular"), 1.0f, 1.0f, 1.0f);
    glUniform1f(glGetUniformLocation(shader.program, "pointLights[2].constant"), 1.0f);
    glUniform1f(glGetUniformLocation(shader.program, "pointLights[2].linear"), 0.09);
    glUniform1f(glGetUniformLocation(shader.program, "pointLights[2].quadratic"), 0.032);

    // Point light 4
    glUniform3f(glGetUniformLocation(shader.program, "pointLights[3].position"), pointLightPositions[3].x, pointLightPositions[3].y, pointLightPositions[3].z);
    glUniform3f(glGetUniformLocation(shader.program, "pointLights[3].ambient"), 0.05f, 0.05f, 0.05f);
    glUniform3f(glGetUniformLocation(shader.program, "pointLights[3].diffuse"), 1.0f, 1.0f, 1.0f);
    glUniform3f(glGetUniformLocation(shader.program, "pointLights[3].specular"), 1.0f, 1.0f, 1.0f);
    glUniform1f(glGetUniformLocation(shader.program, "pointLights[3].constant"), 1.0f);
    glUniform1f(glGetUniformLocation(shader.program, "pointLights[3].linear"), 0.09);
    glUniform1f(glGetUniformLocation(shader.program, "pointLights[3].quadratic"), 0.032);

    // Sport light 1
    glUniform3f(glGetUniformLocation(shader.program, "spotLights[0].position"), camera.position.x, camera.position.y, camera.position.z);
    glUniform3f(glGetUniformLocation(shader.program, "spotLights[0].direction"), camera.front.x, camera.front.y, camera.front.z);
    glUniform3f(glGetUniformLocation(shader.program, "spotLights[0].ambient"), 0.0f, 0.0f, 0.0f);
    glUniform3f(glGetUniformLocation(shader.program, "spotLights[0].diffuse"), 1.0f, 1.0f, 1.0f);
    glUniform3f(glGetUniformLocation(shader.program, "spotLights[0].specular"), 1.0f, 1.0f, 1.0f);
    glUniform1f(glGetUniformLocation(shader.program, "spotLights[0].constant"), 1.0f);
    glUniform1f(glGetUniformLocation(shader.program, "spotLights[0].linear"), 0.09);
    glUniform1f(glGetUniformLocation(shader.program, "spotLights[0].quadratic"), 0.032);
    glUniform1f(glGetUniformLocation(shader.program, "spotLights[0].cutoff"), glm::cos(glm::radians(12.5f)));
    glUniform1f(glGetUniformLocation(shader.program, "spotLights[0].outerCutoff"), glm::cos(glm::radians(15.5f)));

    // Pass material values.
    GLuint materialShininess = glGetUniformLocation(shader.program, "material.shininess");
    GLuint materialDiffuse   = glGetUniformLocation(shader.program, "material.diffuse");
    GLuint materialSpecular  = glGetUniformLocation(shader.program, "material.specular");
    GLuint materialEmission  = glGetUniformLocation(shader.program, "material.emission");
    glUniform1f(materialShininess, 64.0f);
    glUniform1i(materialDiffuse, 0);
    glUniform1i(materialSpecular, 1);
    glUniform1i(materialEmission, 2);

    // Misc values.
    GLuint viewPos = glGetUniformLocation(shader.program, "viewPos");
    glUniform3f(viewPos, camera.position.x, camera.position.y, camera.position.z);

    // Bind the textures.
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, containerTexture);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, containerSpecular);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, containerEmission);

    // Apply world transformations.
    model = glm::mat4();
    GLuint modelMatrix = glGetUniformLocation(shader.program, "model");
    glUniformMatrix4fv(modelMatrix, 1, GL_FALSE, glm::value_ptr(model));

    // Calculate the normal matrix on the CPU (keep them normals perpendicular).
    normal = glm::mat3(glm::transpose(glm::inverse(model)));
    GLuint normalMatrix = glGetUniformLocation(shader.program, "normalMatrix");
    glUniformMatrix3fv(normalMatrix, 1, GL_FALSE, glm::value_ptr(normal));

    // Draw the magic man!
    glStencilFunc(GL_ALWAYS, 1, 0xFF); // Use mask 0xFF to allow all writes.
    glStencilMask(0xFF); // Enable writing to the stencil buffer.
    crysisModel.draw(shader);

    outlineShader.use();
    // Pass the view and projection matrices from the camera.
    viewMatrix = glGetUniformLocation(outlineShader.program, "view");
    glUniformMatrix4fv(viewMatrix, 1, GL_FALSE, glm::value_ptr(camera.view));
    // Change the fov for outline.
    projectionMatrix = glGetUniformLocation(outlineShader.program, "projection");
    glUniformMatrix4fv(projectionMatrix, 1, GL_FALSE, glm::value_ptr(camera.projection));
    // Transform the outline to be scaled larger than the magic man.
    model = glm::mat4();
    model = glm::translate(model, glm::vec3(0.0f, 7.25f, 0.0f));
    model = glm::scale(model, glm::vec3(1.1f));
    model = glm::translate(model, glm::vec3(0.0f, -7.25f, 0.0f));
    modelMatrix = glGetUniformLocation(outlineShader.program, "model");
    glUniformMatrix4fv(modelMatrix, 1, GL_FALSE, glm::value_ptr(model));

    // Bind the VAO and shader.
    glBindVertexArray(lightVAO);
    lampShader.use();

    // Pass the view and projection matrices from the camera.
    viewMatrix = glGetUniformLocation(lampShader.program, "view");
    glUniformMatrix4fv(viewMatrix, 1, GL_FALSE, glm::value_ptr(camera.view));
    projectionMatrix = glGetUniformLocation(lampShader.program, "projection");
    glUniformMatrix4fv(projectionMatrix, 1, GL_FALSE, glm::value_ptr(camera.projection));

    for (GLuint i = 0; i < 4; i++) {
      // Apply world transformations.
      model = glm::mat4();
      model = glm::translate(model, pointLightPositions[i]);
      model = glm::scale(model, glm::vec3(0.2f));

      modelMatrix = glGetUniformLocation(lampShader.program, "model");
      glUniformMatrix4fv(modelMatrix, 1, GL_FALSE, glm::value_ptr(model));

      // Draw the lamp.
      glDrawArrays(GL_TRIANGLES, 0, 36);
    }
    glBindVertexArray(0);

    // Swap buffers used for double buffering.
    glfwSwapBuffers(window);
  }

  // Properly deallocate the VBO and VAO.
  glDeleteVertexArrays(1, &VAO);
  glDeleteBuffers(1, &VBO);

  // Terminate GLFW and clean any resources before exiting.
  glfwTerminate();

  return 0;
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
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, image);
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
