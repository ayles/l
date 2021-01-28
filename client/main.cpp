#include <client/shader_program.h>
#include <client/dual_contour.h>
#include <client/marching_cubes.h>
#include <client/simplex.h>
#include <client/iso_surface_generator.h>

#include <gl/glew.h>
#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/hash.hpp>
#include <GLFW/glfw3.h>

#include <iostream>
#include <chrono>
#include <memory>
#include <tuple>


void window_size_callback(GLFWwindow* window, int x, int y) {
    glViewport(0, 0, x, y);
}


int main() {
    if(!glfwInit()) {
        std::cout << "Failed to initialize GLFW" << std::endl;
        abort();
    }

    glfwWindowHint(GLFW_SAMPLES, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(1024, 768, "L", nullptr, nullptr);
    if (!window){
        std::cout << "Failed to open GLFW window" << std::endl;
        abort();
    }
    glfwMakeContextCurrent(window);

    glewExperimental = true;
    if (glewInit() != GLEW_OK) {
        std::cout << "Failed to initialize GLEW" << std::endl;
        abort();
    }

    glfwSetInputMode(window, GLFW_STICKY_KEYS, GL_TRUE);

    glfwSetWindowSizeCallback(window, window_size_callback);

    glClearColor(0.874509804f, 0.874509804f, 0.874509804f, 0.0f);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    GLuint vaoId;
    glGenVertexArrays(1, &vaoId);
    glBindVertexArray(vaoId);

    ShaderProgram shaderProgram = ShaderProgram::Compile(
R"(#version 330 core

layout(location = 0) in vec3 vertex;
layout(location = 1) in vec3 normal;

out vec3 norm;

uniform mat4 mvp;
uniform mat4 nmat;

void main() {
    gl_Position = mvp * vec4(vertex, 1.0);
    norm = (nmat * vec4(normal, 0.0)).xyz;
})",

R"(#version 330 core

out vec4 outColor;
in vec3 norm;

const vec3 light = -normalize(vec3(2, -4, -1));

void main() {
    float d = dot(vec3(0, 1, 0), norm);
    d = clamp(d, 0, 1);
    vec3 col = (vec3(136,113,171) * d + vec3(158,163,245) * (1 - d)) / 255.0;
	outColor = vec4(col * clamp(dot(norm, light), 0, 1), 1);
})");

    ShaderProgram axisProgram = ShaderProgram::Compile(
        R"(#version 330 core

layout(location = 0) in vec3 vertex;
out vec3 p;

uniform mat4 vp;

void main() {
    gl_Position = vp * vec4(vertex, 1.0);
    p = vertex;
})",

        R"(#version 330 core

out vec4 outColor;
in vec3 p;

void main() {
	outColor = vec4(p.xyz, 1);
})");

    GLuint vboIds[4];
    glGenBuffers(4, vboIds);
    shaderProgram.Bind();

    glm::mat4 projection = glm::perspective(glm::radians(70.0f), 1024.0f / 768.0f, 0.01f, 100.0f);
    glm::mat4 view;
    glm::mat4 model(1.0f);

    std::chrono::high_resolution_clock::time_point start = std::chrono::high_resolution_clock::now();
    float elapsed = 0.0f;
    bool p = false;
    bool pp = false;
    bool ppp = false;
    bool pppp = false;
    bool r = false;

    auto normalFromFunction = [](std::function<float(float, float, float)> f, float d = 0.01) {
        return [f, d](float x, float y, float z) {
            return -glm::normalize(glm::vec3(
                (f(x + d, y, z) - f(x - d, y, z)),
                (f(x, y + d, z) - f(x, y - d, z)),
                (f(x, y, z + d) - f(x, y, z - d))) / 2.0f / d);
        };
    };


    auto f = [&elapsed](float x, float y, float z) {
        float factor = std::max(0.0f, std::max(std::abs(x) - 10, std::max(std::abs(y) - 10, std::abs(z) - 10))) / 7.0f;
        factor *= factor;
        return
            Simplex::noise(glm::vec4(x / 20, 0, z / 20, elapsed / 3.0f)) * 0.7f +
            Simplex::noise(glm::vec4(x / 10, 0, z / 10, elapsed / 3.0f)) * 0.3f +
            //Simplex::noise(glm::vec4(x / 4, 0, z / 4, elapsed / 3.0f)) * 0.0f +
            - y / 10.0f;
        //return Simplex::noise(glm::vec4(x / 10, 0, z / 10, elapsed / 3.0f)) - y / 10.0f;// - factor;
    };

    auto nrm = normalFromFunction(f, 0.01);
    auto ff = [&f, &nrm](const glm::vec3& p) {
        return DualContour::Point {
            (float)f(p.x, p.y, p.z),
            nrm(p.x, p.y, p.z)
        };
    };

    std::array<std::shared_ptr<IIsoSurfaceGenerator>, 2> gens = { std::shared_ptr<IIsoSurfaceGenerator>(new MarchingCubes), std::shared_ptr<IIsoSurfaceGenerator>(new DualContour) };
    gens[0]->SetVolumeProvider(ff);
    gens[1]->SetVolumeProvider(ff);

    const float axis[18] {
        0.0f, 0.0f, 0.0f, 100.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 0.0f, 100.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 100.0f,
    };

    glBindBuffer(GL_ARRAY_BUFFER, vboIds[3]);
    glBufferData(GL_ARRAY_BUFFER, sizeof(axis), axis, GL_STATIC_DRAW);

    IIsoSurfaceGenerator::Mesh* mesh = &gens[0]->Generate(glm::ivec3(-3), glm::ivec3(3));

    do {
        float delta = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now() - start).count() / 1'000'000'000.0f;
        if (r) {
            elapsed += delta;
        }
        start = std::chrono::high_resolution_clock::now();

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glEnableVertexAttribArray(0);
        glEnableVertexAttribArray(1);

        if (r) {
            mesh = &gens[0]->Generate(glm::ivec3(-3), glm::ivec3(3));
        }

        glBindBuffer(GL_ARRAY_BUFFER, vboIds[0]);
        glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 3 * mesh->Vertices.size(), glm::value_ptr(mesh->Vertices[0]), GL_DYNAMIC_DRAW);
        glBindBuffer(GL_ARRAY_BUFFER, vboIds[1]);
        glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 3 * mesh->Normals.size(), glm::value_ptr(mesh->Normals[0]), GL_DYNAMIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vboIds[2]);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(unsigned int) * mesh->Indices.size(), mesh->Indices.data(), GL_DYNAMIC_DRAW);

        glBindBuffer(GL_ARRAY_BUFFER, vboIds[0]);
        glVertexAttribPointer(
            0,                  // attribute 0. No particular reason for 0, but must match the layout in the shader.
            3,                  // size
            GL_FLOAT,           // type
            GL_FALSE,           // normalized?
            0,                  // stride
            nullptr             // array buffer offset
        );

        glBindBuffer(GL_ARRAY_BUFFER, vboIds[1]);
        glVertexAttribPointer(
            1,                  // attribute 0. No particular reason for 0, but must match the layout in the shader.
            3,                  // size
            GL_FLOAT,           // type
            GL_FALSE,           // normalized?
            0,                  // stride
            nullptr             // array buffer offset
        );

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vboIds[2]);

        view = glm::lookAt(glm::vec3(std::sin(elapsed / 10) * 25, 15, std::cos(elapsed / 10) * 25), glm::vec3(0, 0, 0), glm::vec3(0, 1, 0));
        //model *= glm::rotate(delta, glm::vec3(1, 0, 0));

        shaderProgram.Bind();
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram.ProgramId_, "mvp"), 1, GL_FALSE, glm::value_ptr(projection * view * model));
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram.ProgramId_, "nmat"), 1, GL_FALSE, glm::value_ptr(glm::transpose(glm::inverse(model))));

        if (mesh->Indices.empty()) {
            glDrawArrays(GL_TRIANGLES, 0, mesh->Count);
        } else {
            glDrawElements(GL_TRIANGLES, mesh->Count, GL_UNSIGNED_INT, nullptr);
        }


        glBindBuffer(GL_ARRAY_BUFFER, vboIds[3]);
        glVertexAttribPointer(
            0,
            3,
            GL_FLOAT,
            GL_FALSE,
            0,
            nullptr
        );
        axisProgram.Bind();
        glUniformMatrix4fv(glGetUniformLocation(axisProgram.ProgramId_, "vp"), 1, GL_FALSE, glm::value_ptr(projection * view));
        glDrawArrays(GL_LINES, 0, 18);


        glDisableVertexAttribArray(1);
        glDisableVertexAttribArray(0);

        glfwSwapBuffers(window);
        glfwPollEvents();

        if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) {
            if (!p) {
                GLint mode;
                glGetIntegerv(GL_POLYGON_MODE, &mode);
                glPolygonMode(GL_FRONT_AND_BACK, mode == GL_LINE ? GL_FILL : GL_LINE);
            }
        }
        p = glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS;

        if (glfwGetKey(window, GLFW_KEY_H) == GLFW_PRESS) {
            if (!pp) {
                gens[0]->SetHardNormals(!gens[0]->GetHardNormals());
            }
        }
        pp = glfwGetKey(window, GLFW_KEY_H) == GLFW_PRESS;


        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
            if (!ppp) {
                std::swap(gens[0], gens[1]);
                gens[0]->SetHardNormals(gens[1]->GetHardNormals());
            }
        }
        ppp = glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS;

        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
            if (!pppp) {
                r = !r;
            }
        }
        pppp = glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS;
        //std::cout << 1.0f / delta << std::endl;
    } while (glfwGetKey(window, GLFW_KEY_ESCAPE) != GLFW_PRESS && glfwWindowShouldClose(window) == 0);

    glDeleteBuffers(4, vboIds);
    glDeleteVertexArrays(1, &vaoId);

    glfwTerminate();

    return 0;
}
