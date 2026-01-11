# Subproject: Renderer Extraction

**Objective**: Move all Raylib drawing calls out of `main.cpp` into a `Renderer` module.

**Input Files**:
- `src/main.cpp`

**Output Files**:
- `src/systems/renderer.hpp` (New)
- `src/systems/renderer.cpp` (New)
- `src/main.cpp` (Modified)

## Context
`draw_frame` in `main.cpp` contains logic for drawing terrain, the car (using `rlgl` calls directly), wheels, and UI text. This should be isolated.

## Instructions
1.  **Create Renderer System**:
    -   Create `src/systems/renderer.hpp`.
    -   Expose: `void draw_scene(const GameState& state);` (It's okay to pass read-only GameState here as rendering needs access to everything).

2.  **Move Logic**:
    -   Move `draw_frame` content to `Renderer::draw_scene`.
    -   Move `log_state` (debug text) into the renderer or a separate debug helper.
    -   Ensure all `rlgl.h` includes are moved to `renderer.cpp`.

3.  **Refactor Car Rendering**:
    -   Consider making a helper `draw_car(const Car& car)` private to the renderer.

4.  **Update `main.cpp`**:
    -   Replace the giant `draw_frame` function with a single call `Renderer::draw_scene(state)`.

## Verification
-   Visuals should remain exactly the same.
-   `main.cpp` should have zero `Draw*` or `rl*` calls.
