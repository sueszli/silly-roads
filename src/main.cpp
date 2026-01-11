#include "camera.hpp"
#include "car.hpp"
#include "raylib.h"
#include "terrain.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

void draw_scene(const CameraState &cam) {
    BeginDrawing();
    ClearBackground(SKYBLUE);

    BeginMode3D(cam.camera);
    Terrain::draw();
    Car::draw();
    EndMode3D();

    char buf[64];
    std::snprintf(buf, sizeof(buf), "SPEED: %.2f", Car::get_speed());
    DrawText(buf, 10, 40, 20, WHITE);
    Vector3 pos = Car::get_position();
    std::snprintf(buf, sizeof(buf), "X: %.2f Y: %.2f Z: %.2f", pos.x, pos.y, pos.z);
    DrawText(buf, 10, 60, 20, LIGHTGRAY);

    EndDrawing();
}

std::int32_t main() {
    InitWindow(800, 450, "raycer");
    SetTargetFPS(240);

    Vector3 start_pos;
    float start_heading;
    Terrain::init(start_pos, start_heading);
    Car::init(start_pos, start_heading);

    CameraState camera{};

    while (!WindowShouldClose()) {
        float dt = std::min(GetFrameTime(), 0.1f);

        Car::read_input();
        Terrain::update(Car::get_position());
        Car::update(dt);
        update_camera(camera, Car::get_position(), Car::get_heading(), dt);

        draw_scene(camera);
    }

    Terrain::cleanup();
    CloseWindow();
    return EXIT_SUCCESS;
}