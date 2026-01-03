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
    Vector3 ball_pos = {60.0f, 10.0f, 60.0f};
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
GameState STATE{};

/** regenerates the terrain mesh from procedural data and uploads it to the GPU */
void generate_terrain_mesh() {
    // clean up previous mesh if it exists
    if (STATE.terrain_mesh.vertexCount > 0) {
        UnloadMesh(STATE.terrain_mesh);
    }

    STATE.terrain_mesh = generate_terrain_mesh_data(STATE.terrain_offset_x, STATE.terrain_offset_z);

    // upload mesh to GPU and assign the texture
    UploadMesh(&STATE.terrain_mesh, false);
    STATE.terrain_model = LoadModelFromMesh(STATE.terrain_mesh);
    STATE.terrain_model.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = STATE.texture;
    STATE.mesh_generated = true;
}

/** updates ball position and velocity based on gravity and terrain collisions */
void update_physics(float dt) {
    assert(!isnan(dt));
    assert(dt >= 0.0f);
    assert(dt < 1.0f);
    constexpr Vector3 gravity{0.0f, -20.0f, 0.0f};
    constexpr float acceleration = 30.0f;

    const float terrain_h = get_terrain_height(STATE.ball_pos.x, STATE.ball_pos.z);
    // flight controls - always allow movement
    if (IsKeyDown(KEY_W)) {
        STATE.ball_vel.z -= acceleration * dt;
    }
    if (IsKeyDown(KEY_S)) {
        STATE.ball_vel.z += acceleration * dt;
    }
    if (IsKeyDown(KEY_A)) {
        STATE.ball_vel.x -= acceleration * dt;
    }
    if (IsKeyDown(KEY_D)) {
        STATE.ball_vel.x += acceleration * dt;
    }
    if (IsKeyDown(KEY_SPACE)) {
        STATE.ball_vel.y += acceleration * dt;
    }

    // apply gravity
    STATE.ball_vel.x += gravity.x * dt;
    STATE.ball_vel.y += gravity.y * dt;
    STATE.ball_vel.z += gravity.z * dt;

    // integrate position
    STATE.ball_pos.x += STATE.ball_vel.x * dt;
    STATE.ball_pos.y += STATE.ball_vel.y * dt;
    STATE.ball_pos.z += STATE.ball_vel.z * dt;

    // collision with terrain
    if (STATE.ball_pos.y <= terrain_h + BALL_RADIUS) {
        // clamp to surface
        STATE.ball_pos.y = terrain_h + BALL_RADIUS;

        // velocity vector based on terrain normal
        const Vector3 normal = get_terrain_normal(STATE.ball_pos.x, STATE.ball_pos.z);
        const float dot = STATE.ball_vel.x * normal.x + STATE.ball_vel.y * normal.y + STATE.ball_vel.z * normal.z;

        STATE.ball_vel.x -= dot * normal.x;
        STATE.ball_vel.y -= dot * normal.y;
        STATE.ball_vel.z -= dot * normal.z;

        // friction/damping
        STATE.ball_vel.x *= 0.99f;
        STATE.ball_vel.z *= 0.99f;
    }
}

void game_loop() {
    // lazy init of terrain
    if (!STATE.mesh_generated) {
        generate_terrain_mesh();
    }

    // check if we need to regenerate terrain
    // center of current mesh relative to world
    const float half_size = (GRID_SIZE - 1) * 0.5f; // TILE_SIZE is 1.0f implicitly
    const float center_x = STATE.terrain_offset_x + half_size;
    const float center_z = STATE.terrain_offset_z + half_size;
    const float dist_x = std::abs(STATE.ball_pos.x - center_x);
    const float dist_z = std::abs(STATE.ball_pos.z - center_z);

    // regenerate if player is more than 30 units away from center (grid is ~120 wide)
    if (dist_x > 30.0f || dist_z > 30.0f) {
        STATE.terrain_offset_x = std::floor(STATE.ball_pos.x - half_size);
        STATE.terrain_offset_z = std::floor(STATE.ball_pos.z - half_size);
        generate_terrain_mesh();
    }

    DrawFPS(10, 10);

    // update physics with clamped delta time
    float dt = GetFrameTime();
    if (dt > 0.05f) {
        dt = 0.05f;
    }

    update_physics(dt);

    // update camera to follow ball
    STATE.camera.target = STATE.ball_pos;
    STATE.camera.position = {STATE.ball_pos.x, STATE.ball_pos.y + 15.0f, STATE.ball_pos.z + 15.0f};

    // render scene
    BeginDrawing();
    ClearBackground(SKYBLUE);
    BeginMode3D(STATE.camera);

    DrawModel(STATE.terrain_model, {STATE.terrain_offset_x, 0.0f, STATE.terrain_offset_z}, 1.0f, WHITE);
    DrawSphere(STATE.ball_pos, BALL_RADIUS, RED);
    DrawSphereWires(STATE.ball_pos, BALL_RADIUS, 16, 16, MAROON);

    EndMode3D();
    EndDrawing();
}

} // namespace

std::int32_t main() {
    InitWindow(800, 450, "raycer");
    SetTargetFPS(60);

    // checkerboard texture for the terrain
    Image checked = GenImageChecked(256, 256, 128, 128, DARKGRAY, WHITE);
    Texture2D texture = LoadTextureFromImage(checked);
    assert(texture.id != 0);
    UnloadImage(checked);
    STATE.texture = texture;

    while (!WindowShouldClose()) {
        game_loop();
    }

    // cleanup
    UnloadModel(STATE.terrain_model);
    UnloadTexture(STATE.texture);
    CloseWindow();

    return EXIT_SUCCESS;
}
