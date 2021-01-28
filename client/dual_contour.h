#pragma once

#include <client/qef_simd.h>
#include <client/thread_pool.h>
#include <client/iso_surface_generator.h>

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/hash.hpp>

#include <functional>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <array>
#include <cassert>
#include <mutex>



class DualContour : public IIsoSurfaceGenerator {
public:


public:
    DualContour()
        : ThreadPool_(std::thread::hardware_concurrency())
    {
        std::cout << std::thread::hardware_concurrency() << std::endl;
    }

    Mesh& Generate(const glm::ivec3& low, const glm::ivec3& high) override {
        std::array<std::future<void>, Shards_> results;
        for (size_t i = 0; i < Shards_; ++i) {
            results[i] = ThreadPool_.enqueue([i, this, &low, &high]() {
                glm::ivec3 pos;
                for (pos.x = low.x + i; pos.x < high.x; pos.x += Shards_) {
                    for (pos.y = low.y; pos.y < high.y; ++pos.y) {
                        for (pos.z = low.z; pos.z < high.z; ++pos.z) {
                            glm::vec3 v = CalcBestVertex(pos);

                            if (v.x != v.x) {
                                continue;
                            }

                            SetToMapping(pos, v);
                        }
                    }
                }
            });
        }

        for (size_t i = 0; i < Shards_; ++i) {
            results[i].get();
        }
        
        std::array<Mesh, Shards_> meshes;
        Mesh_.Clear();

        auto addTriangle = [this](const glm::vec3& p0, const glm::vec3& p1, const glm::vec3& p2, bool swap, Mesh& mesh) {
            //mesh.Indices.emplace_back(mesh.Vertices.size());
            //mesh.Indices.emplace_back(mesh.Vertices.size() + 1);
            //mesh.Indices.emplace_back(mesh.Vertices.size() + 2);
            if (swap) {
                if (HardNormals_) {
                    glm::vec3 norm = glm::normalize(glm::cross((p1 - p0), (p2 - p0)));
                    mesh.Normals.emplace_back(norm);
                    mesh.Normals.emplace_back(norm);
                    mesh.Normals.emplace_back(norm);
                } else {
                    mesh.Normals.emplace_back(VolumeProvider_(p0).Normal);
                    mesh.Normals.emplace_back(VolumeProvider_(p1).Normal);
                    mesh.Normals.emplace_back(VolumeProvider_(p2).Normal);
                }

                mesh.Vertices.emplace_back(p0);
                mesh.Vertices.emplace_back(p1);
                mesh.Vertices.emplace_back(p2);
                mesh.Count += 3;
            } else {
                if (HardNormals_) {
                    glm::vec3 norm = glm::normalize(glm::cross((p2 - p0), (p1 - p0)));
                    mesh.Normals.emplace_back(norm);
                    mesh.Normals.emplace_back(norm);
                    mesh.Normals.emplace_back(norm);
                } else {
                    mesh.Normals.emplace_back(VolumeProvider_(p2).Normal);
                    mesh.Normals.emplace_back(VolumeProvider_(p1).Normal);
                    mesh.Normals.emplace_back(VolumeProvider_(p0).Normal);
                }
                mesh.Vertices.emplace_back(p2);
                mesh.Vertices.emplace_back(p1);
                mesh.Vertices.emplace_back(p0);
                mesh.Count += 3;
            }
        };

        for (size_t i = 0; i < Shards_; ++i) {
            results[i] = ThreadPool_.enqueue([i, this, &low, &high, &meshes, &addTriangle]() {
                glm::ivec3 pos;
                for (pos.x = low.x + i; pos.x < high.x; pos.x += Shards_) {
                    for (pos.y = low.y; pos.y < high.y; ++pos.y) {
                        for (pos.z = low.z; pos.z < high.z; ++pos.z) {
                            if (pos.x > low.x && pos.y > low.y) {
                                bool s1 = std::signbit(VolumeProvider_(glm::vec3(pos.x, pos.y, pos.z + 0)).Value);
                                bool s2 = std::signbit(VolumeProvider_(glm::vec3(pos.x, pos.y, pos.z + 1)).Value);
                                if (s1 != s2) {
                                    addTriangle(
                                        GetFromMapping(glm::ivec3(pos.x - 1, pos.y - 1, pos.z)),
                                        GetFromMapping(glm::ivec3(pos.x - 0, pos.y - 1, pos.z)),
                                        GetFromMapping(glm::ivec3(pos.x - 0, pos.y - 0, pos.z)),
                                        s2, meshes[i]);

                                    addTriangle(
                                        GetFromMapping(glm::ivec3(pos.x - 1, pos.y - 0, pos.z)),
                                        GetFromMapping(glm::ivec3(pos.x - 1, pos.y - 1, pos.z)),
                                        GetFromMapping(glm::ivec3(pos.x - 0, pos.y - 0, pos.z)),
                                        s2, meshes[i]);
                                }
                            }
                            if (pos.x > low.x && pos.z > low.z) {
                                bool s1 = std::signbit(VolumeProvider_(glm::vec3(pos.x, pos.y + 0, pos.z)).Value);
                                bool s2 = std::signbit(VolumeProvider_(glm::vec3(pos.x, pos.y + 1, pos.z)).Value);
                                if (s1 != s2) {
                                    addTriangle(
                                        GetFromMapping(glm::ivec3(pos.x - 1, pos.y, pos.z - 1)),
                                        GetFromMapping(glm::ivec3(pos.x - 0, pos.y, pos.z - 1)),
                                        GetFromMapping(glm::ivec3(pos.x - 0, pos.y, pos.z - 0)),
                                        s1, meshes[i]);

                                    addTriangle(
                                        GetFromMapping(glm::ivec3(pos.x - 1, pos.y, pos.z - 0)),
                                        GetFromMapping(glm::ivec3(pos.x - 1, pos.y, pos.z - 1)),
                                        GetFromMapping(glm::ivec3(pos.x - 0, pos.y, pos.z - 0)),
                                        s1, meshes[i]);
                                }
                            }
                            if (pos.y > low.y && pos.z > low.z) {
                                bool s1 = std::signbit(VolumeProvider_(glm::vec3(pos.x + 0, pos.y, pos.z)).Value);
                                bool s2 = std::signbit(VolumeProvider_(glm::vec3(pos.x + 1, pos.y, pos.z)).Value);
                                if (s1 != s2) {
                                    addTriangle(
                                        GetFromMapping(glm::ivec3(pos.x, pos.y - 1, pos.z - 1)),
                                        GetFromMapping(glm::ivec3(pos.x, pos.y - 0, pos.z - 1)),
                                        GetFromMapping(glm::ivec3(pos.x, pos.y - 0, pos.z - 0)),
                                        s2, meshes[i]);

                                    addTriangle(
                                        GetFromMapping(glm::ivec3(pos.x, pos.y - 1, pos.z - 0)),
                                        GetFromMapping(glm::ivec3(pos.x, pos.y - 1, pos.z - 1)),
                                        GetFromMapping(glm::ivec3(pos.x, pos.y - 0, pos.z - 0)),
                                        s2, meshes[i]);
                                }
                            }
                        }
                    }
                }

                std::lock_guard<std::mutex> lock(Lock_);
                Mesh_.Merge(meshes[i]);
            });
        }

        for (size_t i = 0; i < Shards_; ++i) {
            results[i].get();
        }

        return Mesh_;
    }

private:
    glm::vec3 CalcBestVertex(const glm::vec3& pos) {
        //return pos + glm::vec3(0.5);

        auto adapt = [](float v0, float v1) {
            return (0.0f - v0) / (v1 - v0);
        };

        glm::mat2 v[2];
        for (size_t dx : { 0, 1 }) {
            for (size_t dy : { 0, 1 }) {
                for (size_t dz : { 0, 1 }) {
                    v[dx][dy][dz] = VolumeProvider_(glm::vec3(pos.x + dx, pos.y + dy, pos.z + dz)).Value;
                }
            }
        }

        std::vector<glm::vec3> changes;

        for (size_t dx : { 0, 1 }) {
            for (size_t dy : { 0, 1 }) {
                if (std::signbit(v[dx][dy][0]) != std::signbit(v[dx][dy][1]))
                    changes.emplace_back(glm::vec3(pos.x + dx, pos.y + dy, pos.z + adapt(v[dx][dy][0], v[dx][dy][1])));
            }
        }

        for (size_t dx : { 0, 1 }) {
            for (size_t dz : { 0, 1 }) {
                if (std::signbit(v[dx][0][dz]) != std::signbit(v[dx][1][dz]))
                    changes.emplace_back(glm::vec3(pos.x + dx, pos.y + adapt(v[dx][0][dz], v[dx][1][dz]), pos.z + dz));
            }
        }

        for (size_t dy : { 0, 1 }) {
            for (size_t dz : { 0, 1 }) {
                if (std::signbit(v[0][dy][dz]) != std::signbit(v[1][dy][dz]))
                    changes.emplace_back(glm::vec3(pos.x + adapt(v[0][dy][dz], v[1][dy][dz]), pos.y + dy, pos.z + dz));
            }
        }

        if (changes.size() < 2) {
            return glm::vec3(std::numeric_limits<float>::quiet_NaN());
        }

        std::vector<glm::vec3> normals;
        glm::vec3 c;
        float i = 0;
        for (auto& vv : changes) {
            normals.emplace_back(VolumeProvider_(vv).Normal);
            c += vv;
            ++i;
        }
        c /= i;

        float biasStrength = 0.001;
        if (false) {
            normals.emplace_back(glm::vec3(biasStrength, 0, 0));
            normals.emplace_back(glm::vec3(0, biasStrength, 0));
            normals.emplace_back(glm::vec3(0, 0, biasStrength));
            changes.emplace_back(c);
            changes.emplace_back(c);
            changes.emplace_back(c);
        }

        glm::vec3 res;
        qef_solve_from_points_3d(glm::value_ptr(changes[0]), glm::value_ptr(normals[0]), changes.size(), glm::value_ptr(res));
        return glm::clamp(res - pos, 0.0f, 1.0f) + pos;
    }

    bool GetHardNormals() const override {
        return HardNormals_;
    }

    void SetHardNormals(bool flag) override {
        HardNormals_ = flag;
    }

    std::function<Point(const glm::vec3& pos)> GetVolumeProvider() const override {
        return VolumeProvider_;
    }

    void SetVolumeProvider(std::function<Point(const glm::vec3& pos)> volumeProvider) override {
        VolumeProvider_ = volumeProvider;
    }

private:
    inline const glm::vec3& GetFromMapping(const glm::ivec3& p) const {
        return Mapping_[p.x & (Shards_ - 1)].at(p);
    }

    inline void SetToMapping(const glm::ivec3& p, const glm::vec3& v) {
        Mapping_[p.x & (Shards_ - 1)][p] = v;
    }

private:
    static constexpr size_t Shards_ = 16;
    Mesh Mesh_;
    bool HardNormals_ = false;
    std::function<Point(const glm::vec3& pos)> VolumeProvider_;
    std::array<std::unordered_map<glm::ivec3, glm::vec3>, Shards_> Mapping_;
    ThreadPool ThreadPool_;
    std::mutex Lock_;
};
