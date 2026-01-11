#include "camera.hpp"
#include "car.hpp"
#include "raylib.h"
#include "terrain.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

void draw_hud() {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "SPEED: %.2f", Car::get_speed());
    DrawText(buf, 10, 40, 20, WHITE);
    Vector3 pos = Car::get_position();
    std::snprintf(buf, sizeof(buf), "X: %.2f Y: %.2f Z: %.2f", pos.x, pos.y, pos.z);
    DrawText(buf, 10, 60, 20, LIGHTGRAY);
}

std::int32_t main() {
    InitWindow(800, 450, "silly roads");
    SetTargetFPS(240);

    Cam::State camera{};

    while (!WindowShouldClose()) {
        float dt = std::min(GetFrameTime(), 0.1f);
        Cam::update(camera, Car::get_position(), Car::get_heading(), dt);

        BeginDrawing();
        ClearBackground(SKYBLUE);

        BeginMode3D(camera.camera);
        Terrain::draw();
        Car::update(dt);
        EndMode3D();

        draw_hud();
        EndDrawing();
    }

    Terrain::cleanup();
    CloseWindow();
    return EXIT_SUCCESS;
}