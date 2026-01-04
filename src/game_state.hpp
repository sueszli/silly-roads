#pragma once
#include "raylib.h"

struct GameState {
    Vector3 ball_pos = {60.0f, 20.0f, 60.0f};
    Vector3 ball_vel = {0.0f, 0.0f, 0.0f};

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
};
