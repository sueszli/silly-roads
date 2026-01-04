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

    //
    // terrain
    //

    Mesh terrain_mesh = {};
    Model terrain_model = {};
    Texture2D texture = {};
    bool mesh_generated = false;
    float terrain_offset_x = 0.0f;
    float terrain_offset_z = 0.0f;

    std::int32_t frame_count = 0;

    //
    // road
    //

    static constexpr std::int32_t ROAD_WINDOW_SIZE = 128; // sliding window of road points
    Vector3 road_points[ROAD_WINDOW_SIZE] = {};           // control points
    std::int32_t road_start_segment = 0;                  // global segment index of first point
    float road_progress = 0.0f;                           // car's progress along road (in segments)
    bool road_initialized = false;

    // road generation state (for continuous generation)
    float road_gen_x = 0.0f;                // last generated x position
    float road_gen_z = 0.0f;                // last generated z position
    float road_gen_angle = 0.0f;            // last generated angle
    std::int32_t road_gen_next_segment = 0; // next segment to generate

    // road rendering
    Mesh road_mesh = {};
    Model road_model = {};
    bool road_mesh_generated = false;
};
