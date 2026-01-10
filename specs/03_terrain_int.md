# Task 3: Terrain Integration and Cleanup

## Goal
Visualize the road directly on the terrain surface by modifying vertex colors, and remove the old "floating" road mesh. This achieves the user's desired "painted on" look.

## Current State
*   Terrain is effectively a grid of colored vertices.
*   Road is currently a separate 3D model drawn on top.
*   We have (from Tasks 1 & 2) the tools to get road segments relevant to a terrain tile.

## Required Changes
1.  **Modify Terrain Generation (`src/terrain.cpp`)**:
    *   Update `generate_terrain_mesh_data` to accept the road path data (or the spatial query helper).
    *   Inside the vertex generation loop (`process_tile` / `push_vert`):
        *   Query relevant road segments for the current tile (once per tile or reuse for the whole chunk).
        *   For each vertex position `(x, z)`:
            *   Calculate the shortest distance to the set of road segments.
            *   If distance < `ROAD_WIDTH / 2`: Use `ROAD_COLOR` (e.g., Dark Gray/Brown).
            *   (Optional) Blend color slightly at the edges for smoothness.
            *   Else: Use `TERRAIN_COLOR` (default Green/Gray).
2.  **Cleanup (`src/main.cpp` & `src/game_state.hpp`)**:
    *   Remove `road_mesh`, `road_model`, `road_mesh_generated`.
    *   Remove `generate_road_mesh_mut` (or the drawing part of it).
    *   Remove `DrawModel(state->road_model)` from `draw_frame`.
    *   Ensure `update_terrain_mut` passes the necessary road data when regenerating terrain.

## Prompt
To execute this task, use the following prompt:

> `Please read 'specs/03_terrain_int.md' and integrate the road visualization into the terrain generation, then remove the old road mesh code.`
