# Subproject: Data Structure Refactoring

**Objective**: Refactor the monolithic `GameState` struct into focused, cohesive component structs to improve code locality and logical grouping.

**Input Files**:
- `src/game_state.hpp`
- `src/main.cpp`
- `src/terrain.cpp`

**Output Files**:
- `src/components.hpp` (New)
- `src/game_state.hpp` (Modified)
- `src/main.cpp` (Modified)
- `src/terrain.cpp` (Modified)

## Context
Currently, `GameState` in `src/game_state.hpp` is a flat list of variables mixing physics states, rendering resources, and game logic counters. This leads to poor cache locality and makes it hard to see which data belongs to which system.

## Instructions
1.  **Create `src/components.hpp`**:
    -   Define a `Car` struct containing: `pos`, `vel`, `heading`, `speed`, `pitch`, `roll`, `wheels`, `wheel_heights`.
    -   Define a `RoadState` struct containing: `points`, `dense_points`, `gen_x`, `gen_z`, `gen_angle`, `gen_next_segment`, `initialized`.
    -   Define a `CameraState` struct containing: `camera` (the Raylib `Camera3D`), `target_pos`.

2.  **Update `src/game_state.hpp`**:
    -   Include `components.hpp`.
    -   Refactor `GameState` to use these new structs.
    -   Example:
        ```cpp
        struct GameState {
            Car car;
            RoadState road;
            CameraState camera;
            std::vector<TerrainChunk> terrain_chunks;
            Texture2D texture;
            // ... other loose fields
        };
        ```
    -   **Important**: Keep `terrain_chunks` and `texture` in `GameState` for now, or group them into a `TerrainState` if it seems appropriate.

3.  **Update References**:
    -   Scan `src/main.cpp` and `src/terrain.cpp`.
    -   Replace all flat accesses (e.g., `state->car_pos`) with hierarchical accesses (e.g., `state->car.pos`).
    -   Ensure the code compiles and runs exactly as before.

## Verification
-   Run the game. It should behave identical to before.
-   Check that `GameState` is now significantly smaller in terms of direct member count.
