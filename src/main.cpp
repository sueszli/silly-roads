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

constexpr float BALL_RADIUS = 0.5f;
constexpr Vector3 PHYS_GRAVITY{0.0f, -1000.0f, 0.0f}; // gravitational force
constexpr float PHYS_MOVE_FORCE = 2000.0f;            // directional speed by player
constexpr float PHYS_DRAG = 0.995f;                   // drag coefficient

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

void generate_terrain_mesh_mut(GameState *state) {
    assert(state != nullptr);

    // clean up previous model (including mesh and materials) if it exists
    if (state->mesh_generated) {
        UnloadModel(state->terrain_model);
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

    // handle ground collision first to get terrain normal
    float terrain_h = get_terrain_height(state->ball_pos.x, state->ball_pos.z);
    bool on_ground = (state->ball_pos.y <= terrain_h + BALL_RADIUS);

    // apply gravity (decomposed based on terrain slope if on ground)
    if (on_ground) {
        // get terrain normal and decompose gravity into parallel and perpendicular components
        Vector3 terrain_normal = get_terrain_normal(state->ball_pos.x, state->ball_pos.z);

        // project gravity onto the terrain surface (parallel component)
        float dot = Vector3DotProduct(PHYS_GRAVITY, terrain_normal);
        Vector3 gravity_parallel = Vector3Subtract(PHYS_GRAVITY, Vector3Scale(terrain_normal, dot));

        // apply only the parallel component to make ball roll down slopes
        state->ball_vel = Vector3Add(state->ball_vel, Vector3Scale(gravity_parallel, dt));
    } else {
        // in air: apply full gravity
        state->ball_vel = Vector3Add(state->ball_vel, Vector3Scale(PHYS_GRAVITY, dt));
    }

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

    // when on ground and moving uphill, project velocity onto terrain surface
    // this makes the ball gain upward velocity when going over slopes/peaks
    // but allows natural falling in valleys
    if (on_ground) {
        Vector3 terrain_normal = get_terrain_normal(state->ball_pos.x, state->ball_pos.z);

        // get horizontal velocity magnitude
        Vector3 vel_horizontal = state->ball_vel;
        vel_horizontal.y = 0.0f;
        float horizontal_speed = Vector3Length(vel_horizontal);

        // if moving horizontally, check if we're going uphill
        if (horizontal_speed > 0.1f) {
            Vector3 horizontal_dir = Vector3Normalize(vel_horizontal);

            // project horizontal direction onto terrain surface
            float dot = Vector3DotProduct(horizontal_dir, terrain_normal);
            Vector3 surface_dir = Vector3Subtract(horizontal_dir, Vector3Scale(terrain_normal, dot));

            // only redirect if we have a valid surface direction AND we're going uphill
            // check if surface direction points upward (positive y component)
            if (Vector3Length(surface_dir) > 0.01f) {
                surface_dir = Vector3Normalize(surface_dir);

                // only apply if the surface direction has an upward component
                // this prevents the ball from sticking to valleys
                if (surface_dir.y > 0.0f) {
                    // apply horizontal speed along the terrain surface
                    state->ball_vel = Vector3Scale(surface_dir, horizontal_speed);
                }
            }
        }
    }

    // update position from velocity
    state->ball_pos = Vector3Add(state->ball_pos, Vector3Scale(state->ball_vel, dt));

    // recheck ground collision after movement
    terrain_h = get_terrain_height(state->ball_pos.x, state->ball_pos.z);
    on_ground = (state->ball_pos.y <= terrain_h + BALL_RADIUS);

    // only apply ground collision if ball is moving into the ground
    // this allows the ball to launch off hills naturally
    if (on_ground && state->ball_vel.y <= 0.0f) {
        state->ball_pos.y = terrain_h + BALL_RADIUS;
        state->ball_vel.y = 0.0f;
    }

    // apply drag
    state->ball_vel = Vector3Scale(state->ball_vel, PHYS_DRAG);
}

void game_loop_mut(GameState *state) {
    assert(state != nullptr);
    // lazy init of terrain
    if (!state->mesh_generated) {
        generate_terrain_mesh_mut(state);
    }

    // regenerate terrain
    // if player is more than 30 units away from center (grid is ~120 wide)
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
    float dt = GetFrameTime();
    if (dt > 0.05f) {
        dt = 0.05f;
    }
    update_physics_mut(state, dt);

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
    DrawText(pos_text, 10, 30, 20, BLACK);
    std::printf("%s\n", pos_text);

    EndDrawing();
}

Texture2D load_texture() {
    Image checked = GenImageChecked(256, 256, 128, 128, DARKGREEN, GREEN);
    Texture2D texture = LoadTextureFromImage(checked);
    assert(texture.id != 0);
    UnloadImage(checked);
    return texture;
}

std::int32_t main() {
    InitWindow(800, 450, "raycer");
    SetTargetFPS(240);

    GameState state{};
    state.texture = load_texture();

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
