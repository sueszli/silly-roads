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

//
// physics
//

constexpr float BALL_RADIUS = 0.5f;
constexpr Vector3 PHYS_GRAVITY{0.0f, -500.0f, 0.0f}; // gravitational force
constexpr float PHYS_MOVE_FORCE = 250.0f;            // directional speed by player
constexpr float PHYS_DRAG = 0.995f;                  // drag coefficient

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
    if (Vector3Length(input_dir) > 0.1f) {
        input_dir = Vector3Normalize(input_dir);
        state->ball_vel = Vector3Add(state->ball_vel, Vector3Scale(input_dir, PHYS_MOVE_FORCE * dt));
    }

    // gravity softening on input
    bool has_input = IsKeyDown(KEY_W) || IsKeyDown(KEY_S) || IsKeyDown(KEY_A) || IsKeyDown(KEY_D);
    Vector3 effective_gravity = has_input ? Vector3Scale(PHYS_GRAVITY, 0.05f) : PHYS_GRAVITY;

    float terrain_h = get_terrain_height(state->ball_pos.x, state->ball_pos.z);
    bool is_on_ground = (state->ball_pos.y <= terrain_h + BALL_RADIUS);

    if (!is_on_ground) {
        // apply full gravity
        state->ball_vel = Vector3Add(state->ball_vel, Vector3Scale(effective_gravity, dt));
    }
    if (is_on_ground) {
        // apply gravity via ground normal
        Vector3 terrain_normal = get_terrain_normal(state->ball_pos.x, state->ball_pos.z);
        float dot = Vector3DotProduct(effective_gravity, terrain_normal);
        Vector3 gravity_parallel = Vector3Subtract(effective_gravity, Vector3Scale(terrain_normal, dot));
        state->ball_vel = Vector3Add(state->ball_vel, Vector3Scale(gravity_parallel, dt));

        // snap ball to ground, but also allow natural launch
        if (state->ball_vel.y <= 0.0f) {
            state->ball_pos.y = terrain_h + BALL_RADIUS;
            state->ball_vel.y = 0.0f;
        }
    }

    // update position
    state->ball_pos = Vector3Add(state->ball_pos, Vector3Scale(state->ball_vel, dt));

    // apply drag
    state->ball_vel = Vector3Scale(state->ball_vel, PHYS_DRAG);
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
// game loop
//

void game_loop_mut(GameState *state) {
    assert(state != nullptr);

    // lazy init and regenerate terrain
    if (!state->mesh_generated) {
        generate_terrain_mesh_mut(state);
    }
    const float half_size = (GRID_SIZE - 1) * 0.5f;
    const float center_x = state->terrain_offset_x + half_size;
    const float center_z = state->terrain_offset_z + half_size;
    const float dist_x = std::abs(state->ball_pos.x - center_x);
    const float dist_z = std::abs(state->ball_pos.z - center_z);
    if (dist_x > 30.0f || dist_z > 30.0f) {
        state->terrain_offset_x = std::floor(state->ball_pos.x - half_size);
        state->terrain_offset_z = std::floor(state->ball_pos.z - half_size);
        generate_terrain_mesh_mut(state);
    }

    // update physics
    update_physics_mut(state);

    // update camera to follow ball
    state->camera.target = state->ball_pos;
    state->camera.position = {state->ball_pos.x, state->ball_pos.y + 15.0f, state->ball_pos.z + 15.0f};

    // render
    BeginDrawing();
    ClearBackground(SKYBLUE);
    BeginMode3D(state->camera);
    DrawModel(state->terrain_model, {state->terrain_offset_x, 0.0f, state->terrain_offset_z}, 1.0f, WHITE);
    DrawSphere(state->ball_pos, BALL_RADIUS, RED);
    EndMode3D();

    // game stats
    char pos_text[64];
    std::snprintf(pos_text, sizeof(pos_text), "X: %.2f Y: %.2f Z: %.2f", state->ball_pos.x, state->ball_pos.y, state->ball_pos.z);
    DrawText(pos_text, 10, 30, 30, BLACK);
    std::printf("%s\n", pos_text);

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
