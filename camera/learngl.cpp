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

// Coordinate system matrices.
glm::mat4 model;

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

  // Read and compile the vertex and fragment shaders using
  // the shader helper class.
  Shader shader("glsl/vertex.glsl", "glsl/fragment.glsl");

  // Load the textures for the magic square.
  GLuint texture1 = loadTexture("assets/container.jpg");
  GLuint texture2 = loadTexture("assets/awesomeface.png");

  // Cube vertices with no indices.
  GLfloat vertices[] = {
    -0.5f, -0.5f, -0.5f,  0.0f, 0.0f,
     0.5f, -0.5f, -0.5f,  1.0f, 0.0f,
     0.5f,  0.5f, -0.5f,  1.0f, 1.0f,
     0.5f,  0.5f, -0.5f,  1.0f, 1.0f,
    -0.5f,  0.5f, -0.5f,  0.0f, 1.0f,
    -0.5f, -0.5f, -0.5f,  0.0f, 0.0f,

    -0.5f, -0.5f,  0.5f,  0.0f, 0.0f,
     0.5f, -0.5f,  0.5f,  1.0f, 0.0f,
     0.5f,  0.5f,  0.5f,  1.0f, 1.0f,
     0.5f,  0.5f,  0.5f,  1.0f, 1.0f,
    -0.5f,  0.5f,  0.5f,  0.0f, 1.0f,
    -0.5f, -0.5f,  0.5f,  0.0f, 0.0f,

    -0.5f,  0.5f,  0.5f,  1.0f, 0.0f,
    -0.5f,  0.5f, -0.5f,  1.0f, 1.0f,
    -0.5f, -0.5f, -0.5f,  0.0f, 1.0f,
    -0.5f, -0.5f, -0.5f,  0.0f, 1.0f,
    -0.5f, -0.5f,  0.5f,  0.0f, 0.0f,
    -0.5f,  0.5f,  0.5f,  1.0f, 0.0f,

     0.5f,  0.5f,  0.5f,  1.0f, 0.0f,
     0.5f,  0.5f, -0.5f,  1.0f, 1.0f,
     0.5f, -0.5f, -0.5f,  0.0f, 1.0f,
     0.5f, -0.5f, -0.5f,  0.0f, 1.0f,
     0.5f, -0.5f,  0.5f,  0.0f, 0.0f,
     0.5f,  0.5f,  0.5f,  1.0f, 0.0f,

    -0.5f, -0.5f, -0.5f,  0.0f, 1.0f,
     0.5f, -0.5f, -0.5f,  1.0f, 1.0f,
     0.5f, -0.5f,  0.5f,  1.0f, 0.0f,
     0.5f, -0.5f,  0.5f,  1.0f, 0.0f,
    -0.5f, -0.5f,  0.5f,  0.0f, 0.0f,
    -0.5f, -0.5f, -0.5f,  0.0f, 1.0f,

    -0.5f,  0.5f, -0.5f,  0.0f, 1.0f,
     0.5f,  0.5f, -0.5f,  1.0f, 1.0f,
     0.5f,  0.5f,  0.5f,  1.0f, 0.0f,
     0.5f,  0.5f,  0.5f,  1.0f, 0.0f,
    -0.5f,  0.5f,  0.5f,  0.0f, 0.0f,
    -0.5f,  0.5f, -0.5f,  0.0f, 1.0f
  };

  // Create a VBO to store the vertex data, an EBO to store indice data, and
  // create a VAO to retain our vertex attribute pointers.
  GLuint VBO, VAO;
  glGenVertexArrays(1, &VAO);
  glGenBuffers(1, &VBO);

  // Bind the VAO for the square first, set the vertex buffers,
  // then the attribute pointers.
  glBindVertexArray(VAO);

  // Fill up the VBO and EBO for the square while the VAO for the square is
  // currently bound (see above).
  glBindBuffer(GL_ARRAY_BUFFER, VBO);
  glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

  // Set the vertex attributes.
  // p = position, c = color, t = texture coordinate
  // format: pppccctt
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), (GLvoid*)0);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), (GLvoid*)(3 * sizeof(GLfloat)));
  glEnableVertexAttribArray(1);

  // Unbind the VBO and VAO to get a clean state. DO NOT unbind the EBO or the
  // VAO will no longer be able to access it (I have no idea why).
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glBindVertexArray(0);

  // Many cubes is better than one cube.
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

    // Clear the screen to a nice blue color.
    glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Activate the shader program for this square.
    shader.use();

    // Bind textures for the square to mix.
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture1);
    glUniform1i(glGetUniformLocation(shader.program, "texture1"), 0);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, texture2);
    glUniform1i(glGetUniformLocation(shader.program, "texture2"), 1);

    const GLfloat limitTime = 1.0f;
    fovTime += delta;
    if (fovTime > limitTime) {
      fovTime = limitTime;
    }

    // Update the perspective to account for changes in fov.
    camera.fov = easeOutQuart(fovTime, startFov, (startFov - targetFov) * -1, limitTime);
    camera.update();

    // Pass the model, view, and projection matrices to get the vertices into
    // clip space (OpenGL will convert from clip space to coordinate space).
    GLuint viewMatrix = glGetUniformLocation(shader.program, "view");
    glUniformMatrix4fv(viewMatrix, 1, GL_FALSE, glm::value_ptr(camera.view));
    GLuint projectionMatrix = glGetUniformLocation(shader.program, "projection");
    glUniformMatrix4fv(projectionMatrix, 1, GL_FALSE, glm::value_ptr(camera.projection));

    glBindVertexArray(VAO);
    for (int i = 0; i < 10; i++) {
      GLfloat rotation = 0.0f;
      if (i % 3 == 0) {
        rotation = (GLfloat)glfwGetTime() + i;
      }

      // Apply transformations for the cube.
      model = glm::mat4();
      model = glm::translate(model, cubePositions[i]);
      model = glm::rotate(model, rotation, glm::vec3(0.5f, 1.0f, 0.0f));
      GLuint modelMatrix = glGetUniformLocation(shader.program, "model");
      glUniformMatrix4fv(modelMatrix, 1, GL_FALSE, glm::value_ptr(model));

      // Draw the square!
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
  fovTime = 0.0f;
  startFov = camera.fov;
  targetFov -= glm::radians(yoffset * 3);

  // Constrain the fov (zoom).
  if (targetFov < glm::radians(1.0f)) targetFov = glm::radians(1.0f);
  if (targetFov > glm::radians(45.0f)) targetFov = glm::radians(45.0f);
}
