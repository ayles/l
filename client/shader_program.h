#pragma once

#include <gl/glew.h>

#include <string_view>
#include <iostream>
#include <vector>

class ShaderProgram {
public:
    void Bind() const {
        glUseProgram(ProgramId_);
    }

    ~ShaderProgram() {
        glDeleteProgram(ProgramId_);
    }

private:
    explicit ShaderProgram(GLuint programId)
        : ProgramId_(programId)
    {

    }

public:
    static ShaderProgram Compile(const std::string_view& vertexShader, const std::string_view& fragmentShader, const std::string_view& geometryShader = "") {
        auto compile = [](const std::string_view& source, size_t type) {
            std::cout << "Compiling " << (type == GL_VERTEX_SHADER ? "vertex" : (type == GL_GEOMETRY_SHADER ? "geometry" : "fragment")) << " shader" << std::endl;

            GLuint shaderId = glCreateShader(type);
            const char* c = source.data();
            glShaderSource(shaderId, 1, &c , nullptr);
            glCompileShader(shaderId);

            GLint result = GL_FALSE;
            GLint infoLogLength;
            glGetShaderiv(shaderId, GL_COMPILE_STATUS, &result);
            glGetShaderiv(shaderId, GL_INFO_LOG_LENGTH, &infoLogLength);
            if (infoLogLength > 0){
                std::vector<char> errorMessage(infoLogLength + 1);
                glGetShaderInfoLog(shaderId, infoLogLength, nullptr, &errorMessage[0]);
                std::cout << &errorMessage[0] << std::endl;
            }

            return shaderId;
        };

        std::vector<GLuint> shaderIds { compile(vertexShader, GL_VERTEX_SHADER), compile(fragmentShader, GL_FRAGMENT_SHADER) };
        if (!geometryShader.empty()) {
            shaderIds.push_back(compile(geometryShader, GL_GEOMETRY_SHADER));
        }

        std::cout << "Linking program" << std::endl;
        GLuint programId = glCreateProgram();
        for (GLuint shaderId : shaderIds) {
            glAttachShader(programId, shaderId);
        }
        glLinkProgram(programId);

        GLint result = GL_FALSE;
        GLint infoLogLength;
        glGetProgramiv(programId, GL_LINK_STATUS, &result);
        glGetProgramiv(programId, GL_INFO_LOG_LENGTH, &infoLogLength);
        if (infoLogLength > 0){
            std::vector<char> errorMessage(infoLogLength + 1);
            glGetProgramInfoLog(programId, infoLogLength, nullptr, &errorMessage[0]);
            std::cout << &errorMessage[0] << std::endl;
        }

        for (GLuint shaderId : shaderIds) {
            glDetachShader(programId, shaderId);
            glDeleteShader(shaderId);
        }

        return ShaderProgram(programId);
    }

public:
    GLuint ProgramId_;
};
