#pragma once

#include "raylib.h"
#include "raymath.h"
#include "terrain.hpp"

#include <cmath>

namespace Cam {

struct State {
    Camera3D camera = {
        .position = {0.0f, 10.0f, 10.0f},
        .target = {0.0f, 0.0f, 0.0f},
        .up = {0.0f, 1.0f, 0.0f},
        .fovy = 45.0f,
        .projection = CAMERA_PERSPECTIVE,
    };
};

inline void update(State &cam, const Vector3 &car_pos, float car_heading, float dt) {
    Vector3 target_cam_pos = {
        car_pos.x - std::sin(car_heading) * 15.0f,
        car_pos.y + 8.0f,
        car_pos.z - std::cos(car_heading) * 15.0f,
    };
    cam.camera.position = Vector3Lerp(cam.camera.position, target_cam_pos, dt * 3.0f);

    float cam_terrain_h = Terrain::get_height(cam.camera.position.x, cam.camera.position.z);
    if (cam.camera.position.y < cam_terrain_h + 2.0f) {
        cam.camera.position.y = cam_terrain_h + 2.0f;
    }

    cam.camera.target = car_pos;
}

} // namespace Cam
