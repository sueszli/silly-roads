#include "game_state.hpp"
#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"
#include "terrain.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

// Physics Constants
constexpr float PHYS_ACCEL = 200.0f;     // acceleration per second
constexpr float PHYS_BRAKE = 400.0f;     // braking/reverse force
constexpr float PHYS_MAX_SPEED = 120.0f; // max speed
constexpr float PHYS_DRAG = 0.98f;       // drag coefficient
constexpr float PHYS_TURN_RATE = 2.0f;   // turn rate in rad/s

CarControls read_inputs() {
    CarControls inputs = {};

    // A/D steering
    if (IsKeyDown(KEY_D)) {
        inputs.steer = 1.0f;
    } else if (IsKeyDown(KEY_A)) {
        inputs.steer = -1.0f;
    }

    // W/S throttle
    if (IsKeyDown(KEY_W)) {
        inputs.throttle = 1.0f;
    } else if (IsKeyDown(KEY_S)) {
        inputs.throttle = -1.0f;
    }

    return inputs;
}

void update_physics(GameState &state, const CarControls &inputs, float dt) {
    Car &car = state.car;

    // Steering
    if (std::abs(car.speed) > 0.5f) {
        float turn_factor = (car.speed > 0.0f) ? 1.0f : -1.0f; // reverse steering when going backward
        car.heading -= inputs.steer * PHYS_TURN_RATE * dt * turn_factor;
    }

    // Acceleration / Braking
    bool has_input = false;
    if (inputs.throttle > 0.0f) {
        car.speed += PHYS_ACCEL * inputs.throttle * dt;
        has_input = true;
    } else if (inputs.throttle < 0.0f) {
        car.speed += PHYS_BRAKE * inputs.throttle * dt; // throttle is negative here
        has_input = true;
    }

    // speed limits & drag
    if (car.speed > PHYS_MAX_SPEED)
        car.speed = PHYS_MAX_SPEED;
    if (car.speed < -PHYS_MAX_SPEED)
        car.speed = -PHYS_MAX_SPEED;
    car.speed *= PHYS_DRAG;
    if (!has_input && std::abs(car.speed) < 0.1f)
        car.speed = 0.0f;

    // calculate velocity & update horizontal position
    // moving body before sampling wheels prevents clipping at high speed
    car.vel.x = std::sin(car.heading) * car.speed;
    car.vel.z = std::cos(car.heading) * car.speed;
    car.pos.x += car.vel.x * dt;
    car.pos.z += car.vel.z * dt;

    //
    // wheel update
    //

    // wheel steering animation lerp
    constexpr float MAX_STEER_ANGLE = 0.52f; // 30 degrees
    constexpr float STEER_LERP_RATE = 8.0f;
    float target_steer = -inputs.steer * MAX_STEER_ANGLE;

    car.wheels[0].steering_angle += (target_steer - car.wheels[0].steering_angle) * STEER_LERP_RATE * dt;
    car.wheels[1].steering_angle += (target_steer - car.wheels[1].steering_angle) * STEER_LERP_RATE * dt;
    car.wheels[2].steering_angle = 0.0f;
    car.wheels[3].steering_angle = 0.0f;

    // wheel terrain sampling at new positions
    const float s = std::sin(car.heading);
    const float c = std::cos(car.heading);
    float h[4];
    float avg_h = 0.0f;
    for (int i = 0; i < 4; i++) {
        const Vector3 off = car.wheels[i].local_offset;
        const float wx = car.pos.x + (off.x * c + off.z * s);
        const float wz = car.pos.z + (-off.x * s + off.z * c);

        // Using global get_terrain_height from terrain.hpp
        h[i] = Terrain::get_height(wx, wz);

        // car.wheel_heights[i] = h[i]; // Removed caching
        avg_h += h[i];
    }
    avg_h *= 0.25f;

    //
    // body - vertical movement
    //

    float target_y = avg_h + 0.5f;
    if (car.pos.y < target_y)
        car.pos.y = target_y;
    car.pos.y += (target_y - car.pos.y) * 20.0f * dt;

    // body pitch and roll
    const float front_h = (h[0] + h[1]) * 0.5f;
    const float back_h = (h[2] + h[3]) * 0.5f;
    const float left_h = (h[0] + h[2]) * 0.5f;
    const float right_h = (h[1] + h[3]) * 0.5f;

    const float target_pitch = std::atan2(back_h - front_h, 3.0f);
    const float target_roll = std::atan2(right_h - left_h, 2.0f);

    car.pitch += (target_pitch - car.pitch) * 15.0f * dt;
    car.roll += (target_roll - car.roll) * 15.0f * dt;
}

void update_camera(GameState &state, float dt) {
    CameraState &cam = state.camera;
    const Car &car = state.car;

    // update camera to follow car (chase cam)
    Vector3 target_cam_pos;
    target_cam_pos.x = car.pos.x - std::sin(car.heading) * 15.0f;
    target_cam_pos.z = car.pos.z - std::cos(car.heading) * 15.0f;
    target_cam_pos.y = car.pos.y + 8.0f;

    // smooth follow
    cam.camera.position = Vector3Lerp(cam.camera.position, target_cam_pos, dt * 3.0f);

    // check terrain collision for camera
    float cam_terrain_h = Terrain::get_height(cam.camera.position.x, cam.camera.position.z);
    if (cam.camera.position.y < cam_terrain_h + 2.0f) {
        cam.camera.position.y = cam_terrain_h + 2.0f;
    }
    cam.camera.target = car.pos;
}

void draw_car(const Car &car) {
    rlPushMatrix();
    rlTranslatef(car.pos.x, car.pos.y, car.pos.z);

    // car body
    rlRotatef(car.heading * RAD2DEG, 0.0f, 1.0f, 0.0f);
    rlRotatef(car.pitch * RAD2DEG, 1.0f, 0.0f, 0.0f);
    rlRotatef(car.roll * RAD2DEG, 0.0f, 0.0f, 1.0f);

    // simple box car
    DrawCube({0.0f, 0.5f, 0.0f}, 2.0f, 1.0f, 4.0f, RED);
    DrawCube({0.0f, 0.5f, 1.0f}, 1.8f, 0.8f, 2.0f, MAROON); // cabin

    // wheels
    for (int i = 0; i < 4; i++) {
        rlPushMatrix();
        rlTranslatef(car.wheels[i].local_offset.x, car.wheels[i].local_offset.y, car.wheels[i].local_offset.z);

        // steering
        if (i < 2) {
            rlRotatef(car.wheels[i].steering_angle * RAD2DEG, 0.0f, 1.0f, 0.0f);
        }

        Vector3 wheel_start = {-0.1f, 0.0f, 0.0f};
        Vector3 wheel_end = {0.1f, 0.0f, 0.0f};
        DrawCylinderEx(wheel_start, wheel_end, 0.3f, 0.3f, 16, DARKGRAY);
        rlPopMatrix();
    }

    rlPopMatrix();
}

void draw_scene(const GameState &state) {
    BeginDrawing();
    ClearBackground(SKYBLUE);

    BeginMode3D(state.camera.camera);

    // Draw all chunks
    for (const auto &chunk : state.terrain_chunks) {
        float chunk_world_size = (Terrain::GRID_SIZE - 1) * Terrain::TILE_SIZE;
        float ox = (float)chunk.cx * chunk_world_size;
        float oz = (float)chunk.cz * chunk_world_size;
        Vector3 pos = {ox, 0.0f, oz};

        DrawModel(chunk.model, pos, 1.0f, WHITE);
    }

    // car rendering
    draw_car(state.car);

    EndMode3D();

    // game stats
    char pos_text[64];
    std::snprintf(pos_text, sizeof(pos_text), "X: %.2f Y: %.2f Z: %.2f", state.car.pos.x, state.car.pos.y, state.car.pos.z);

    // draw speed on screen
    char speed_text[64];
    std::snprintf(speed_text, sizeof(speed_text), "SPEED: %.2f", state.car.speed);
    DrawText(speed_text, 10, 40, 20, WHITE);
    DrawText(pos_text, 10, 60, 20, LIGHTGRAY); // Show Stats

    EndDrawing();
}

std::int32_t main() {
    InitWindow(800, 450, "raycer");
    SetTargetFPS(240);

    GameState state{};
    state.texture = Terrain::load_texture();
    // Initial car placement on road
    float start_z = 0.0f;
    float start_x = Terrain::get_road_offset(start_z);

    state.car.pos = {start_x, 0.0f, start_z};
    state.car.pos.y = Terrain::get_height(start_x, start_z) + 2.0f;

    // Face the road direction (approximate derivative)
    float look_ahead_z = start_z + 1.0f;
    float look_ahead_x = Terrain::get_road_offset(look_ahead_z);

    float dx = look_ahead_x - start_x;
    float dz = look_ahead_z - start_z;
    state.car.heading = std::atan2(dx, dz);

    // Force initial terrain update for chunks
    Terrain::update(&state);

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();
        dt = std::min(dt, 0.1f);

        // Update
        CarControls controls = read_inputs();

        Terrain::update(&state);

        update_physics(state, controls, dt);
        update_camera(state, dt);

        // Draw
        draw_scene(state);
    }

    // Cleanup
    for (const auto &chunk : state.terrain_chunks) {
        UnloadModel(chunk.model);
    }
    UnloadTexture(state.texture);
    CloseWindow();

    return EXIT_SUCCESS;
}
