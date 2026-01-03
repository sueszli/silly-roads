#include "raylib.h"
#include "rlgl.h"
#include "terrain.hpp"

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <vector>

namespace {

constexpr float BALL_RADIUS = 0.5f;

struct GameState {
    Vector3 ball_pos;
    Vector3 ball_vel;
    Camera3D camera;
    Mesh terrain_mesh;
    Model terrain_model;
    Texture2D texture;
    bool mesh_generated;
};

void generate_terrain_mesh(GameState &state) {
    if (state.terrain_mesh.vertexCount > 0) {
        UnloadMesh(state.terrain_mesh);
    }

    state.terrain_mesh = generate_terrain_mesh_data();

    UploadMesh(&state.terrain_mesh, false);
    state.terrain_model = LoadModelFromMesh(state.terrain_mesh);
    state.terrain_model.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = state.texture;
    state.mesh_generated = true;
}

void update_physics(GameState &state, float dt) {
    constexpr Vector3 gravity{0.0f, -20.0f, 0.0f};

    state.ball_vel.x += gravity.x * dt;
    state.ball_vel.y += gravity.y * dt;
    state.ball_vel.z += gravity.z * dt;

    state.ball_pos.x += state.ball_vel.x * dt;
    state.ball_pos.y += state.ball_vel.y * dt;
    state.ball_pos.z += state.ball_vel.z * dt;

    if (state.ball_pos.y < -50.0f) {
        state.ball_pos = {60.0f, 10.0f, 60.0f};
        state.ball_vel = {0.0f, 0.0f, 0.0f};
    }

    const float terrain_h = get_terrain_height(state.ball_pos.x, state.ball_pos.z);

    if (state.ball_pos.y <= terrain_h + BALL_RADIUS) {
        state.ball_pos.y = terrain_h + BALL_RADIUS;
        const Vector3 normal = get_terrain_normal(state.ball_pos.x, state.ball_pos.z);
        const float dot = state.ball_vel.x * normal.x + state.ball_vel.y * normal.y + state.ball_vel.z * normal.z;

        state.ball_vel.x -= dot * normal.x;
        state.ball_vel.y -= dot * normal.y;
        state.ball_vel.z -= dot * normal.z;

        state.ball_vel.x *= 0.99f;
        state.ball_vel.z *= 0.99f;
    }
}

void game_loop(GameState &state) {
    if (!state.mesh_generated) {
        generate_terrain_mesh(state);
    }

    DrawFPS(10, 10);
    float dt = GetFrameTime();
    if (dt > 0.05f)
        dt = 0.05f;

    update_physics(state, dt);

    state.camera.target = state.ball_pos;
    state.camera.position = {state.ball_pos.x, state.ball_pos.y + 15.0f, state.ball_pos.z + 15.0f};

    BeginDrawing();
    ClearBackground(SKYBLUE);
    BeginMode3D(state.camera);

    DrawModel(state.terrain_model, {0.0f, 0.0f, 0.0f}, 1.0f, WHITE);
    DrawGrid(GRID_SIZE, 10.0f);
    DrawSphere(state.ball_pos, BALL_RADIUS, RED);
    DrawSphereWires(state.ball_pos, BALL_RADIUS, 16, 16, MAROON);

    EndMode3D();
    EndDrawing();
}

} // namespace

std::int32_t main() {
    InitWindow(800, 450, "raycer");
    SetTargetFPS(60);

    Image checked = GenImageChecked(256, 256, 128, 128, DARKGRAY, WHITE);
    Texture2D texture = LoadTextureFromImage(checked);
    UnloadImage(checked);

    GameState state{
        .ball_pos = {60.0f, 10.0f, 60.0f},
        .ball_vel = {0.0f, 0.0f, 0.0f},
        .camera = {.position = {0.0f, 10.0f, 10.0f}, .target = {0.0f, 0.0f, 0.0f}, .up = {0.0f, 1.0f, 0.0f}, .fovy = 45.0f, .projection = CAMERA_PERSPECTIVE},
        .terrain_mesh = {},
        .terrain_model = {},
        .texture = texture,
        .texture = texture,
        .mesh_generated = false,
    };

    while (!WindowShouldClose()) {
        game_loop(state);
    }

    UnloadModel(state.terrain_model);
    UnloadTexture(state.texture);
    CloseWindow();

    return EXIT_SUCCESS;
}
