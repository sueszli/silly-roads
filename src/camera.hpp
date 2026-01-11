#pragma once

#include "car.hpp"
#include "raylib.h"
#include "raymath.h"
#include "terrain.hpp"

#include <cmath>

namespace Cam {

/** updates and returns the camera (follows the car) */
inline Camera3D &update(float dt) {
    static Camera3D camera = {
        .position = {0.0f, 10.0f, 10.0f},
        .target = {0.0f, 0.0f, 0.0f},
        .up = {0.0f, 1.0f, 0.0f},
        .fovy = 45.0f,
        .projection = CAMERA_PERSPECTIVE,
    };

    Vector3 car_pos = Car::get_position();
    float car_heading = Car::get_heading();

    Vector3 target_cam_pos = {
        car_pos.x - std::sin(car_heading) * 15.0f,
        car_pos.y + 8.0f,
        car_pos.z - std::cos(car_heading) * 15.0f,
    };
    camera.position = Vector3Lerp(camera.position, target_cam_pos, dt * 3.0f);

    float cam_terrain_h = Terrain::get_height(camera.position.x, camera.position.z);
    if (camera.position.y < cam_terrain_h + 2.0f) {
        camera.position.y = cam_terrain_h + 2.0f;
    }

    camera.target = car_pos;
    return camera;
}

} // namespace Cam