#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"
#include "terrain.hpp"

#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <vector>

namespace {

constexpr float BALL_RADIUS = 0.5f;
constexpr float PHYS_MASS = 1.0f;
constexpr float PHYS_DRAG_AIR = 0.999f;
constexpr float PHYS_DRAG_GROUND = 0.95f;
constexpr Vector3 PHYS_GRAVITY{0.0f, -20.0f, 0.0f};
constexpr float PHYS_MOVE_FORCE = 40.0f;
constexpr float PHYS_JUMP_FORCE = 500.0f;

struct GameState {
    Vector3 ball_pos = {60.0f, 20.0f, 60.0f}; // Start higher to drop
    Vector3 ball_vel = {0.0f, 0.0f, 0.0f};
    Quaternion ball_rot = QuaternionIdentity(); // For visual rolling

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

/** regenerates the terrain mesh from procedural data and uploads it to the GPU */
void generate_terrain_mesh_mut(GameState *state) {
    assert(state != nullptr);

    // clean up previous mesh if it exists
    if (state->terrain_mesh.vertexCount > 0) {
        UnloadMesh(state->terrain_mesh);
    }

    state->terrain_mesh = generate_terrain_mesh_data(state->terrain_offset_x, state->terrain_offset_z);

    // upload mesh to GPU and assign the texture
    UploadMesh(&state->terrain_mesh, false);
    state->terrain_model = LoadModelFromMesh(state->terrain_mesh);
    state->terrain_model.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = state->texture;
    state->mesh_generated = true;
}

/** updates ball position and velocity based on physical forces */
void update_physics_mut(GameState *state, float dt) {
    assert(state != nullptr);
    assert(!std::isnan(dt));
    assert(dt >= 0.0f);
    assert(dt < 1.0f);

    // (2) accumulate forces
    Vector3 total_force = {0.0f, 0.0f, 0.0f};

    // gravity
    total_force = Vector3Add(total_force, Vector3Scale(PHYS_GRAVITY, PHYS_MASS));

    // input forces (relative to camera)
    Vector3 input_dir = {0.0f, 0.0f, 0.0f};

    // get camera forward/right vectors flattened on XZ plane
    Vector3 cam_fwd = Vector3Subtract(state->camera.target, state->camera.position);
    cam_fwd.y = 0.0f;
    cam_fwd = Vector3Normalize(cam_fwd);
    Vector3 cam_right = Vector3CrossProduct(cam_fwd, {0.0f, 1.0f, 0.0f});
    cam_right = Vector3Normalize(cam_right);

    if (IsKeyDown(KEY_W))
        input_dir = Vector3Add(input_dir, cam_fwd);
    if (IsKeyDown(KEY_S))
        input_dir = Vector3Subtract(input_dir, cam_fwd);
    if (IsKeyDown(KEY_A))
        input_dir = Vector3Subtract(input_dir, cam_right);
    if (IsKeyDown(KEY_D))
        input_dir = Vector3Add(input_dir, cam_right);

    if (Vector3Length(input_dir) > 0.1f) {
        input_dir = Vector3Normalize(input_dir);
        total_force = Vector3Add(total_force, Vector3Scale(input_dir, PHYS_MOVE_FORCE));
    }

    // (2) integration (symplectic euler)
    // a = F / m
    Vector3 acc = total_force;

    // v += a * dt
    state->ball_vel = Vector3Add(state->ball_vel, Vector3Scale(acc, dt));

    // (3) collision & constraints
    float terrain_h = get_terrain_height(state->ball_pos.x, state->ball_pos.z);
    bool on_ground = (state->ball_pos.y <= terrain_h + BALL_RADIUS);

    // jump (only when on ground or slightly above)
    if (on_ground && IsKeyPressed(KEY_SPACE)) {
        state->ball_vel.y += PHYS_JUMP_FORCE * dt; // apply instant impulse
        on_ground = false;                         // detach immediately
    }

    if (on_ground) {
        // clamp to surface (simple approach, prevent sinking)
        state->ball_pos.y = terrain_h + BALL_RADIUS;

        // normal force logic: remove velocity component opposing the normal
        Vector3 normal = get_terrain_normal(state->ball_pos.x, state->ball_pos.z);

        // v_normal = dot(v, n) * n
        float v_dot_n = Vector3DotProduct(state->ball_vel, normal);

        // if moving into the terrain, cancel that component (slide)
        if (v_dot_n < 0.0f) {
            state->ball_vel = Vector3Subtract(state->ball_vel, Vector3Scale(normal, v_dot_n));
        }

        // apply ground friction
        state->ball_vel = Vector3Scale(state->ball_vel, PHYS_DRAG_GROUND);
    } else {
        // apply air resistance
        state->ball_vel = Vector3Scale(state->ball_vel, PHYS_DRAG_AIR);
    }

    // (4) update position: p += v * dt
    state->ball_pos = Vector3Add(state->ball_pos, Vector3Scale(state->ball_vel, dt));

    // (5) update visual rotation (rolling)
    float speed = Vector3Length(state->ball_vel);
    if (speed > 0.01f) {
        Vector3 move_dir = Vector3Normalize(state->ball_vel);
        Vector3 rot_axis = Vector3CrossProduct({0.0f, 1.0f, 0.0f}, move_dir);
        rot_axis = Vector3Normalize(rot_axis);

        float angle = -speed * dt / BALL_RADIUS; // Negative to roll 'forward'
        Quaternion q_rot = QuaternionFromAxisAngle(rot_axis, angle);
        state->ball_rot = QuaternionMultiply(q_rot, state->ball_rot);
        state->ball_rot = QuaternionNormalize(state->ball_rot);
    }
}

void game_loop_mut(GameState *state) {
    assert(state != nullptr);

    // lazy init of terrain
    if (!state->mesh_generated) {
        generate_terrain_mesh_mut(state);
    }

    // check if we need to regenerate terrain
    // center of current mesh relative to world
    const float half_size = (GRID_SIZE - 1) * 0.5f; // TILE_SIZE is 1.0f implicitly
    const float center_x = state->terrain_offset_x + half_size;
    const float center_z = state->terrain_offset_z + half_size;
    const float dist_x = std::abs(state->ball_pos.x - center_x);
    const float dist_z = std::abs(state->ball_pos.z - center_z);

    // regenerate if player is more than 30 units away from center (grid is ~120 wide)
    if (dist_x > 30.0f || dist_z > 30.0f) {
        state->terrain_offset_x = std::floor(state->ball_pos.x - half_size);
        state->terrain_offset_z = std::floor(state->ball_pos.z - half_size);
        generate_terrain_mesh_mut(state);
    }

    DrawFPS(10, 10);

    // update physics with clamped delta time
    float dt = GetFrameTime();
    if (dt > 0.05f) {
        dt = 0.05f;
    }

    update_physics_mut(state, dt);

    // update camera to follow ball
    state->camera.target = state->ball_pos;
    state->camera.position = {state->ball_pos.x, state->ball_pos.y + 15.0f, state->ball_pos.z + 15.0f};

    // render scene
    BeginDrawing();

    // gradient sky
    DrawRectangleGradientV(0, 0, GetScreenWidth(), GetScreenHeight(), SKYBLUE, {0, 50, 100, 255});

    BeginMode3D(state->camera);

    DrawModel(state->terrain_model, {state->terrain_offset_x, 0.0f, state->terrain_offset_z}, 1.0f, WHITE);

    // draw shadow blob
    float shadow_y = get_terrain_height(state->ball_pos.x, state->ball_pos.z) + 0.1f;
    DrawCylinder({state->ball_pos.x, shadow_y, state->ball_pos.z}, BALL_RADIUS, BALL_RADIUS, 0.01f, 16, {0, 0, 0, 100});

    // draw ball with rotation
    rlPushMatrix();
    rlTranslatef(state->ball_pos.x, state->ball_pos.y, state->ball_pos.z);

    Vector3 axis;
    float angle;
    QuaternionToAxisAngle(state->ball_rot, &axis, &angle);
    rlRotatef(angle * RAD2DEG, axis.x, axis.y, axis.z);

    DrawSphere({0, 0, 0}, BALL_RADIUS, RED);
    DrawSphereWires({0, 0, 0}, BALL_RADIUS, 16, 16, MAROON); // wires help visualize rotation

    rlPopMatrix();

    EndMode3D();
    EndDrawing();
}

} // namespace

std::int32_t main() {
    InitWindow(800, 450, "raycer");
    SetTargetFPS(60);

    GameState state{};

    // checkerboard texture for the terrain
    Image checked = GenImageChecked(256, 256, 128, 128, DARKGREEN, GREEN);
    Texture2D texture = LoadTextureFromImage(checked);
    assert(texture.id != 0);
    UnloadImage(checked);
    state.texture = texture;

    while (!WindowShouldClose()) {
        game_loop_mut(&state);
    }

    // cleanup
    UnloadModel(state.terrain_model);
    UnloadTexture(state.texture);
    CloseWindow();

    return EXIT_SUCCESS;
}
