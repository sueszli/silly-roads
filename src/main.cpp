#include "raylib.h"
#include "rlgl.h"
#include "terrain.hpp"

#include <cassert>
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

GameState STATE = GameState{
    .ball_pos = {60.0f, 10.0f, 60.0f},
    .ball_vel = {0.0f, 0.0f, 0.0f},
    .camera =
        {
            .position = {0.0f, 10.0f, 10.0f},
            .target = {0.0f, 0.0f, 0.0f},
            .up = {0.0f, 1.0f, 0.0f},
            .fovy = 45.0f,
            .projection = CAMERA_PERSPECTIVE,
        },
    .terrain_mesh = {},
    .terrain_model = {},
    .texture = {},
    .mesh_generated = false,
};

void generate_terrain_mesh() {
    if (STATE.terrain_mesh.vertexCount > 0) {
        UnloadMesh(STATE.terrain_mesh);
    }

    STATE.terrain_mesh = generate_terrain_mesh_data();

    UploadMesh(&STATE.terrain_mesh, false);
    STATE.terrain_model = LoadModelFromMesh(STATE.terrain_mesh);
    STATE.terrain_model.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = STATE.texture;
    STATE.mesh_generated = true;
}

void update_physics(float dt) {
    assert(!isnan(dt));
    assert(dt >= 0.0f);
    assert(dt < 1.0f);
    constexpr Vector3 gravity{0.0f, -20.0f, 0.0f};

    STATE.ball_vel.x += gravity.x * dt;
    STATE.ball_vel.y += gravity.y * dt;
    STATE.ball_vel.z += gravity.z * dt;

    STATE.ball_pos.x += STATE.ball_vel.x * dt;
    STATE.ball_pos.y += STATE.ball_vel.y * dt;
    STATE.ball_pos.z += STATE.ball_vel.z * dt;

    if (STATE.ball_pos.y < -50.0f) {
        STATE.ball_pos = {60.0f, 10.0f, 60.0f};
        STATE.ball_vel = {0.0f, 0.0f, 0.0f};
    }

    const float terrain_h = get_terrain_height(STATE.ball_pos.x, STATE.ball_pos.z);

    if (STATE.ball_pos.y <= terrain_h + BALL_RADIUS) {
        STATE.ball_pos.y = terrain_h + BALL_RADIUS;
        const Vector3 normal = get_terrain_normal(STATE.ball_pos.x, STATE.ball_pos.z);
        const float dot = STATE.ball_vel.x * normal.x + STATE.ball_vel.y * normal.y + STATE.ball_vel.z * normal.z;

        STATE.ball_vel.x -= dot * normal.x;
        STATE.ball_vel.y -= dot * normal.y;
        STATE.ball_vel.z -= dot * normal.z;

        STATE.ball_vel.x *= 0.99f;
        STATE.ball_vel.z *= 0.99f;
    }
}

void game_loop() {
    if (!STATE.mesh_generated) {
        generate_terrain_mesh();
    }

    DrawFPS(10, 10);
    float dt = GetFrameTime();
    if (dt > 0.05f) {
        dt = 0.05f;
    }

    update_physics(dt);

    STATE.camera.target = STATE.ball_pos;
    STATE.camera.position = {STATE.ball_pos.x, STATE.ball_pos.y + 15.0f, STATE.ball_pos.z + 15.0f};

    BeginDrawing();
    ClearBackground(SKYBLUE);
    BeginMode3D(STATE.camera);

    DrawModel(STATE.terrain_model, {0.0f, 0.0f, 0.0f}, 1.0f, WHITE);
    DrawGrid(GRID_SIZE, 10.0f);
    DrawSphere(STATE.ball_pos, BALL_RADIUS, RED);
    DrawSphereWires(STATE.ball_pos, BALL_RADIUS, 16, 16, MAROON);

    EndMode3D();
    EndDrawing();
}

} // namespace

std::int32_t main() {
    InitWindow(800, 450, "raycer");
    SetTargetFPS(60);

    Image checked = GenImageChecked(256, 256, 128, 128, DARKGRAY, WHITE);
    Texture2D texture = LoadTextureFromImage(checked);
    assert(texture.id != 0);
    UnloadImage(checked);
    STATE.texture = texture;

    while (!WindowShouldClose()) {
        game_loop();
    }

    UnloadModel(STATE.terrain_model);
    UnloadTexture(STATE.texture);
    CloseWindow();

    return EXIT_SUCCESS;
}
