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

constexpr float BALL_RADIUS = 0.5f;
constexpr Vector3 PHYS_GRAVITY{0.0f, -500.0f, 0.0f}; // gravitational force
constexpr float AIR_TIME_GRAVITY_MULTIPLIER = 0.6f;  // reduce gravity in air
constexpr float PHYS_ACCEL = 200.0f;                 // acceleration per second
constexpr float PHYS_BRAKE = 400.0f;                 // braking/reverse force
constexpr float PHYS_MAX_SPEED = 120.0f;             // max speed
constexpr float PHYS_DRAG = 0.98f;                   // drag coefficient
constexpr float PHYS_TURN_RATE = 2.0f;               // turn rate in rad/s

void update_physics_mut(GameState *state) {
    assert(state != nullptr);
    float dt = GetFrameTime();
    dt = std::min(dt, 0.05f);

    // A/D steering (only when moving)
    if (std::abs(state->car_speed) > 0.5f) {
        float turn_factor = (state->car_speed > 0.0f) ? 1.0f : -1.0f; // reverse steering when going backward
        if (IsKeyDown(KEY_A)) {
            state->car_heading += PHYS_TURN_RATE * dt * turn_factor;
        }
        if (IsKeyDown(KEY_D)) {
            state->car_heading -= PHYS_TURN_RATE * dt * turn_factor;
        }
    }

    // W/S drive force
    bool has_input = false;
    if (IsKeyDown(KEY_W)) {
        state->car_speed += PHYS_ACCEL * dt;
        has_input = true;
    }
    if (IsKeyDown(KEY_S)) {
        state->car_speed -= PHYS_BRAKE * dt;
        has_input = true;
    }

    // limit speed
    if (state->car_speed > PHYS_MAX_SPEED)
        state->car_speed = PHYS_MAX_SPEED;
    if (state->car_speed < -PHYS_MAX_SPEED)
        state->car_speed = -PHYS_MAX_SPEED;

    // apply drag
    state->car_speed *= PHYS_DRAG;

    // snap to 0 if very slow and no input
    if (!has_input && std::abs(state->car_speed) < 0.1f) {
        state->car_speed = 0.0f;
    }

    // calculate horizontal velocity from heading and speed
    state->car_vel.x = std::sin(state->car_heading) * state->car_speed;
    state->car_vel.z = std::cos(state->car_heading) * state->car_speed;

    float terrain_h = get_terrain_height(state->car_pos.x, state->car_pos.z);
    bool is_on_ground = (state->car_pos.y <= terrain_h + BALL_RADIUS);

    // soften gravity if mid-air and holding input
    Vector3 effective_gravity = PHYS_GRAVITY;
    if (!is_on_ground && has_input) {
        effective_gravity = Vector3Scale(PHYS_GRAVITY, AIR_TIME_GRAVITY_MULTIPLIER);
    }

    if (!is_on_ground) {
        // free fall
        state->car_vel.y += effective_gravity.y * dt;
    } else {
        // ground physics
        Vector3 terrain_normal = get_terrain_normal(state->car_pos.x, state->car_pos.z);

        // match vertical velocity to slope
        // v dot n = 0 -> v_y * n_y + v_x * n_x + v_z * n_z = 0
        // v_y = -(v_x * n_x + v_z * n_z) / n_y
        if (terrain_normal.y > 0.001f) {
            float slope_y = -(state->car_vel.x * terrain_normal.x + state->car_vel.z * terrain_normal.z) / terrain_normal.y;
            // only apply if it keeps us above ground or pulling down
            state->car_vel.y = slope_y;
        }

        // keep ball on surface
        if (state->car_pos.y < terrain_h + BALL_RADIUS) {
            state->car_pos.y = terrain_h + BALL_RADIUS;
            // damping vertical velocity if slamming into ground?
            // For arcade feel, we just stick to ground usually.
            if (state->car_vel.y < 0)
                state->car_vel.y = 0;
        }
    }

    // update position
    state->car_pos = Vector3Add(state->car_pos, Vector3Scale(state->car_vel, dt));
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
    float terrain_h = get_terrain_height(state->car_pos.x, state->car_pos.z);
    bool is_on_ground = (state->car_pos.y <= terrain_h + BALL_RADIUS + 0.01f); // check grounded with epsilon

    std::printf("[FRAME %d] CAR_POS:%.2f %.2f %.2f | CAR_VEL:%.2f %.2f %.2f | SPEED:%.2f | HEADING:%.2f | GROUND:%d\n", frame, state->car_pos.x, state->car_pos.y, state->car_pos.z, state->car_vel.x, state->car_vel.y, state->car_vel.z, state->car_speed, state->car_heading, is_on_ground ? 1 : 0);
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
    DrawCube({0.0f, 0.0f, 0.0f}, 2.0f, 1.0f, 4.0f, RED);
    DrawCubeWires({0.0f, 0.0f, 0.0f}, 2.0f, 1.0f, 4.0f, MAROON);
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

std::int32_t main() {
    InitWindow(800, 450, "raycer");
    SetTargetFPS(240);

    GameState state{};
    state.texture = load_terrain_texture();
    spawn_initial_target(&state);

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
