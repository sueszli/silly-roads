#include "lib.h"
#include <print>

#include "raylib.h"

int main() {
    std::println("Hello, World!");
    hello_from_lib();

    InitWindow(800, 450, "raylib [core] example - basic window");
    SetTargetFPS(60);

    while (!WindowShouldClose()) {
        BeginDrawing();
        ClearBackground(RAYWHITE);
        DrawText("Congrats! You created your first window!", 190, 200, 20, LIGHTGRAY);
        DrawText("Press ESC to exit gracefully", 190, 230, 20, DARKGRAY);
        EndDrawing();
    }

    CloseWindow();

    return 0;
}
