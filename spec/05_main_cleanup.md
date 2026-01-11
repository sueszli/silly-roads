# Subproject: Main Loop Cleanup & Camera Extraction

**Objective**: Finish the refactoring by extracting the Camera system and polishing `main.cpp` to be a pure orchestrator.

**Input Files**:
- `src/main.cpp`

**Output Files**:
- `src/systems/camera_system.hpp` (New)
- `src/systems/camera_system.cpp` (New)
- `src/main.cpp` (Final Polish)

## Context
After previous steps, `main.cpp` will still contain `update_camera_mut` and the loop structure. We want to finalize the separation.

## Instructions
1.  **Camera System**:
    -   Create `src/systems/camera_system.hpp`.
    -   Move `update_camera_mut` logic there.
    -   Signature: `void update(CameraState& cam, const Vector3& target, float dt);`

2.  **Main Cleanup**:
    -   Ensure `main.cpp` looks like:
        ```cpp
        int main() {
            // Init
            InitWindow(...);
            GameState state = ...;
            
            // Loop
            while (!WindowShouldClose()) {
                float dt = GetFrameTime();
                
                // Input
                CarControls controls = read_inputs();
                
                // Update
                RoadSystem::update(state.road, state.car.pos);
                TerrainSystem::update(state.terrain, state.car.pos); // If you extracted this too
                Physics::update(state.car, controls, dt);
                CameraSystem::update(state.camera, state.car.pos, dt);
                
                // Draw
                Renderer::draw(state);
            }
            // Cleanup ...
        }
        ```
    -   Remove unused includes.

## Verification
-   Code compilation.
-   Run the game to ensure full feature parity.
