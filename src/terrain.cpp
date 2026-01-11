#include "terrain.hpp"
#include "raymath.h"
#include "rlgl.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <numeric>
#include <random>

namespace {

constexpr float NOISE_SCALE = 0.05f;
constexpr float TERRAIN_HEIGHT_SCALE = 7.0f;
constexpr float ROAD_NOISE_SCALE = 0.003f;
constexpr float ROAD_AMPLITUDE = 200.0f;
constexpr int32_t GRID_SIZE = 128;
constexpr float TILE_SIZE = 1.0f;
constexpr float CHUNK_SIZE = (GRID_SIZE - 1) * TILE_SIZE;

struct TerrainChunk {
    int cx;
    int cz;
    Model model;
};

struct TerrainState {
    std::vector<TerrainChunk> chunks;
    Texture2D texture = {};
    float chunk_size = 0.0f;
    Vector3 start_pos = {};
    float start_heading = 0.0f;
    bool initialized = false;
} internal_state;

float get_road_center_x(float z); // forward declaration

void ensure_initialized() {
    if (internal_state.initialized) {
        return;
    }
    internal_state.initialized = true;

    Image img = GenImageColor(2, 2, WHITE);
    internal_state.texture = LoadTextureFromImage(img);
    UnloadImage(img);
    internal_state.chunk_size = CHUNK_SIZE;

    // compute road start position
    float start_z = 0.0f;
    float start_x = get_road_center_x(start_z) + 1.5f;
    internal_state.start_pos = {start_x, Terrain::get_height(start_x, start_z) + 2.0f, start_z};

    // align with road
    float look_ahead_x = get_road_center_x(start_z + 1.0f) + 1.5f;
    internal_state.start_heading = std::atan2(look_ahead_x - start_x, 1.0f);
}

float sample_perlin_noise(float x, float y, float z) {
    const auto get_permutation = []() {
        std::array<int32_t, 512> p;
        std::iota(p.begin(), p.begin() + 256, 0);
        std::shuffle(p.begin(), p.begin() + 256, std::default_random_engine(42));
        std::copy(p.begin(), p.begin() + 256, p.begin() + 256);
        return p;
    };

    const auto get_grad = [](int32_t hash, float x, float y, float z) {
        const int32_t h = hash & 15;
        const float u = h < 8 ? x : y;
        const float v = h < 4 ? y : h == 12 || h == 14 ? x : z;
        return ((h & 1) == 0 ? u : -u) + ((h & 2) == 0 ? v : -v);
    };

    static const auto p = get_permutation();
    const auto fade = [](float t) { return t * t * t * (t * (t * 6 - 15) + 10); };
    const auto lerp = [](float t, float a, float b) { return a + t * (b - a); };

    const int32_t X = static_cast<int32_t>(std::floor(x)) & 255;
    const int32_t Y = static_cast<int32_t>(std::floor(y)) & 255;
    const int32_t Z = static_cast<int32_t>(std::floor(z)) & 255;

    x -= std::floor(x);
    y -= std::floor(y);
    z -= std::floor(z);

    const float u = fade(x), v = fade(y), w = fade(z);

    // cast to size_t for array indexing
    const size_t idx_X = static_cast<size_t>(X);
    const size_t idx_Y = static_cast<size_t>(Y);
    const size_t idx_Z = static_cast<size_t>(Z);

    const size_t A = static_cast<size_t>(p[idx_X]) + idx_Y;
    const size_t AA = static_cast<size_t>(p[A]) + idx_Z;
    const size_t AB = static_cast<size_t>(p[A + 1]) + idx_Z;
    const size_t B = static_cast<size_t>(p[idx_X + 1]) + idx_Y;
    const size_t BA = static_cast<size_t>(p[B]) + idx_Z;
    const size_t BB = static_cast<size_t>(p[B + 1]) + idx_Z;

    return lerp(w, lerp(v, lerp(u, get_grad(p[AA], x, y, z), get_grad(p[BA], x - 1, y, z)), lerp(u, get_grad(p[AB], x, y - 1, z), get_grad(p[BB], x - 1, y - 1, z))), lerp(v, lerp(u, get_grad(p[AA + 1], x, y, z - 1), get_grad(p[BA + 1], x - 1, y, z - 1)), lerp(u, get_grad(p[AB + 1], x, y - 1, z - 1), get_grad(p[BB + 1], x - 1, y - 1, z - 1))));
}

float get_road_center_x(float z) { return sample_perlin_noise(0.0f, 42.0f, z * ROAD_NOISE_SCALE) * ROAD_AMPLITUDE; }

Mesh generate_chunk_mesh(float offset_x, float offset_z) {
    const auto get_normal = [](float x, float z) {
        const auto h = [](float x, float z) { return sample_perlin_noise(x * NOISE_SCALE, 0.0f, z * NOISE_SCALE) * TERRAIN_HEIGHT_SCALE; };
        const float step = 0.1f;
        const float height = h(x, z);
        const Vector3 v1 = {step, h(x + step, z) - height, 0.0f};
        const Vector3 v2 = {0.0f, h(x, z + step) - height, step};
        return Vector3Normalize(Vector3CrossProduct(v2, v1));
    };
    const auto get_color = [&](int x, int z, float wx, float wz) {
        const float dist = std::abs(wx - get_road_center_x(wz));
        Color col = ((x + z) % 2 == 0) ? DARKGREEN : GREEN; // green area
        constexpr Color ROAD_COLOR = {30, 30, 30, 255};     // dark asphalt
        if (dist < 6.0f) {
            col = (dist < 4.0f) ? ROAD_COLOR : ColorLerp(ROAD_COLOR, col, (dist - 4.0f) / 2.0f);
        }
        return col;
    };

    Mesh mesh = {};
    mesh.vertexCount = GRID_SIZE * GRID_SIZE;
    mesh.triangleCount = (GRID_SIZE - 1) * (GRID_SIZE - 1) * 2;

    mesh.vertices = static_cast<float *>(MemAlloc(static_cast<unsigned int>(static_cast<size_t>(mesh.vertexCount) * 3 * sizeof(float))));
    mesh.normals = static_cast<float *>(MemAlloc(static_cast<unsigned int>(static_cast<size_t>(mesh.vertexCount) * 3 * sizeof(float))));
    mesh.texcoords = static_cast<float *>(MemAlloc(static_cast<unsigned int>(static_cast<size_t>(mesh.vertexCount) * 2 * sizeof(float))));
    mesh.colors = static_cast<unsigned char *>(MemAlloc(static_cast<unsigned int>(static_cast<size_t>(mesh.vertexCount) * 4 * sizeof(unsigned char))));
    mesh.indices = static_cast<unsigned short *>(MemAlloc(static_cast<unsigned int>(static_cast<size_t>(mesh.triangleCount) * 3 * sizeof(unsigned short))));

    for (int z = 0; z < GRID_SIZE; ++z) {
        for (int x = 0; x < GRID_SIZE; ++x) {
            const int i = z * GRID_SIZE + x;
            const float wx = offset_x + x * TILE_SIZE;
            const float wz = offset_z + z * TILE_SIZE;
            const float wy = Terrain::get_height(wx, wz);
            const Vector3 n = get_normal(wx, wz);
            const Color col = get_color(x, z, wx, wz);

            mesh.vertices[i * 3] = static_cast<float>(x) * TILE_SIZE;
            mesh.vertices[i * 3 + 1] = wy;
            mesh.vertices[i * 3 + 2] = static_cast<float>(z) * TILE_SIZE;

            mesh.normals[i * 3] = n.x;
            mesh.normals[i * 3 + 1] = n.y;
            mesh.normals[i * 3 + 2] = n.z;

            mesh.texcoords[i * 2] = 0.0f;
            mesh.texcoords[i * 2 + 1] = 0.0f;

            mesh.colors[i * 4] = col.r;
            mesh.colors[i * 4 + 1] = col.g;
            mesh.colors[i * 4 + 2] = col.b;
            mesh.colors[i * 4 + 3] = col.a;
        }
    }

    constexpr int INDEX_GRID = GRID_SIZE - 1;
    for (int z = 0; z < INDEX_GRID; ++z) {
        for (int x = 0; x < INDEX_GRID; ++x) {
            const int i = z * INDEX_GRID + x;
            const auto tl = static_cast<unsigned short>(z * GRID_SIZE + x);
            const auto bl = static_cast<unsigned short>((z + 1) * GRID_SIZE + x);
            const int idx = i * 6;
            mesh.indices[idx] = tl;
            mesh.indices[idx + 1] = bl;
            mesh.indices[idx + 2] = tl + 1;
            mesh.indices[idx + 3] = tl + 1;
            mesh.indices[idx + 4] = bl;
            mesh.indices[idx + 5] = bl + 1;
        }
    }
    return mesh;
}

} // namespace

namespace Terrain {

void update(const Vector3 &car_pos) {
    ensure_initialized();
    const int cx = (int)std::floor(car_pos.x / CHUNK_SIZE);
    const int cz = (int)std::floor(car_pos.z / CHUNK_SIZE);

    // unload distant chunks
    std::erase_if(internal_state.chunks, [&](const TerrainChunk &c) {
        bool keep = std::abs(c.cx - cx) <= 2 && std::abs(c.cz - cz) <= 2;
        if (!keep)
            UnloadModel(c.model);
        return !keep;
    });

    // load new chunks
    for (int z = -2; z <= 2; ++z) {
        for (int x = -2; x <= 2; ++x) {
            if (std::none_of(internal_state.chunks.begin(), internal_state.chunks.end(), [&](const auto &c) { return c.cx == cx + x && c.cz == cz + z; })) {
                Mesh mesh = generate_chunk_mesh((cx + x) * CHUNK_SIZE, (cz + z) * CHUNK_SIZE);
                UploadMesh(&mesh, false);
                Model model = LoadModelFromMesh(mesh);
                model.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = internal_state.texture;
                internal_state.chunks.push_back({cx + x, cz + z, model});
            }
        }
    }
}

void draw() {
    ensure_initialized();
    for (const auto &chunk : internal_state.chunks) {
        DrawModel(chunk.model, {(float)chunk.cx * CHUNK_SIZE, 0.0f, (float)chunk.cz * CHUNK_SIZE}, 1.0f, WHITE);
    }
}

void cleanup() {
    for (const auto &chunk : internal_state.chunks) {
        UnloadModel(chunk.model);
    }
    internal_state.chunks.clear();
    UnloadTexture(internal_state.texture);
}

float get_height(float x, float z) { return sample_perlin_noise(x * NOISE_SCALE, 0.0f, z * NOISE_SCALE) * TERRAIN_HEIGHT_SCALE; }

Vector3 get_start_position() {
    ensure_initialized();
    return internal_state.start_pos;
}

float get_start_heading() {
    ensure_initialized();
    return internal_state.start_heading;
}

} // namespace Terrain