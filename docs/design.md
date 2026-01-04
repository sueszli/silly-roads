# Raycer Design Specification

## Project Overview

**Goal**: Develop a simple car driving simulator on mountainous terrain.  
**Current State**: A red ball rolls on procedurally generated mountainous terrain with basic physics.  
**Target State**: A controllable car with arcade physics drives on the terrain, chased by a dynamic camera, with collectible objectives.

## Core Constraints

1. **Tiger Style Compliance**: Follow `CONTRIBUTING.md` strictly (Data-Oriented, `_mut` suffix, aggressive asserts)
2. **LLM Observability**: Since the LLM cannot "see" the screen, all verification happens via structured text logs to stdout
3. **Atomic Stages**: Each stage does ONE thing. If unsure, make it smaller.

---

## Log Format (LLM's Eyes)

All stages output a standard log line each frame for verification:

```
[FRAME <n>] POS:<x> <y> <z> | VEL:<x> <y> <z> | GROUND:<0|1> | HEADING:<rad> | SPEED:<f>
```

Fields may be omitted if not yet implemented. Example:
```
[FRAME 42] POS:60.12 5.34 60.00 | VEL:0.00 -2.10 0.00 | GROUND:0
```

---

## Implementation Stages

### Stage 1: Replace Ball with Car Visual

**What**: Change the rendered shape from a sphere to a box (car body placeholder).  
**Why**: Visual foundation for car before adding physics.  
**Size**: ~10 lines changed

**Requirements**:
1. Replace `DrawSphere(state->ball_pos, BALL_RADIUS, RED)` with `DrawCube(state->ball_pos, 2.0f, 1.0f, 4.0f, RED)`
2. Rename `ball_pos` → `car_pos` and `ball_vel` → `car_vel` throughout codebase
3. Add `float car_heading = 0.0f` to `GameState` (used later)
4. Update log to say `CAR_POS` (optional but helpful)

**Prompt**:
> Replace the ball visual with a red box (2x1x4 units). Rename `ball_pos`/`ball_vel` to `car_pos`/`car_vel` everywhere. Add `float car_heading = 0.0f` to `GameState`. Keep physics identical.

**Verification**: Run game. A red box should roll on terrain. Log shows same physics behavior.

---

### Stage 2: Rotate Car by Heading

**What**: Make the car box rotate visually based on `car_heading`.  
**Why**: Visual feedback before adding steering physics.  
**Size**: ~15 lines changed

**Requirements**:
1. Use raylib's `DrawModelEx` or calculate rotation matrix manually
2. For now, just increment `car_heading` by a tiny amount each frame to test rotation
3. Car should visibly spin slowly around Y axis

**Prompt**:
> Make the car box rotate around the Y-axis based on `car_heading`. For testing, increment `car_heading` by `0.01f` each frame. Use `DrawCubeWires` or `rlPushMatrix`/`rlRotatef` to apply the rotation before drawing.

**Verification**: Run game. The red box should slowly spin as it moves.

---

### Stage 3: Steering Input

**What**: A/D keys control `car_heading` instead of directly moving left/right.  
**Why**: Introduce steering mechanics.  
**Size**: ~20 lines changed

**Requirements**:
1. Remove the constant heading increment from Stage 3
2. A/D keys now change `car_heading` (only when moving, i.e., `Vector3Length(car_vel) > 0.5f`)
3. W/S still apply force, but now in the direction of `car_heading`
4. Update log to include `HEADING:<radians>`

**Prompt**:
> Remap controls: A/D change `car_heading` (turn rate ~2.0 rad/s). W/S apply force in the heading direction (forward = +sin(heading) X, +cos(heading) Z). Steering only works when moving. Log the heading.

**Verification**: Run game. A/D should rotate the car. W should accelerate in facing direction.

---

### Stage 4: Arcade Car Speed

**What**: Replace velocity-based movement with scalar `car_speed` plus heading.  
**Why**: Arcade cars use speed+heading, not raw velocity vectors.  
**Size**: ~30 lines changed

**Requirements**:
1. Add `float car_speed = 0.0f` to `GameState`
2. W increases `car_speed`, S decreases (with limits)
3. Drag reduces `car_speed` over time
4. `car_vel.x = sin(car_heading) * car_speed`, `car_vel.z = cos(car_heading) * car_speed`
5. Keep Y velocity separate for gravity
6. Log `SPEED:<f>`

**Prompt**:
> Convert to arcade physics: add `car_speed` scalar. W/S accelerate/decelerate. Derive horizontal velocity from heading. Apply separate gravity to Y. Add drag to speed. Log speed.

**Verification**: Run game. Car should accelerate smoothly. Log shows `SPEED` increasing/decreasing.

---

### Stage 5: Chase Camera

**What**: Camera follows behind the car based on heading.  
**Why**: Creates driving game feel.  
**Size**: ~25 lines changed

**Requirements**:
1. Calculate `target_cam_pos` = car position + offset rotated by heading
   - Offset: 15 units behind, 8 units above
2. Smoothly interpolate camera position using `Vector3Lerp(current, target, dt * 3.0f)`
3. Camera looks at car position (or slightly ahead)
4. Ensure camera Y never goes below terrain height + 2.0f

**Prompt**:
> Implement a chase camera: position it 15 units behind and 8 units above the car (relative to heading). Use `Vector3Lerp` for smooth following. Camera target is the car. Check terrain collision.

**Verification**: Run game. Camera should smoothly follow behind as you turn. No underground clipping.

---

### Stage 6: Collectible Objective

**What**: Spawn a target cylinder. Reaching it increments score.  
**Why**: Adds gameplay loop.  
**Size**: ~40 lines changed

**Requirements**:
1. Add to `GameState`: `Vector3 target_pos`, `std::int32_t score = 0`
2. Initialize `target_pos` to a random location on terrain
3. Each frame: if distance(car, target) < 5.0f, increment score and respawn target
4. Draw target as a yellow cylinder
5. Log `[GAME] COLLECTED! Score: <n>` when collected
6. Display score on screen

**Prompt**:
> Add a collectible target: yellow cylinder at random terrain position. When car reaches it (distance < 5), increment score, log `[GAME] COLLECTED! Score: <n>`, and respawn target elsewhere. Display score on screen.

**Verification**: Run game. Drive to yellow cylinder. Log shows collection message. Score increments.

---

## Summary Table

| Stage | Name | Lines Changed | Key Files |
|-------|------|--------------|-----------|
| 1 | Car Visual | ~10 | main.cpp |
| 2 | Heading Rotation | ~15 | main.cpp |
| 3 | Steering Input | ~20 | main.cpp |
| 4 | Arcade Speed | ~30 | main.cpp |
| 5 | Chase Camera | ~25 | main.cpp |
| 6 | Collectibles | ~40 | main.cpp |

**Total**: 6 small stages, ~125 lines of incremental changes

---

## Notes for LLM Developers

1. **One stage at a time**: Complete verification before proceeding
2. **Read the log**: The stdout log is your primary debugging tool
3. **Compile often**: `make clean && make` after each change
4. **Assert everything**: Follow CONTRIBUTING.md's safety rules
5. **Rollback on failure**: If a stage breaks, revert and retry with smaller steps
