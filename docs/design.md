# Raycer Design Specification

## Project Overview

**Goal**: Develop a car driving simulator with a pickup truck on procedurally generated mountainous terrain with roads.  
**Current State**: A red box (car) with arcade physics, chase camera, and collectible objectives.  
**Next Milestone**: Replace box with 4-wheeled pickup truck model, add procedural road, and improve ground snapping.

## Core Constraints

1. **Tiger Style Compliance**: Follow `CONTRIBUTING.md` strictly (Data-Oriented, `_mut` suffix, aggressive asserts)
2. **LLM Observability**: Verification via structured text logs to stdout
3. **Atomic Stages**: Each stage does ONE thing. If unsure, make it smaller.

---

## Log Format (LLM's Eyes)

```
[FRAME <n>] CAR_POS:<x> <y> <z> | SPEED:<f> | HEADING:<rad> | GROUND:<0|1> | WHEEL_STEER:<rad>
```

---

## Implementation Stages

### Stage 1: Road Data Structure

**What**: Add data for procedural road spline.  
**Why**: Foundation for rendering road on terrain.  
**Size**: ~30 lines

**Concept**: Road is a list of control points. Use Catmull-Rom spline for smooth curves.

**State Changes** (in `game_state.hpp`):
```cpp
constexpr std::int32_t ROAD_POINTS = 64;

// Add to GameState
Vector3 road_points[ROAD_POINTS] = {};  // control points
bool road_initialized = false;
```

**Prompt**:
> Add road data structure: `road_points[64]` array in `GameState`, `road_initialized` flag. Create `init_road_mut(GameState*)` function that generates 64 control points in a winding path. Start from car position, extend in a loop pattern.

**Verification**: Code compiles. Call `init_road_mut` at startup.

---

### Stage 2: Generate Road Points

**What**: Procedurally generate road control points.  
**Why**: Creates an interesting path to drive on.  
**Size**: ~40 lines

**Algorithm**:
1. Start at origin
2. For each point: advance by fixed step, vary direction using noise
3. Sample terrain height for Y coordinate
4. Create a rough loop by curving back toward origin at end

**Prompt**:
> Implement `init_road_mut`: generate 64 road points starting at (0, 0, 0). Use angle-based walking: `angle += noise(i) * 0.3`. Step size 20 units. Set Y from `get_terrain_height()`. Final points should curve back toward start to create a loop. Log `[ROAD] Generated N points`.

**Verification**: Log shows road generation message. Points should form a rough loop.

---

### Stage 3: Render Road Mesh

**What**: Draw the road as a gray strip projected onto terrain.  
**Why**: Visual road to follow.  
**Size**: ~60 lines

**Approach**:
1. Create a new header/source: `src/road.hpp`, `src/road.cpp`
2. Interpolate between control points using Catmull-Rom
3. At each sample, calculate road center + left/right edges (width 8 units)
4. Project edge points onto terrain
5. Build triangle strip mesh, upload to GPU
6. Draw as gray material

**Prompt**:
> Create `road.hpp/cpp`. Implement `generate_road_mesh(const Vector3* points, std::int32_t count)` that returns a `Mesh`. Interpolate points using Catmull-Rom. Road width 8 units. Sample terrain height for each vertex. Return mesh ready for GPU upload. Draw in gray (DARKGRAY).

**Verification**: Run game. Gray road should appear on terrain, following the generated curve.

---

### Stage 4: Road Follows Player

**What**: Regenerate road around player as they move.  
**Why**: Infinite procedural road.  
**Size**: ~30 lines

**Approach**:
1. Track player progress along road
2. When player passes midpoint, shift road points
3. Generate new points ahead, discard old points behind
4. Regenerate mesh

**Prompt**:
> Track which road segment player is nearest. When player passes point 32, shift array: move points 32-63 to 0-31, generate new points at 32-63. Regenerate road mesh. Log `[ROAD] Extended`.

**Verification**: Drive along road. Road should extend ahead infinitely. Log shows extension messages.
