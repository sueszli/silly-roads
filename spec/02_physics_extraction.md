# Subproject: Physics System Extraction

**Objective**: Extract hardcoded physics logic from `main.cpp` into a dedicated `Physics` system module.

**Input Files**:
- `src/main.cpp`
- `src/components.hpp`
- `src/game_state.hpp`

**Output Files**:
- `src/systems/physics.hpp` (New)
- `src/systems/physics.cpp` (New)
- `src/main.cpp` (Modified)

## Context
`main.cpp` currently contains `update_physics_mut` which mixes input handling (Raylib `IsKeyDown`) with physics integration (velocity updates). We want to decouple these.

## Instructions
1.  **Define Input Structure**:
    -   In `src/components.hpp`, add a simple `InputState` or `CarControls` struct:
        ```cpp
        struct CarControls {
            float throttle; // -1.0 (brake/reverse) to 1.0 (accel)
            float steer;    // -1.0 (left) to 1.0 (right)
        };
        ```

2.  **Create Physics Module**:
    -   Create `src/systems/physics.hpp` and `cpp`.
    -   Function signature: `void update_car_physics(Car& car, const CarControls& inputs, const TerrainState& terrain, float dt);`
    -   Move all constants (`PHYS_ACCEL`, `PHYS_BRAKE`, etc.) from `main.cpp` to `physics.cpp` (internal constants) or `physics.hpp`.

3.  **Refactor Logic**:
    -   Move the physics math from `update_physics_mut` to `update_car_physics`.
    -   **Crucial**: The new function should NOT call Raylib's `IsKeyDown`. It should rely entirely on the `CarControls` passed to it.
    -   Map the existing terrain height lookups. You might need to pass a specific function or interface to look up terrain height if `TerrainState` isn't sufficient. (Currently `get_terrain_height` is a global in `terrain.hpp`, you can keep using that for now, but prefer passing context if easy).

4.  **Update `main.cpp`**:
    -   Create a helper to read inputs into `CarControls`.
    -   Call `Physics::update_car_physics` inside the game loop.
    -   Remove the old `update_physics_mut`.

## Verification
-   The car should drive exactly as before.
-   `main.cpp` should no longer contain `PHYS_` constants or integration math.
