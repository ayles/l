#include <iostream>
#include <object.pb.h>
#include <core/object.h>
#include <unordered_map>
#include <unordered_set>
#include <chrono>
#include <thread>
#include <mutex>
#include <enet/enet.h>
#include <iostream>

std::mutex global_lock;
std::unordered_map<uint32_t, Object> world_objects;
std::vector<uint32_t> objects_to_delete;
std::unordered_set<uint32_t> objects_to_create;
volatile bool stop = false;
volatile uint32_t next_id = 1;

uint64_t now() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();
}

void step(float delta) {
    const std::lock_guard<std::mutex> lock(global_lock);

    for (auto& [id, object] : world_objects) {
        object.position += object.velocity * delta;
    }
}

void set_peer_id(ENetPeer* peer, uint32_t id) {
    memcpy(&peer->data, &id, sizeof(uint32_t));
}

uint32_t get_peer_id(ENetPeer* peer) {
    uint32_t id;
    memcpy(&id, &peer->data, sizeof(uint32_t));
    return id;
}

void network() {
    if (enet_initialize() != 0) {
        std::cout << "An error occurred while initializing ENet." << std::endl;
        abort();
    }

    ENetAddress address;
    ENetHost* server;

    address.host = ENET_HOST_ANY;
    address.port = 8111;

    server = enet_host_create(&address, 32, 2, 0, 0);

    if (!server) {
        std::cout << "An error occurred while trying to create an ENet server host." << std::endl;
        abort();
    }

    std::cout << "ENet server started." << std::endl;

    uint64_t lastTime = now();
    ENetEvent event;
    while (enet_host_service(server, &event, 10) >= 0) {
        if (event.type == ENET_EVENT_TYPE_NONE && (now() - lastTime) < 10000000) {
            continue;
        }

        const std::lock_guard<std::mutex> lock(global_lock);

        switch (event.type) {
            case ENET_EVENT_TYPE_CONNECT: {
                uint32_t id = next_id++;
                set_peer_id(event.peer, id);
                printf("A new client connected from %x:%u, setting id %d\n", event.peer->address.host, event.peer->address.port, id);

                world_objects[id].id = id;
                world_objects[id].color = Eigen::Vector3f((float)rand() / RAND_MAX, (float)rand() / RAND_MAX, (float)rand() / RAND_MAX);
                objects_to_create.emplace(id);

                proto::ObjectsVector vector;
                vector.add_me(id);
                auto data = vector.SerializeAsString();
                ENetPacket* packet = enet_packet_create(data.data(), data.size(), ENET_PACKET_FLAG_RELIABLE);
                enet_peer_send(event.peer, 0, packet);

                break;
            }

            case ENET_EVENT_TYPE_RECEIVE: {
                //printf("A packet of length %u containing %s was received from %s on channel %u.\n", event.packet->dataLength, event.packet->data, event.peer->data, event.channelID);
                uint32_t id = get_peer_id(event.peer);
                proto::UserUpdate uu;
                uu.ParseFromArray(event.packet->data, event.packet->dataLength);
                world_objects[id].velocity = Eigen::Vector2f(std::clamp(uu.velocity().x(), -1.0f, 1.0f), std::clamp(uu.velocity().y(), -1.0f, 1.0f)) * 10.0f;
                world_objects[id].rotation = uu.rotation();

                enet_packet_destroy(event.packet);
                break;
            }

            case ENET_EVENT_TYPE_DISCONNECT: {
                uint32_t id = get_peer_id(event.peer);
                printf("%d disconnected.\n", id);
                world_objects.erase(id);
                objects_to_delete.push_back(id);
                break;
            }

            default:
                break;
        }

        if ((now() - lastTime) < 10000000) {
            continue;
        }

        lastTime = now();

        auto add_object = [](const Object& object) {
            proto::Object proto_object;
            proto_object.set_id(object.id);
            proto_object.mutable_color()->set_r(object.color.x());
            proto_object.mutable_color()->set_g(object.color.y());
            proto_object.mutable_color()->set_b(object.color.z());
            proto_object.mutable_position()->set_x(object.position.x());
            proto_object.mutable_position()->set_y(object.position.y());
            proto_object.mutable_velocity()->set_x(object.velocity.x());
            proto_object.mutable_velocity()->set_y(object.velocity.y());
            proto_object.set_rotation(object.rotation);
            return proto_object;
        };

        proto::ObjectsVector vector;
        for (uint32_t id : objects_to_delete) {
            vector.add_objects_to_delete(id);
        }
        for (uint32_t id : objects_to_create) {
            (*vector.add_objects()) = add_object(world_objects[id]);
        }
        for (const auto& [id, object] : world_objects) {
            if (!objects_to_create.contains(id)) {
                (*vector.add_objects()) = add_object(world_objects[id]);
            }
        }

        auto data = vector.SerializeAsString();
        ENetPacket* packet = enet_packet_create(data.data(), data.size(), (objects_to_delete.empty() && objects_to_create.empty()) ? (ENET_PACKET_FLAG_UNRELIABLE_FRAGMENT | ENET_PACKET_FLAG_UNSEQUENCED) : ENET_PACKET_FLAG_RELIABLE);
        enet_host_broadcast(server, 0, packet);

        objects_to_create.clear();
        objects_to_delete.clear();
    }

    enet_host_destroy(server);
    enet_deinitialize();

    stop = true;
}

int main() {
    std::thread net_thread(&network);

    uint64_t lastTime = now();
    while (!stop) {
        uint64_t currentTime = now();
        float delta = (currentTime - lastTime) / 1000000000.0f;
        lastTime = currentTime;

        step(delta);

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    net_thread.join();

    return 0;
}
