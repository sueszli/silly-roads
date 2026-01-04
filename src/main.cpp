#include "game_state.hpp"
#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"
#include "terrain.hpp"

#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

//
// physics
//

constexpr float PHYS_ACCEL = 200.0f;     // acceleration per second
constexpr float PHYS_BRAKE = 400.0f;     // braking/reverse force
constexpr float PHYS_MAX_SPEED = 120.0f; // max speed
constexpr float PHYS_DRAG = 0.98f;       // drag coefficient
constexpr float PHYS_TURN_RATE = 2.0f;   // turn rate in rad/s

void update_physics_mut(GameState *state) {
    assert(state != nullptr);
    float dt = GetFrameTime();
    dt = std::min(dt, 0.1f);

    //
    // body - horizontal movement
    //

    // A/D steering & W/S drive force inputs
    if (std::abs(state->car_speed) > 0.5f) {
        float turn_factor = (state->car_speed > 0.0f) ? 1.0f : -1.0f; // reverse steering when going backward
        if (IsKeyDown(KEY_D)) {
            state->car_heading -= PHYS_TURN_RATE * dt * turn_factor;
        }
        if (IsKeyDown(KEY_A)) {
            state->car_heading += PHYS_TURN_RATE * dt * turn_factor;
        }
    }
    bool has_input = false;
    if (IsKeyDown(KEY_W)) {
        state->car_speed += PHYS_ACCEL * dt;
        has_input = true;
    }
    if (IsKeyDown(KEY_S)) {
        state->car_speed -= PHYS_BRAKE * dt;
        has_input = true;
    }

    // speed limits & drag
    if (state->car_speed > PHYS_MAX_SPEED)
        state->car_speed = PHYS_MAX_SPEED;
    if (state->car_speed < -PHYS_MAX_SPEED)
        state->car_speed = -PHYS_MAX_SPEED;
    state->car_speed *= PHYS_DRAG;
    if (!has_input && std::abs(state->car_speed) < 0.1f)
        state->car_speed = 0.0f;

    // calculate velocity & update horizontal position
    // moving body before sampling wheels prevents clipping at high speed
    state->car_vel.x = std::sin(state->car_heading) * state->car_speed;
    state->car_vel.z = std::cos(state->car_heading) * state->car_speed;
    state->car_pos.x += state->car_vel.x * dt;
    state->car_pos.z += state->car_vel.z * dt;

    //
    // wheel update
    //

    // wheel steering animation lerp
    constexpr float MAX_STEER_ANGLE = 0.52f; // 30 degrees
    constexpr float STEER_LERP_RATE = 8.0f;
    float target_steer = 0.0f;
    if (IsKeyDown(KEY_D)) {
        target_steer = -MAX_STEER_ANGLE;
    } else if (IsKeyDown(KEY_A)) {
        target_steer = MAX_STEER_ANGLE;
    }

    state->wheels[0].steering_angle += (target_steer - state->wheels[0].steering_angle) * STEER_LERP_RATE * dt;
    state->wheels[1].steering_angle += (target_steer - state->wheels[1].steering_angle) * STEER_LERP_RATE * dt;
    state->wheels[2].steering_angle = 0.0f;
    state->wheels[3].steering_angle = 0.0f;

    // wheel terrain sampling at new positions
    const float s = std::sin(state->car_heading);
    const float c = std::cos(state->car_heading);
    float h[4];
    float avg_h = 0.0f;
    for (int i = 0; i < 4; i++) {
        const Vector3 off = state->wheels[i].local_offset;
        const float wx = state->car_pos.x + (off.x * c + off.z * s);
        const float wz = state->car_pos.z + (-off.x * s + off.z * c);
        h[i] = get_terrain_height(wx, wz);
        state->wheel_heights[i] = h[i];
        avg_h += h[i];
    }
    avg_h *= 0.25f;

    //
    // body - vertical movement
    //

    float target_y = avg_h + 0.5f;
    if (state->car_pos.y < target_y)
        state->car_pos.y = target_y;
    state->car_pos.y += (target_y - state->car_pos.y) * 20.0f * dt;

    // body pitch and roll
    const float front_h = (h[0] + h[1]) * 0.5f;
    const float back_h = (h[2] + h[3]) * 0.5f;
    const float left_h = (h[0] + h[2]) * 0.5f;
    const float right_h = (h[1] + h[3]) * 0.5f;

    const float target_pitch = std::atan2(back_h - front_h, 3.0f);
    const float target_roll = std::atan2(right_h - left_h, 2.0f);

    state->car_pitch += (target_pitch - state->car_pitch) * 15.0f * dt;
    state->car_roll += (target_roll - state->car_roll) * 15.0f * dt;
}

//
// terrain
//

Texture2D load_terrain_texture() {
    // checkered terrain texture
    Image checked = GenImageChecked(256, 256, 128, 128, DARKGREEN, GREEN);
    Texture2D texture = LoadTextureFromImage(checked);
    assert(texture.id != 0);
    UnloadImage(checked);
    return texture;
}

void generate_terrain_mesh_mut(GameState *state) {
    // upload mesh to GPU and assign the texture
    assert(state != nullptr);
    if (state->mesh_generated) {
        UnloadModel(state->terrain_model);
    }
    state->terrain_mesh = generate_terrain_mesh_data(state->terrain_offset_x, state->terrain_offset_z);
    UploadMesh(&state->terrain_mesh, false);
    state->terrain_model = LoadModelFromMesh(state->terrain_mesh);
    state->terrain_model.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = state->texture;
    state->mesh_generated = true;
}

//
// loggng
//

void log_state(const GameState *state, std::int32_t frame) {
    assert(state != nullptr);
    std::printf("[FRAME %d] CAR_POS:%.2f %.2f %.2f | SPEED:%.2f | HEADING:%.2f | PITCH:%.2f | ROLL:%.2f | WHEEL_STEER:%.2f\n", frame, state->car_pos.x, state->car_pos.y, state->car_pos.z, state->car_speed, state->car_heading, state->car_pitch, state->car_roll, state->wheels[0].steering_angle);
}

//
// game loop
//

void update_terrain_mut(GameState *state) {
    assert(state != nullptr);
    // lazy init and regenerate terrain
    if (!state->mesh_generated) {
        generate_terrain_mesh_mut(state);
    }

    const float half_size = (GRID_SIZE - 1) * 0.5f;
    const float center_x = state->terrain_offset_x + half_size;
    const float center_z = state->terrain_offset_z + half_size;
    const float dist_x = std::abs(state->car_pos.x - center_x);
    const float dist_z = std::abs(state->car_pos.z - center_z);

    if (dist_x > 30.0f || dist_z > 30.0f) {
        state->terrain_offset_x = std::floor(state->car_pos.x - half_size);
        state->terrain_offset_z = std::floor(state->car_pos.z - half_size);
        generate_terrain_mesh_mut(state);
    }
}

void update_gameplay_mut(GameState *state) {
    assert(state != nullptr);
    // ensure target matches terrain height
    float target_h = get_terrain_height(state->target_pos.x, state->target_pos.z);
    state->target_pos.y = target_h;

    // check collection
    float dist_to_target = Vector3Distance(state->car_pos, state->target_pos);
    if (dist_to_target < 5.0f) {
        state->score++;
        std::printf("[GAME] collected! score: %d\n", state->score);

        // respawn target close to car
        state->target_pos.x = state->car_pos.x + (float)GetRandomValue(-40, 40);
        state->target_pos.z = state->car_pos.z + (float)GetRandomValue(-40, 40);
        state->target_pos.y = get_terrain_height(state->target_pos.x, state->target_pos.z);
    }
}

void update_camera_mut(GameState *state) {
    assert(state != nullptr);
    float dt = GetFrameTime();

    // update camera to follow car (chase cam)
    Vector3 target_cam_pos;
    target_cam_pos.x = state->car_pos.x - std::sin(state->car_heading) * 15.0f;
    target_cam_pos.z = state->car_pos.z - std::cos(state->car_heading) * 15.0f;
    target_cam_pos.y = state->car_pos.y + 8.0f;

    // smooth follow
    state->camera.position = Vector3Lerp(state->camera.position, target_cam_pos, dt * 3.0f);

    // check terrain collision for camera
    float cam_terrain_h = get_terrain_height(state->camera.position.x, state->camera.position.z);
    if (state->camera.position.y < cam_terrain_h + 2.0f) {
        state->camera.position.y = cam_terrain_h + 2.0f;
    }

    // look at car
    state->camera.target = state->car_pos;
}

void draw_frame(const GameState *state) {
    assert(state != nullptr);
    BeginDrawing();
    ClearBackground(SKYBLUE);
    BeginMode3D(state->camera);
    DrawModel(state->terrain_model, {state->terrain_offset_x, 0.0f, state->terrain_offset_z}, 1.0f, WHITE);

    // draw target
    DrawCylinder(state->target_pos, 1.0f, 1.0f, 4.0f, 16, YELLOW);
    DrawCylinderWires(state->target_pos, 1.0f, 1.0f, 4.0f, 16, ORANGE);

    // draw car with rotation
    rlPushMatrix();
    rlTranslatef(state->car_pos.x, state->car_pos.y, state->car_pos.z);
    rlRotatef(state->car_heading * RAD2DEG, 0.0f, 1.0f, 0.0f);
    rlRotatef(state->car_pitch * RAD2DEG, 1.0f, 0.0f, 0.0f);
    rlRotatef(state->car_roll * RAD2DEG, 0.0f, 0.0f, 1.0f);

    // pickup truck body (multi-box shape)
    // cabin: box at (0, 0.5, 0.8), size (1.8, 1.0, 1.5)
    DrawCube({0.0f, 0.5f, 0.8f}, 1.8f, 1.0f, 1.5f, BLUE);
    DrawCubeWires({0.0f, 0.5f, 0.8f}, 1.8f, 1.0f, 1.5f, DARKBLUE);
    // bed: box at (0, 0.3, -0.95), size (1.8, 0.6, 2.0)
    DrawCube({0.0f, 0.3f, -0.95f}, 1.8f, 0.6f, 2.0f, BLUE);
    DrawCubeWires({0.0f, 0.3f, -0.95f}, 1.8f, 0.6f, 2.0f, DARKBLUE);
    // hood: box at (0, 0.2, 1.8), size (1.6, 0.4, 0.8)
    DrawCube({0.0f, 0.2f, 1.8f}, 1.6f, 0.4f, 0.8f, BLUE);
    DrawCubeWires({0.0f, 0.2f, 1.8f}, 1.6f, 0.4f, 0.8f, DARKBLUE);

    // draw wheels (4 dark gray cylinders)
    for (int i = 0; i < 4; i++) {
        rlPushMatrix();
        Vector3 wheel_center = state->wheels[i].local_offset;
        rlTranslatef(wheel_center.x, wheel_center.y, wheel_center.z);

        // apply steering rotation to front wheels
        if (i < 2) { // front wheels
            rlRotatef(state->wheels[i].steering_angle * RAD2DEG, 0.0f, 1.0f, 0.0f);
        }

        Vector3 wheel_start = {-0.1f, 0.0f, 0.0f};
        Vector3 wheel_end = {0.1f, 0.0f, 0.0f};
        DrawCylinderEx(wheel_start, wheel_end, 0.3f, 0.3f, 16, DARKGRAY);
        rlPopMatrix();
    }

    rlPopMatrix();

    EndMode3D();

    // game stats
    char pos_text[64];
    std::snprintf(pos_text, sizeof(pos_text), "X: %.2f Y: %.2f Z: %.2f", state->car_pos.x, state->car_pos.y, state->car_pos.z);
    DrawText(pos_text, 10, 30, 30, BLACK);

    char score_text[32];
    std::snprintf(score_text, sizeof(score_text), "SCORE: %d", state->score);
    DrawText(score_text, 10, 60, 40, YELLOW);

    // log state
    log_state(state, state->frame_count);

    EndDrawing();
}

void game_loop_mut(GameState *state) {
    assert(state != nullptr);
    state->frame_count++;

    update_terrain_mut(state);
    update_physics_mut(state);
    update_gameplay_mut(state);
    update_camera_mut(state);
    draw_frame(state);
}

void spawn_initial_target(GameState *state) {
    if (state == nullptr) {
        return;
    }
    state->target_pos.x = state->car_pos.x + 20.0f; // start nearby for easy testing
    state->target_pos.z = state->car_pos.z + 20.0f;
    state->target_pos.y = get_terrain_height(state->target_pos.x, state->target_pos.z);
}

void init_road_mut(GameState *state) {
    assert(state != nullptr);
    if (state->road_initialized) {
        return;
    }

    // start near car
    float x = state->car_pos.x;
    float z = state->car_pos.z;
    float angle = 0.0f;

    for (int i = 0; i < GameState::ROAD_POINTS; i++) {
        state->road_points[i] = {x, 0.0f, z};
        state->road_points[i].y = get_terrain_height(x, z);

        // move forward in a winding path
        angle += 0.2f;
        x += std::cos(angle) * 10.0f;
        z += std::sin(angle) * 10.0f;
    }

    state->road_initialized = true;
    std::printf("[ROAD] Initialized %d points\n", GameState::ROAD_POINTS);
}

std::int32_t main() {
    InitWindow(800, 450, "raycer");
    SetTargetFPS(240);

    GameState state{};
    state.texture = load_terrain_texture();
    spawn_initial_target(&state);
    init_road_mut(&state);

    // game loop
    while (!WindowShouldClose()) {
        game_loop_mut(&state);
    }

    // cleanup
    UnloadModel(state.terrain_model);
    UnloadTexture(state.texture);
    CloseWindow();

    return EXIT_SUCCESS;
}
