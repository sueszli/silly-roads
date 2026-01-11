#include "game_state.hpp"
#include "raylib.h"

#include "camera.hpp"
#include "physics.hpp"
#include "raymath.h"
#include "renderer.hpp"
#include "terrain.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <vector>

Components::CarControls read_inputs() {
    Components::CarControls inputs = {};

    // A/D steering
    if (IsKeyDown(KEY_D)) {
        inputs.steer = 1.0f;
    } else if (IsKeyDown(KEY_A)) {
        inputs.steer = -1.0f;
    }

    // W/S throttle
    if (IsKeyDown(KEY_W)) {
        inputs.throttle = 1.0f;
    } else if (IsKeyDown(KEY_S)) {
        inputs.throttle = -1.0f;
    }

    return inputs;
}

std::int32_t main() {
    InitWindow(800, 450, "raycer");
    SetTargetFPS(240);

    GameState state{};
    state.texture = Terrain::load_texture();
    Terrain::init_road(state.road);

    // Initial car placement on road
    if (!state.road.points.empty()) {
        Vector3 start = state.road.points[0];
        Vector3 next = state.road.points[1];
        state.car.pos = start;
        state.car.pos.y = Terrain::get_height(start.x, start.z) + 2.0f; // Initial drop height

        // Face the road direction
        float dx = next.x - start.x;
        float dz = next.z - start.z;
        state.car.heading = std::atan2(dx, dz);
    }

    // Force initial terrain update for chunks
    Terrain::update(&state);

    // game loop
    while (!WindowShouldClose()) {
        float dt = GetFrameTime();
        dt = std::min(dt, 0.1f);

        // Input
        Components::CarControls controls = read_inputs();

        // Update
        Terrain::update_road(state.road, state.car.pos);
        Terrain::update(&state);
        Physics::update_car_physics(state.car, controls, dt);
        CameraSystem::update(state.camera, state.car, dt);

        // Draw
        Renderer::draw_scene(state);
    }

    // cleanup
    for (const auto &chunk : state.terrain_chunks) {
        UnloadModel(chunk.model);
    }

    UnloadTexture(state.texture);
    CloseWindow();

    return EXIT_SUCCESS;
}
