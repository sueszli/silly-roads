# Subproject: Road System Extraction

**Objective**: encapsulated road generation and management into a dedicated `RoadSystem` module.

**Input Files**:
- `src/main.cpp`
- `src/game_state.hpp` (or `components.hpp` if 01 is done)

**Output Files**:
- `src/systems/road_system.hpp` (New)
- `src/systems/road_system.cpp` (New)
- `src/main.cpp` (Modified)

## Context
Road logic is currently split between global functions (`generate_next_road_point_mut`, `update_road_mut`, `init_road_mut`) in `main.cpp`. This clutters the entry point.

## Instructions
1.  **Create Road System**:
    -   Create `src/systems/road_system.hpp`.
    -   Define a namespace `RoadSystem`.
    -   Expose functions:
        -   `void init(RoadState& road);`
        -   `void update(RoadState& road, const Vector3& car_pos);`

2.  **Move Logic**:
    -   Move `generate_next_road_point_mut` logic into `road_system.cpp` (make it a private helper or member).
    -   Move `init_road_mut` logic to `RoadSystem::init`.
    -   Move `update_road_mut` logic to `RoadSystem::update`. Note: `update` needs `car_pos` to determine when to generate new segments. Avoid passing the whole `GameState`.

3.  **Update `main.cpp`**:
    -   Include `systems/road_system.hpp`.
    -   Replace manual calls with `RoadSystem::init(state.road)` and `RoadSystem::update(state.road, state.car.pos)`.

## Verification
-   Road generation should continue to work infinitely as the car drives.
-   `main.cpp` should lose ~100 lines of code.
