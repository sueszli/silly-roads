#pragma once
#include "raylib.h"
#include <cstdint>
#include <vector>

struct CarControls {
    float throttle; // -1.0 (brake/reverse) to 1.0 (accel)
    float steer;    // -1.0 (left) to 1.0 (right)
};

struct WheelState {
    Vector3 local_offset; // position relative to car body
    float steering_angle; // only non-zero for front wheels
    float spin_angle;     // rotation from rolling
};

struct Car {
    Vector3 pos = {60.0f, 20.0f, 60.0f};
    Vector3 vel = {0.0f, 0.0f, 0.0f};
    float heading = 0.0f;
    float speed = 0.0f;
    float pitch = 0.0f; // rotation around X-axis (front/back tilt)
    float roll = 0.0f;  // rotation around Z-axis (left/right tilt)
    WheelState wheels[4] = {
        {{-1.0f, -0.3f, 1.5f}, 0.0f, 0.0f},  // front-left
        {{1.0f, -0.3f, 1.5f}, 0.0f, 0.0f},   // front-right
        {{-1.0f, -0.3f, -1.5f}, 0.0f, 0.0f}, // rear-left
        {{1.0f, -0.3f, -1.5f}, 0.0f, 0.0f},  // rear-right
    };
};

struct CameraState {
    Camera3D camera = {
        .position = {0.0f, 10.0f, 10.0f},
        .target = {0.0f, 0.0f, 0.0f},
        .up = {0.0f, 1.0f, 0.0f},
        .fovy = 45.0f,
        .projection = CAMERA_PERSPECTIVE,
    };
    Vector3 target_pos = {0.0f, 0.0f, 0.0f};
};

struct TerrainChunk {
    int cx; // chunk grid x coordinate
    int cz; // chunk grid z coordinate
    Model model;
};

struct GameState {
    Car car;
    CameraState camera;

    std::vector<TerrainChunk> terrain_chunks;
    Texture2D texture = {};
    float chunk_size = 0.0f; // stored for convenience (=(GRID_SIZE-1)*TILE_SIZE)

    std::int32_t frame_count = 0;
    bool road_path_generated = false;
};
