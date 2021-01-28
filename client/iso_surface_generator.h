#pragma once

#include <glm/glm.hpp>

class IIsoSurfaceGenerator {
public:
    struct Point {
        float Value;
        glm::vec3 Normal;
    };

    struct Mesh {
        std::vector<glm::vec3> Vertices;
        std::vector<glm::vec3> Normals;
        std::vector<unsigned int> Indices;
        size_t Count = 0;

        void Merge(Mesh& other) {
            if (!other.Indices.empty()) {
                Indices.reserve(Indices.size() + other.Indices.size());
                for (unsigned int index : other.Indices) {
                    Indices.emplace_back(index + Vertices.size());
                }
            }

            Vertices.insert(Vertices.end(), other.Vertices.begin(), other.Vertices.end());
            Normals.insert(Normals.end(), other.Normals.begin(), other.Normals.end());

            Count += other.Count;
        }

        void Clear() {
            Count = 0;
            Vertices.clear();
            Normals.clear();
            Indices.clear();
        }
    };

    virtual Mesh& Generate(const glm::ivec3& low, const glm::ivec3& high) = 0;

    virtual bool GetHardNormals() const = 0;
    virtual void SetHardNormals(bool flag) = 0;

    virtual std::function<Point(const glm::vec3& pos)> GetVolumeProvider() const = 0;
    virtual void SetVolumeProvider(std::function<Point(const glm::vec3& pos)> volumeProvider) = 0;

    virtual ~IIsoSurfaceGenerator() = default;
};
