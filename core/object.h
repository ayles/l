#pragma once

#include <Eigen/Dense>

class Object {
public:
    uint32_t id;
    Eigen::Vector3f color;
    Eigen::Vector2f position;
    Eigen::Vector2f velocity;
    float rotation;
};
