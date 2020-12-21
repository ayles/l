#include <gl/glew.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <vector>
#include <core/object.h>
#include <unordered_map>
#include <thread>
#include <mutex>
#include <enet/enet.h>
#include <object.pb.h>


GLuint compile_shader(const std::string& vertex_shader, const std::string& fragment_shader){
    auto compile = [](const std::string& source, size_t type) {
        std::cout << "Compiling " << (type == GL_VERTEX_SHADER ? "vertex" : "fragment") << " shader" << std::endl;

        GLuint shader_id = glCreateShader(type);
        const char* c = source.c_str();
        glShaderSource(shader_id, 1, &c , nullptr);
        glCompileShader(shader_id);

        GLint result = GL_FALSE;
        GLint info_log_length;
        glGetShaderiv(shader_id, GL_COMPILE_STATUS, &result);
        glGetShaderiv(shader_id, GL_INFO_LOG_LENGTH, &info_log_length);
        if (info_log_length > 0){
            std::vector<char> error_message(info_log_length + 1);
            glGetShaderInfoLog(shader_id, info_log_length, nullptr, &error_message[0]);
            std::cout << &error_message[0] << std::endl;
        }

        return shader_id;
    };

    GLuint vertex_shader_id = compile(vertex_shader, GL_VERTEX_SHADER);
    GLuint fragment_shader_id = compile(fragment_shader, GL_FRAGMENT_SHADER);

    std::cout << "Linking program" << std::endl;
    GLuint program_id = glCreateProgram();
    glAttachShader(program_id, vertex_shader_id);
    glAttachShader(program_id, fragment_shader_id);
    glLinkProgram(program_id);

    GLint result = GL_FALSE;
    GLint info_log_length;
    glGetProgramiv(program_id, GL_LINK_STATUS, &result);
    glGetProgramiv(program_id, GL_INFO_LOG_LENGTH, &info_log_length);
    if (info_log_length > 0){
        std::vector<char> error_message(info_log_length + 1);
        glGetProgramInfoLog(program_id, info_log_length, nullptr, &error_message[0]);
        std::cout << &error_message[0] << std::endl;
    }

    glDetachShader(program_id, vertex_shader_id);
    glDetachShader(program_id, fragment_shader_id);

    glDeleteShader(vertex_shader_id);
    glDeleteShader(fragment_shader_id);

    return program_id;
}


std::mutex global_lock;
std::unordered_map<uint32_t, Object> world_objects;
Eigen::Vector2f velocity;
Eigen::Vector2f mouse_pos;
float rotation = 0.0f;
uint32_t me = 0;


void network() {
    if (enet_initialize() != 0) {
        std::cout << "An error occurred while initializing ENet." << std::endl;
        abort();
    }

    ENetAddress address;
    ENetHost* client;
    ENetPeer* server;

    client = enet_host_create(nullptr, 1, 2, 0, 0);

    if (!client) {
        std::cout << "An error occurred while trying to create an ENet client host." << std::endl;
        abort();
    }

    enet_address_set_host(&address, "127.0.0.1");
    address.port = 8111;

    server = enet_host_connect(client, &address, 2, 0);

    if (!server) {
        std::cout << "Can't start connecting." << std::endl;
        abort();
    }

    std::cout << "ENet connecting started." << std::endl;

    ENetEvent event;
    if (enet_host_service(client, &event, 5000) <= 0 || event.type != ENET_EVENT_TYPE_CONNECT) {
        std::cout << "Error connecting." << std::endl;
        abort();
    }

    while (enet_host_service(client, &event, 10) >= 0) {
        const std::lock_guard<std::mutex> lock(global_lock);

        switch (event.type) {
            case ENET_EVENT_TYPE_RECEIVE: {
                //printf("A packet of length %u containing %s was received from %s on channel %u.\n", event.packet->dataLength, event.packet->data, event.peer->data, event.channelID);

                proto::ObjectsVector objects_vector;
                objects_vector.ParseFromArray(event.packet->data, event.packet->dataLength);

                auto update_object = [](const proto::Object& proto_object) {
                    Object& object = world_objects[proto_object.id()];
                    object.id = proto_object.id();
                    object.color = Eigen::Vector3f(proto_object.color().r(), proto_object.color().g(), proto_object.color().b());
                    object.position = Eigen::Vector2f(proto_object.position().x(), proto_object.position().y());
                    object.velocity = Eigen::Vector2f(proto_object.velocity().x(), proto_object.velocity().y());
                    object.rotation = proto_object.rotation();
                };

                for (const proto::Object& proto_object : objects_vector.objects()) {
                    update_object(proto_object);
                }

                for (uint32_t id : objects_vector.objects_to_delete()) {
                    world_objects.erase(id);
                }

                if (objects_vector.me().size()) {
                    me = objects_vector.me(0);
                }

                enet_packet_destroy(event.packet);
                break;
            }

            case ENET_EVENT_TYPE_DISCONNECT: {
                printf("Connection closed.\n", event.peer->address.host);
                abort();
            }

            default:
                break;
        }

        if (world_objects.contains(me)) {
            Eigen::Vector2f diff = mouse_pos - world_objects.at(me).position;
            rotation = std::atan2(diff.y(), diff.x());
        }

        proto::UserUpdate uu;
        uu.mutable_velocity()->set_x(velocity.x());
        uu.mutable_velocity()->set_y(velocity.y());
        uu.set_rotation(rotation);

        auto data = uu.SerializeAsString();
        ENetPacket* packet = enet_packet_create(data.data(), data.size(), ENET_PACKET_FLAG_RELIABLE);
        enet_peer_send(server, 0, packet);
    }

    enet_peer_reset(server);
    enet_host_destroy(client);
    enet_deinitialize();
}


void update_mouse_pos(GLFWwindow* window, double x, double y) {
    int width, height;
    glfwGetWindowSize(window, &width, &height);
    mouse_pos = Eigen::Vector2f(x, height - y - 1) / 25.0f;
}

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

    glfwSetCursorPosCallback(window, update_mouse_pos);
    glfwSetWindowSizeCallback(window, window_size_callback);

    glClearColor(0.874509804f, 0.874509804f, 0.874509804f, 0.0f);

    GLuint vao_id;
    glGenVertexArrays(1, &vao_id);
    glBindVertexArray(vao_id);

    GLuint program_id = compile_shader(
R"(#version 330 core

layout(location = 0) in vec3 vertex;
uniform float rotation;
uniform vec2 position;
uniform vec2 window_size;

void main() {
    vec2 v = vertex.xy;
    v = vec2(v.x * cos(rotation) - v.y * sin(rotation), v.x * sin(rotation) + v.y * cos(rotation));
    gl_Position.xyz = vec3((v.x + position.x) / window_size.x, (v.y + position.y) / window_size.y, vertex.z) * 25;
    gl_Position.xyz = gl_Position.xyz * 2 - 1;
    gl_Position.w = 1.0;
})",

R"(#version 330 core

uniform vec3 color;
out vec3 out_color;

void main() {
	out_color = color;
})");

    static const GLfloat g_vertex_buffer_data[] = {
        -1.0f, -1.0f, 0.0f,
        1.0f, 0.0f, 0.0f,
        -1.0f, 1.0f, 0.0f,
    };

    GLuint vbo_id;
    glGenBuffers(1, &vbo_id);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_id);
    glBufferData(GL_ARRAY_BUFFER, sizeof(g_vertex_buffer_data), g_vertex_buffer_data, GL_STATIC_DRAW);
    glUseProgram(program_id);

    GLint color_loc = glGetUniformLocation(program_id, "color");
    GLint pos_loc = glGetUniformLocation(program_id, "position");
    GLint rot_loc = glGetUniformLocation(program_id, "rotation");
    GLint window_size_loc = glGetUniformLocation(program_id, "window_size");

    std::thread network_thread(&network);

    do {
        glClear(GL_COLOR_BUFFER_BIT);
        glEnableVertexAttribArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, vbo_id);

        glVertexAttribPointer(
            0,                  // attribute 0. No particular reason for 0, but must match the layout in the shader.
            3,                  // size
            GL_FLOAT,           // type
            GL_FALSE,           // normalized?
            0,                  // stride
            nullptr             // array buffer offset
        );

        int width, height;
        glfwGetWindowSize(window, &width, &height);
        glUniform2f(window_size_loc, width, height);

        {
            const std::lock_guard<std::mutex> lock(global_lock);

            velocity = Eigen::Vector2f(
                (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) - (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS),
                (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) - (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS));

            for (const auto&[id, object] : world_objects) {
                glUniform2f(pos_loc, object.position.x(), object.position.y());
                glUniform3f(color_loc, object.color.x(), object.color.y(), object.color.z());
                glUniform1f(rot_loc, object.rotation);
                glDrawArrays(GL_TRIANGLES, 0, 3);
            }
        }

        glDisableVertexAttribArray(0);

        glfwSwapBuffers(window);
        glfwPollEvents();
    } while (glfwGetKey(window, GLFW_KEY_ESCAPE) != GLFW_PRESS && glfwWindowShouldClose(window) == 0);

    glDeleteBuffers(1, &vbo_id);
    glDeleteVertexArrays(1, &vao_id);
    glDeleteProgram(program_id);

    glfwTerminate();

    return 0;
}
