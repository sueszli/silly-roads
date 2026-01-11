#pragma once
#include "raylib.h"
#include <cstdint>
#include <vector>

#include "components.hpp"

struct GameState {
    Car car;
    RoadState road;
    CameraState camera;

    //
    // terrain
    //

    struct TerrainChunk {
        int cx; // chunk grid x coordinate
        int cz; // chunk grid z coordinate
        Model model;
    };

    std::vector<TerrainChunk> terrain_chunks;
    Texture2D texture = {};
    float chunk_size = 0.0f; // stored for convenience (=(GRID_SIZE-1)*TILE_SIZE)

    std::int32_t frame_count = 0;
    bool road_path_generated = false;
};
