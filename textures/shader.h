#ifndef SHADER_H
#define SHADER_H

#include <string>

#include <GL/glew.h>

using namespace std;

class Shader {
  public:
    // Shader program pointer in the OpenGL state machine.
    GLuint program;

    // Shader constructor for creating a shader program with both a vertex
    // shader and a fragment shader.
    Shader(const GLchar* vertexPath, const GLchar* fragmentPath);

    // Activate the shader in the OpenGL state machine. This is simply just a
    // wrapper around glUseProgram using the public program field as input.
    void use();
  private:
    // Reads a shader (or any file for that matter) and puts it into a string.
    string readShader(string filepath);

    // Used to check if a shader compiled successfully.
    bool checkCompileStatus(GLuint shader);

    // Used to check if a shader linked successfully.
    bool checkLinkStatus();
};

#endif
