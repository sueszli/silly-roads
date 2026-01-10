# Task 1: Extract Road Spline Mathematics

## Goal
Decouple the road geometry calculation from the mesh generation. We need the "dense" road path (the interpolated points that form the curve) to be available as a data structure, independent of the 3D mesh construction. This is a prerequisite for the terrain to "know" where the road is.

## Current State
Currently, `generate_road_mesh` in `src/road.cpp`:
1.  Takes sparse control points (`state->road_points`).
2.  Interpolates them using Catmull-Rom splines into `center_points`.
3.  Immediately builds a 3D mesh from these `center_points`.

## Required Changes
1.  **Refactor `src/road.cpp` / `src/road.hpp`**:
    *   Extract the spline interpolation logic into a new function, e.g., `std::vector<Vector3> generate_road_path(const std::vector<Vector3>& control_points)`.
    *   This function should return the list of interpolated "dense" points.
    *   Update `generate_road_mesh` to use this new function to get the points, then build the mesh as before (for now).
2.  **Verify**:
    *   Ensure the road mesh still draws correctly in the game.

## Prompt
To execute this task, use the following prompt:

> `Please read 'specs/01_road_math.md' and refactor the road generation code in 'src/road.cpp' to separate the spline interpolation logic from the mesh generation.`
