#include "raylib.h"
#include "rlgl.h"
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdlib>

Vector3 ball_pos = {0.0f, 1.0f, 0.0f};
Camera3D camera = {{0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 1.0f}, {0.0f, 1.0f, 0.0f}, 45.0f, CAMERA_PERSPECTIVE};

void game_loop() {
    DrawFPS(10, 10);
    float dt = GetFrameTime();
    assert(dt >= 0.0f);

    ball_pos.z -= 10.0f * dt;
    camera.target = ball_pos;
    camera.position = (Vector3){ball_pos.x, ball_pos.y + 10.0f, ball_pos.z + 10.0f};

    BeginDrawing();
    ClearBackground(BLACK);
    BeginMode3D(camera);

    // infinite grid
    rlPushMatrix();
    rlTranslatef(0.0f, 0.0f, roundf(ball_pos.z));
    DrawGrid(200, 1.0f);
    rlPopMatrix();

    // rolling ball
    rlPushMatrix();
    rlTranslatef(ball_pos.x, ball_pos.y, ball_pos.z);
    rlRotatef((-ball_pos.z) * (180.0f / PI), 1.0f, 0.0f, 0.0f);
    DrawSphere(Vector3{0.0f, 0.0f, 0.0f}, 1.0f, RED);
    DrawSphereWires(Vector3{0.0f, 0.0f, 0.0f}, 1.0f, 16, 16, MAROON);
    rlPopMatrix();

    EndMode3D();
    EndDrawing();
}

std::int32_t main() {
    InitWindow(800, 450, "raycer");
    SetTargetFPS(60);
    while (!WindowShouldClose()) {
        game_loop();
    }
    CloseWindow();
    return EXIT_SUCCESS;
}
