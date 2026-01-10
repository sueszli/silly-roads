#include "game_state.hpp"
#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"

#include "terrain.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

//
// forward declarations
//

void update_road_mut(GameState *state);

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
    // plain white texture, so vertex colors are unfiltered
    Image white = GenImageColor(2, 2, WHITE);
    Texture2D texture = LoadTextureFromImage(white);
    assert(texture.id != 0);
    UnloadImage(white);
    return texture;
}

void update_terrain_mut(GameState *state) {
    assert(state != nullptr);

    float chunk_world_size = (GRID_SIZE - 1) * TILE_SIZE;
    state->chunk_size = chunk_world_size;

    int center_cx = (int)std::floor(state->car_pos.x / chunk_world_size);
    int center_cz = (int)std::floor(state->car_pos.z / chunk_world_size);

    // 5x5 Grid (radius 2)
    int render_radius = 2; // -2 to +2

    // 1. Identify valid chunks
    std::vector<GameState::TerrainChunk> new_chunks;
    new_chunks.reserve(state->terrain_chunks.size());

    // Keep existing chunks that are in range
    for (const auto &chunk : state->terrain_chunks) {
        if (std::abs(chunk.cx - center_cx) <= render_radius && std::abs(chunk.cz - center_cz) <= render_radius) {
            new_chunks.push_back(chunk);
        } else {
            // Unload
            UnloadModel(chunk.model);
        }
    }
    state->terrain_chunks = std::move(new_chunks);

    // 2. Identify missing chunks and load them
    for (int z = -render_radius; z <= render_radius; z++) {
        for (int x = -render_radius; x <= render_radius; x++) {
            int target_cx = center_cx + x;
            int target_cz = center_cz + z;

            bool exists = std::any_of(state->terrain_chunks.begin(), state->terrain_chunks.end(), [target_cx, target_cz](const GameState::TerrainChunk &chunk) { return chunk.cx == target_cx && chunk.cz == target_cz; });

            if (!exists) {
                // Generate chunk
                float ox = (float)target_cx * chunk_world_size;
                float oz = (float)target_cz * chunk_world_size;

                Mesh mesh = generate_terrain_mesh_data(ox, oz, state->dense_road_points);
                UploadMesh(&mesh, false);

                Model model = LoadModelFromMesh(mesh);
                model.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = state->texture;

                state->terrain_chunks.push_back({target_cx, target_cz, model});
            }
        }
    }
}

//
// logging
//

void log_state(const GameState *state, std::int32_t /*frame*/) {
    assert(state != nullptr);
    std::printf("CAR_POS:%.2f %.2f %.2f | SPEED:%.2f | HEADING:%.2f | PITCH:%.2f | ROLL:%.2f | WHEEL_STEER:%.2f\n", state->car_pos.x, state->car_pos.y, state->car_pos.z, state->car_speed, state->car_heading, state->car_pitch, state->car_roll, state->wheels[0].steering_angle);
}

//
// game loop
//

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
    state->camera.target = state->car_pos;
}

void draw_frame(const GameState *state) {
    assert(state != nullptr);
    BeginDrawing();
    ClearBackground(SKYBLUE);

    BeginMode3D(state->camera);

    // Draw all chunks
    for (const auto &chunk : state->terrain_chunks) {
        float chunk_world_size = (GRID_SIZE - 1) * TILE_SIZE;
        float ox = (float)chunk.cx * chunk_world_size;
        float oz = (float)chunk.cz * chunk_world_size;
        Vector3 pos = {ox, 0.0f, oz};

        DrawModel(chunk.model, pos, 1.0f, WHITE);
    }

    //
    // car rendering
    //

    rlPushMatrix();
    rlTranslatef(state->car_pos.x, state->car_pos.y, state->car_pos.z);

    // car body
    rlRotatef(state->car_heading * RAD2DEG, 0.0f, 1.0f, 0.0f);
    rlRotatef(state->car_pitch * RAD2DEG, 1.0f, 0.0f, 0.0f);
    rlRotatef(state->car_roll * RAD2DEG, 0.0f, 0.0f, 1.0f);

    // simple box car
    DrawCube({0.0f, 0.5f, 0.0f}, 2.0f, 1.0f, 4.0f, RED);
    DrawCube({0.0f, 0.5f, 1.0f}, 1.8f, 0.8f, 2.0f, MAROON); // cabin

    // wheels
    for (int i = 0; i < 4; i++) {
        rlPushMatrix();
        rlTranslatef(state->wheels[i].local_offset.x, state->wheels[i].local_offset.y, state->wheels[i].local_offset.z);

        // steering
        if (i < 2) {
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
    std::snprintf(pos_text, sizeof(pos_text), "X: %.2f Y: %.2f Z: %.2f | Chunks: %zu", state->car_pos.x, state->car_pos.y, state->car_pos.z, state->terrain_chunks.size());

    // draw speed on screen
    char speed_text[64];
    std::snprintf(speed_text, sizeof(speed_text), "SPEED: %.2f", state->car_speed);
    DrawText(speed_text, 10, 40, 20, WHITE);
    DrawText(pos_text, 10, 60, 20, LIGHTGRAY); // Show Stats

    // log state
    log_state(state, state->frame_count);

    EndDrawing();
}

void game_loop_mut(GameState *state) {
    assert(state != nullptr);
    update_road_mut(state); // Update road first to have points for terrain
    update_terrain_mut(state);
    update_physics_mut(state);
    update_camera_mut(state);
    draw_frame(state);
}

//
// road
//

Vector3 generate_next_road_point_mut(GameState *state) {
    assert(state != nullptr);
    const float step_size = 20.0f;

    // use stored state for continuous generation
    float x = state->road_gen_x;
    float z = state->road_gen_z;
    float angle = state->road_gen_angle;
    std::int32_t segment_index = state->road_gen_next_segment;

    // advance position
    if (segment_index > 0) {
        x += std::cos(angle) * step_size;
        z += std::sin(angle) * step_size;
    }

    // deterministic "noise" for winding path
    float noise = std::sin((float)segment_index * 0.3f) * 0.5f + std::cos((float)segment_index * 0.17f) * 0.3f;
    angle += noise * 0.2f;

    // update state for next call
    state->road_gen_x = x;
    state->road_gen_z = z;
    state->road_gen_angle = angle;
    state->road_gen_next_segment = segment_index + 1;

    float y = get_terrain_height(x, z);
    return {x, y, z};
}

void update_road_mut(GameState *state) {
    assert(state != nullptr);
    if (!state->road_initialized) {
        return;
    }

    // find closest road point to car
    int closest_idx = 0;
    float min_dist_sq = 1e9f;
    for (size_t i = 0; i < state->road_points.size(); i++) {
        Vector3 to_car = Vector3Subtract(state->car_pos, state->road_points[i]);
        float dist_sq = to_car.x * to_car.x + to_car.z * to_car.z; // ignore Y
        if (dist_sq < min_dist_sq) {
            min_dist_sq = dist_sq;
            closest_idx = (int)i;
        }
    }

    // generate new points if we're near the end
    int points_ahead = (int)state->road_points.size() - closest_idx;
    const int min_points_ahead = 64;

    if (points_ahead < min_points_ahead) {
        int to_generate = min_points_ahead - points_ahead;
        for (int i = 0; i < to_generate; i++) {
            state->road_points.push_back(generate_next_road_point_mut(state));
        }

        // Prune old points to prevent infinite growth and lag
        // Keep some points behind for spline continuity and rear visibility
        const int keep_behind = 32;
        if (closest_idx > keep_behind + 16) {
            int prune_count = closest_idx - keep_behind;
            state->road_points.erase(state->road_points.begin(), state->road_points.begin() + prune_count);
        }

        // regenerate dense points
        state->dense_road_points = generate_road_path(state->road_points);

        // Chunks intersecting the new road might need update,
        // but for now, we assume simple generation is enough as road is static in old chunks.
    }
}

void init_road_mut(GameState *state) {
    assert(state != nullptr);
    if (state->road_initialized) {
        return;
    }

    // initialize generation state
    state->road_gen_x = 0.0f;
    state->road_gen_z = 0.0f;
    state->road_gen_angle = 0.0f;
    state->road_gen_next_segment = 0;

    // generate initial road ahead
    state->road_points.clear();
    state->road_points.reserve(128); // reserve space to avoid reallocations

    for (int i = 0; i < 128; i++) {
        state->road_points.push_back(generate_next_road_point_mut(state));
    }

    state->road_initialized = true;
    state->dense_road_points = generate_road_path(state->road_points);
    std::printf("[ROAD] Generated initial %zu points\n", state->road_points.size());
}

std::int32_t main() {
    InitWindow(800, 450, "raycer");
    SetTargetFPS(240);

    GameState state{};
    state.texture = load_terrain_texture();
    init_road_mut(&state);

    // Initial car placement on road
    if (!state.road_points.empty()) {
        Vector3 start = state.road_points[0];
        Vector3 next = state.road_points[1];
        state.car_pos = start;
        state.car_pos.y = get_terrain_height(start.x, start.z) + 2.0f; // Initial drop height

        // Face the road direction
        float dx = next.x - start.x;
        float dz = next.z - start.z;
        state.car_heading = std::atan2(dx, dz);
    }

    // Force initial terrain update for chunks
    update_terrain_mut(&state);

    // game loop
    while (!WindowShouldClose()) {
        game_loop_mut(&state);
    }

    // cleanup
    for (const auto &chunk : state.terrain_chunks) {
        UnloadModel(chunk.model);
    }

    UnloadTexture(state.texture);
    CloseWindow();

    return EXIT_SUCCESS;
}
