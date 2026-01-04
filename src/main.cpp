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
constexpr float PHYS_MOVE_FORCE = 400.0f;            // directional speed by player
constexpr float PHYS_DRAG = 0.995f;                  // drag coefficient (so the ball halts eventually)

void update_physics_mut(GameState *state) {
    assert(state != nullptr);
    float dt = GetFrameTime();
    dt = std::min(dt, 0.05f);

    // handle input (relative to camera)
    Vector3 cam_fwd = Vector3Subtract(state->camera.target, state->camera.position);
    cam_fwd.y = 0.0f;
    cam_fwd = Vector3Normalize(cam_fwd);
    Vector3 cam_right = Vector3CrossProduct(cam_fwd, {0.0f, 1.0f, 0.0f});
    cam_right = Vector3Normalize(cam_right);
    Vector3 input_dir = {0.0f, 0.0f, 0.0f};
    if (IsKeyDown(KEY_W)) {
        input_dir = Vector3Add(input_dir, cam_fwd);
    }
    if (IsKeyDown(KEY_S)) {
        input_dir = Vector3Subtract(input_dir, cam_fwd);
    }
    if (IsKeyDown(KEY_A)) {
        input_dir = Vector3Subtract(input_dir, cam_right);
    }
    if (IsKeyDown(KEY_D)) {
        input_dir = Vector3Add(input_dir, cam_right);
    }

    bool has_input = Vector3Length(input_dir) > 0.1f;
    if (has_input) {
        input_dir = Vector3Normalize(input_dir);
        state->car_vel = Vector3Add(state->car_vel, Vector3Scale(input_dir, PHYS_MOVE_FORCE * dt));
    }

    float terrain_h = get_terrain_height(state->car_pos.x, state->car_pos.z);
    bool is_on_ground = (state->car_pos.y <= terrain_h + BALL_RADIUS);

    // soften gravity if mid-air and holding input
    Vector3 effective_gravity = PHYS_GRAVITY;
    if (!is_on_ground && has_input) {
        effective_gravity = Vector3Scale(PHYS_GRAVITY, AIR_TIME_GRAVITY_MULTIPLIER);
    }

    if (!is_on_ground) {
        // free fall
        state->car_vel = Vector3Add(state->car_vel, Vector3Scale(effective_gravity, dt));
    } else {
        // ground physics
        Vector3 terrain_normal = get_terrain_normal(state->car_pos.x, state->car_pos.z);

        // apply slope acceleration (gravity parallel to surface)
        float g_dot_n = Vector3DotProduct(effective_gravity, terrain_normal);
        Vector3 gravity_parallel = Vector3Subtract(effective_gravity, Vector3Scale(terrain_normal, g_dot_n));
        state->car_vel = Vector3Add(state->car_vel, Vector3Scale(gravity_parallel, dt));

        // project velocity to be tangent to surface (prevent tunneling into ground)
        float v_dot_n = Vector3DotProduct(state->car_vel, terrain_normal);
        if (v_dot_n < 0.0f) {
            state->car_vel = Vector3Subtract(state->car_vel, Vector3Scale(terrain_normal, v_dot_n));
        }

        // keep ball on surface if not launching
        if (state->car_pos.y < terrain_h + BALL_RADIUS) {
            state->car_pos.y = terrain_h + BALL_RADIUS;
        }
    }

    // update position
    state->car_pos = Vector3Add(state->car_pos, Vector3Scale(state->car_vel, dt));

    // apply drag
    state->car_vel = Vector3Scale(state->car_vel, PHYS_DRAG);
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

    std::printf("[FRAME %d] CAR_POS:%.2f %.2f %.2f | CAR_VEL:%.2f %.2f %.2f | GROUND:%d\n", frame, state->car_pos.x, state->car_pos.y, state->car_pos.z, state->car_vel.x, state->car_vel.y, state->car_vel.z, is_on_ground ? 1 : 0);
}

//
// game loop
//

void game_loop_mut(GameState *state) {
    assert(state != nullptr);
    static std::int32_t frame = 0;
    frame++;

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

    // update physics
    update_physics_mut(state);

    // update camera to follow ball
    state->camera.target = state->car_pos;
    state->camera.position = {state->car_pos.x, state->car_pos.y + 15.0f, state->car_pos.z + 15.0f};

    // render
    BeginDrawing();
    ClearBackground(SKYBLUE);
    BeginMode3D(state->camera);
    DrawModel(state->terrain_model, {state->terrain_offset_x, 0.0f, state->terrain_offset_z}, 1.0f, WHITE);
    DrawCube(state->car_pos, 2.0f, 1.0f, 4.0f, RED);
    EndMode3D();

    // game stats
    char pos_text[64];
    std::snprintf(pos_text, sizeof(pos_text), "X: %.2f Y: %.2f Z: %.2f", state->car_pos.x, state->car_pos.y, state->car_pos.z);
    DrawText(pos_text, 10, 30, 30, BLACK);

    // log state
    log_state(state, frame);

    EndDrawing();
}

std::int32_t main() {
    InitWindow(800, 450, "raycer");
    SetTargetFPS(240);

    GameState state{};
    state.texture = load_terrain_texture();

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
