#include "renderer.hpp"

#include "game_state.hpp"
#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"
#include "terrain.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace Renderer {

namespace {

void draw_car(const Components::Car &car) {
    rlPushMatrix();
    rlTranslatef(car.pos.x, car.pos.y, car.pos.z);

    // car body
    rlRotatef(car.heading * RAD2DEG, 0.0f, 1.0f, 0.0f);
    rlRotatef(car.pitch * RAD2DEG, 1.0f, 0.0f, 0.0f);
    rlRotatef(car.roll * RAD2DEG, 0.0f, 0.0f, 1.0f);

    // simple box car
    DrawCube({0.0f, 0.5f, 0.0f}, 2.0f, 1.0f, 4.0f, RED);
    DrawCube({0.0f, 0.5f, 1.0f}, 1.8f, 0.8f, 2.0f, MAROON); // cabin

    // wheels
    for (int i = 0; i < 4; i++) {
        rlPushMatrix();
        rlTranslatef(car.wheels[i].local_offset.x, car.wheels[i].local_offset.y, car.wheels[i].local_offset.z);

        // steering
        if (i < 2) {
            rlRotatef(car.wheels[i].steering_angle * RAD2DEG, 0.0f, 1.0f, 0.0f);
        }

        Vector3 wheel_start = {-0.1f, 0.0f, 0.0f};
        Vector3 wheel_end = {0.1f, 0.0f, 0.0f};
        DrawCylinderEx(wheel_start, wheel_end, 0.3f, 0.3f, 16, DARKGRAY);
        rlPopMatrix();
    }

    rlPopMatrix();
}

} // namespace

void draw_scene(const GameState &state) {
    BeginDrawing();
    ClearBackground(SKYBLUE);

    BeginMode3D(state.camera.camera);

    // Draw all chunks
    for (const auto &chunk : state.terrain_chunks) {
        float chunk_world_size = (Terrain::GRID_SIZE - 1) * Terrain::TILE_SIZE;
        float ox = (float)chunk.cx * chunk_world_size;
        float oz = (float)chunk.cz * chunk_world_size;
        Vector3 pos = {ox, 0.0f, oz};

        DrawModel(chunk.model, pos, 1.0f, WHITE);
    }

    // car rendering
    draw_car(state.car);

    EndMode3D();

    // game stats
    char pos_text[64];
    std::snprintf(pos_text, sizeof(pos_text), "X: %.2f Y: %.2f Z: %.2f | Chunks: %zu", state.car.pos.x, state.car.pos.y, state.car.pos.z, state.terrain_chunks.size());

    // draw speed on screen
    char speed_text[64];
    std::snprintf(speed_text, sizeof(speed_text), "SPEED: %.2f", state.car.speed);
    DrawText(speed_text, 10, 40, 20, WHITE);
    DrawText(pos_text, 10, 60, 20, LIGHTGRAY); // Show Stats

    EndDrawing();
}

} // namespace Renderer
