#include "raylib.h"
#include "rlgl.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <numeric>
#include <random>
#include <vector>

namespace {

constexpr std::int32_t GRID_SIZE = 120;
constexpr float TILE_SIZE = 1.0f;
constexpr float NOISE_SCALE = 0.1f;
constexpr float TERRAIN_HEIGHT_SCALE = 10.0f;
constexpr float BALL_RADIUS = 0.5f;

[[nodiscard]] float sample_noise(const std::vector<std::int32_t> &p, float x, float y, float z) {
    const auto fade = [](float t) { return t * t * t * (t * (t * 6 - 15) + 10); };
    const auto lerp = [](float t, float a, float b) { return a + t * (b - a); };
    const auto grad = [](std::int32_t hash, float x, float y, float z) {
        const std::int32_t h = hash & 15;
        const float u = h < 8 ? x : y;
        const float v = h < 4 ? y : h == 12 || h == 14 ? x : z;
        return ((h & 1) == 0 ? u : -u) + ((h & 2) == 0 ? v : -v);
    };

    const std::int32_t X = static_cast<std::int32_t>(std::floor(x)) & 255;
    const std::int32_t Y = static_cast<std::int32_t>(std::floor(y)) & 255;
    const std::int32_t Z = static_cast<std::int32_t>(std::floor(z)) & 255;

    x -= std::floor(x);
    y -= std::floor(y);
    z -= std::floor(z);

    const float u = fade(x);
    const float v = fade(y);
    const float w = fade(z);

    const std::int32_t A = p[static_cast<std::size_t>(X)] + Y;
    const std::int32_t AA = p[static_cast<std::size_t>(A)] + Z;
    const std::int32_t AB = p[static_cast<std::size_t>(A) + 1] + Z;
    const std::int32_t B = p[static_cast<std::size_t>(X) + 1] + Y;
    const std::int32_t BA = p[static_cast<std::size_t>(B)] + Z;
    const std::int32_t BB = p[static_cast<std::size_t>(B) + 1] + Z;

    return lerp(w, lerp(v, lerp(u, grad(p[static_cast<std::size_t>(AA)], x, y, z), grad(p[static_cast<std::size_t>(BA)], x - 1, y, z)), lerp(u, grad(p[static_cast<std::size_t>(AB)], x, y - 1, z), grad(p[static_cast<std::size_t>(BB)], x - 1, y - 1, z))), lerp(v, lerp(u, grad(p[static_cast<std::size_t>(AA) + 1], x, y, z - 1), grad(p[static_cast<std::size_t>(BA) + 1], x - 1, y, z - 1)), lerp(u, grad(p[static_cast<std::size_t>(AB) + 1], x, y - 1, z - 1), grad(p[static_cast<std::size_t>(BB) + 1], x - 1, y - 1, z - 1))));
}

[[nodiscard]] std::vector<std::int32_t> generate_permutation() {
    std::vector<std::int32_t> p(256);
    std::iota(p.begin(), p.end(), 0);
    constexpr std::int32_t seed = 42;
    std::default_random_engine engine(static_cast<std::uint32_t>(seed));
    std::shuffle(p.begin(), p.end(), engine);
    p.insert(p.end(), p.begin(), p.end());
    return p;
}

[[nodiscard]] float get_terrain_height(const std::vector<std::int32_t> &p, float x, float z) { return sample_noise(p, x * NOISE_SCALE, 0.0f, z * NOISE_SCALE) * TERRAIN_HEIGHT_SCALE; }

[[nodiscard]] Vector3 get_terrain_normal(const std::vector<std::int32_t> &p, float x, float z) {
    const float h = get_terrain_height(p, x, z);
    constexpr float step = 0.1f;
    const float h_x = get_terrain_height(p, x + step, z);
    const float h_z = get_terrain_height(p, x, z + step);

    const Vector3 v1 = {step, h_x - h, 0.0f};
    const Vector3 v2 = {0.0f, h_z - h, step};

    const Vector3 normal = {v1.y * v2.z - v1.z * v2.y, v1.z * v2.x - v1.x * v2.z, v1.x * v2.y - v1.y * v2.x};
    const float len = std::sqrt(normal.x * normal.x + normal.y * normal.y + normal.z * normal.z);
    return {normal.x / len, normal.y / len, normal.z / len};
}

struct GameState {
    Vector3 ball_pos;
    Vector3 ball_vel;
    Camera3D camera;
    Mesh terrain_mesh;
    Model terrain_model;
    Texture2D texture;
    bool mesh_generated;
    std::vector<std::int32_t> noise_permutation;
};

void generate_terrain_mesh(GameState &state) {
    if (state.terrain_mesh.vertexCount > 0) {
        UnloadMesh(state.terrain_mesh);
    }

    state.terrain_mesh = {0};
    state.terrain_mesh.triangleCount = (GRID_SIZE - 1) * (GRID_SIZE - 1) * 2;
    state.terrain_mesh.vertexCount = state.terrain_mesh.triangleCount * 3;

    state.terrain_mesh.vertices = static_cast<float *>(MemAlloc(static_cast<std::uint32_t>(state.terrain_mesh.vertexCount) * 3 * sizeof(float)));
    state.terrain_mesh.normals = static_cast<float *>(MemAlloc(static_cast<std::uint32_t>(state.terrain_mesh.vertexCount) * 3 * sizeof(float)));
    state.terrain_mesh.texcoords = static_cast<float *>(MemAlloc(static_cast<std::uint32_t>(state.terrain_mesh.vertexCount) * 2 * sizeof(float)));
    state.terrain_mesh.colors = static_cast<unsigned char *>(MemAlloc(static_cast<std::uint32_t>(state.terrain_mesh.vertexCount) * 4 * sizeof(unsigned char)));

    assert(state.terrain_mesh.vertices != nullptr);

    std::int32_t vCounter = 0;

    for (std::int32_t z = 0; z < GRID_SIZE - 1; z++) {
        for (std::int32_t x = 0; x < GRID_SIZE - 1; x++) {
            const float x1 = static_cast<float>(x) * TILE_SIZE;
            const float z1 = static_cast<float>(z) * TILE_SIZE;
            const float x2 = static_cast<float>(x + 1) * TILE_SIZE;
            const float z2 = static_cast<float>(z) * TILE_SIZE;
            const float x3 = static_cast<float>(x) * TILE_SIZE;
            const float z3 = static_cast<float>(z + 1) * TILE_SIZE;
            const float x4 = static_cast<float>(x + 1) * TILE_SIZE;
            const float z4 = static_cast<float>(z + 1) * TILE_SIZE;

            const float y1 = get_terrain_height(state.noise_permutation, x1, z1);
            const float y2 = get_terrain_height(state.noise_permutation, x2, z2);
            const float y3 = get_terrain_height(state.noise_permutation, x3, z3);
            const float y4 = get_terrain_height(state.noise_permutation, x4, z4);

            const Vector3 n1 = get_terrain_normal(state.noise_permutation, x1, z1);

            auto PushVert = [&](float px, float py, float pz, float nx, float ny, float nz, float u, float v) {
                state.terrain_mesh.vertices[vCounter * 3] = px;
                state.terrain_mesh.vertices[vCounter * 3 + 1] = py;
                state.terrain_mesh.vertices[vCounter * 3 + 2] = pz;
                state.terrain_mesh.normals[vCounter * 3] = nx;
                state.terrain_mesh.normals[vCounter * 3 + 1] = ny;
                state.terrain_mesh.normals[vCounter * 3 + 2] = nz;
                state.terrain_mesh.texcoords[vCounter * 2] = u;
                state.terrain_mesh.texcoords[vCounter * 2 + 1] = v;
                state.terrain_mesh.colors[vCounter * 4] = 100;
                state.terrain_mesh.colors[vCounter * 4 + 1] = 100;
                state.terrain_mesh.colors[vCounter * 4 + 2] = 100;
                state.terrain_mesh.colors[vCounter * 4 + 3] = 255;
                vCounter++;
            };

            PushVert(x1, y1, z1, n1.x, n1.y, n1.z, 0.0f, 0.0f);
            PushVert(x3, y3, z3, n1.x, n1.y, n1.z, 0.0f, 1.0f);
            PushVert(x2, y2, z2, n1.x, n1.y, n1.z, 1.0f, 0.0f);

            const Vector3 n2 = {0.0f, 1.0f, 0.0f};
            PushVert(x2, y2, z2, n2.x, n2.y, n2.z, 1.0f, 0.0f);
            PushVert(x3, y3, z3, n2.x, n2.y, n2.z, 0.0f, 1.0f);
            PushVert(x4, y4, z4, n2.x, n2.y, n2.z, 1.0f, 1.0f);
        }
    }

    UploadMesh(&state.terrain_mesh, false);
    state.terrain_model = LoadModelFromMesh(state.terrain_mesh);
    state.terrain_model.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = state.texture;
    state.mesh_generated = true;
}

void update_physics(GameState &state, float dt) {
    constexpr Vector3 gravity{0.0f, -20.0f, 0.0f};

    state.ball_vel.x += gravity.x * dt;
    state.ball_vel.y += gravity.y * dt;
    state.ball_vel.z += gravity.z * dt;

    state.ball_pos.x += state.ball_vel.x * dt;
    state.ball_pos.y += state.ball_vel.y * dt;
    state.ball_pos.z += state.ball_vel.z * dt;

    if (state.ball_pos.y < -50.0f) {
        state.ball_pos = {60.0f, 10.0f, 60.0f};
        state.ball_vel = {0.0f, 0.0f, 0.0f};
    }

    const float terrain_h = get_terrain_height(state.noise_permutation, state.ball_pos.x, state.ball_pos.z);

    if (state.ball_pos.y <= terrain_h + BALL_RADIUS) {
        state.ball_pos.y = terrain_h + BALL_RADIUS;
        const Vector3 normal = get_terrain_normal(state.noise_permutation, state.ball_pos.x, state.ball_pos.z);
        const float dot = state.ball_vel.x * normal.x + state.ball_vel.y * normal.y + state.ball_vel.z * normal.z;

        state.ball_vel.x -= dot * normal.x;
        state.ball_vel.y -= dot * normal.y;
        state.ball_vel.z -= dot * normal.z;

        state.ball_vel.x *= 0.99f;
        state.ball_vel.z *= 0.99f;
    }
}

void game_loop(GameState &state) {
    if (!state.mesh_generated) {
        generate_terrain_mesh(state);
    }

    DrawFPS(10, 10);
    float dt = GetFrameTime();
    if (dt > 0.05f)
        dt = 0.05f;

    update_physics(state, dt);

    state.camera.target = state.ball_pos;
    state.camera.position = {state.ball_pos.x, state.ball_pos.y + 15.0f, state.ball_pos.z + 15.0f};

    BeginDrawing();
    ClearBackground(SKYBLUE);
    BeginMode3D(state.camera);

    DrawModel(state.terrain_model, {0.0f, 0.0f, 0.0f}, 1.0f, WHITE);
    DrawGrid(GRID_SIZE, 10.0f);
    DrawSphere(state.ball_pos, BALL_RADIUS, RED);
    DrawSphereWires(state.ball_pos, BALL_RADIUS, 16, 16, MAROON);

    EndMode3D();
    EndDrawing();
}

} // namespace

std::int32_t main() {
    InitWindow(800, 450, "raycer");
    SetTargetFPS(60);

    Image checked = GenImageChecked(256, 256, 32, 32, DARKGRAY, WHITE);
    Texture2D texture = LoadTextureFromImage(checked);
    UnloadImage(checked);

    GameState state{
        .ball_pos = {60.0f, 10.0f, 60.0f},
        .ball_vel = {0.0f, 0.0f, 0.0f},
        .camera = {.position = {0.0f, 10.0f, 10.0f}, .target = {0.0f, 0.0f, 0.0f}, .up = {0.0f, 1.0f, 0.0f}, .fovy = 45.0f, .projection = CAMERA_PERSPECTIVE},
        .terrain_mesh = {},
        .terrain_model = {},
        .texture = texture,
        .mesh_generated = false,
        .noise_permutation = generate_permutation(),
    };

    while (!WindowShouldClose()) {
        game_loop(state);
    }

    UnloadModel(state.terrain_model);
    UnloadTexture(state.texture);
    CloseWindow();

    return EXIT_SUCCESS;
}
