#pragma once
#include "raylib.h"
#include <cstdint>

struct WheelState {
    Vector3 local_offset; // position relative to car body
    float steering_angle; // only non-zero for front wheels
    float spin_angle;     // rotation from rolling
};

struct GameState {
    Vector3 car_pos = {60.0f, 20.0f, 60.0f};
    Vector3 car_vel = {0.0f, 0.0f, 0.0f};
    float car_heading = 0.0f;
    float car_speed = 0.0f;
    float car_pitch = 0.0f;       // rotation around X-axis (front/back tilt)
    float car_roll = 0.0f;        // rotation around Z-axis (left/right tilt)
    float wheel_heights[4] = {0}; // FL, FR, RL, RR terrain heights
    WheelState wheels[4] = {
        {{-1.0f, -0.3f, 1.5f}, 0.0f, 0.0f},  // front-left
        {{1.0f, -0.3f, 1.5f}, 0.0f, 0.0f},   // front-right
        {{-1.0f, -0.3f, -1.5f}, 0.0f, 0.0f}, // rear-left
        {{1.0f, -0.3f, -1.5f}, 0.0f, 0.0f},  // rear-right
    };

    Camera3D camera = {
        .position = {0.0f, 10.0f, 10.0f},
        .target = {0.0f, 0.0f, 0.0f},
        .up = {0.0f, 1.0f, 0.0f},
        .fovy = 45.0f,
        .projection = CAMERA_PERSPECTIVE,
    };

    Mesh terrain_mesh = {};
    Model terrain_model = {};
    Texture2D texture = {};
    bool mesh_generated = false;
    float terrain_offset_x = 0.0f;
    float terrain_offset_z = 0.0f;

    Vector3 target_pos = {0.0f, 0.0f, 0.0f};
    std::int32_t score = 0;
    std::int32_t frame_count = 0;
};
