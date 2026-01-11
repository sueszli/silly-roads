Based on tigerstyle.

Workflow: Write specification. Chunk into small tasks. Implement. Write tests. Refactor.

# (1) Safety

- No Undefined Behavior. Treat all compiler warnings as errors and resolve immediately.
- Assert Aggressively: Assert preconditions, postconditions, invariants, every return value.
- Bounded Execution: Set fixed upper limits on all loops.
- Controlled Memory: Strongly prefer static allocation over dynamic allocation. If dynamic allocation is necessary prefer arenas or memory pools.
- Simple flow: Avoid complex control flow (e.g., goto, nested if-else).
- Explicit Mutation: Avoid manipulating function arguments or causing side effects. If copying has an extremely high memory cost, mutation is allowed but the function must be named with a suffix (`_mut`) and mutation must be obvious in the naming (e.g., `out_result`).
- Testing: Achieve 100% coverage. Cover edge cases, boundary conditions and failure modes.

# (2) Performance

- Follow Data-Oriented Design (DoD) principles
      - Struct of Arrays (SoA): Prefer SoA over Array of Structs (AoS) for heavy computation to maximize SIMD usage.
- Design for Hardware: Organize data to match how the hardware reads it (cache lines).
- Batch Processing: Write functions that transform arrays of data rather than single elements (data transformation > object interaction).
- Existence-based Processing: Filter data *before* processing so loops run on contiguous, valid data (avoid `if (obj->active)` inside hot loops).

# (3) Experience

- Obvious Code > Clever Code
- Maximize Locality: Keep related code together. Define things near usage. Minimize variable scope.
- Centralize Control Flow: Branching logic belongs in parents. leaf functions should be pure logic.
- Guard Clauses: Handle checks first, return early, minimize nesting.
- Functions: Do one coherent thing (ideally <70 lines). Prefer lambdas/inline logic over tiny single-use functions.
- Decompose Conditionals: Use named variables to simplify complex `if` conditions.
- Comments: Comments explain *why*, not *what*. Use lowercase single lines. ASCII illustrations are welcome.
- Functional Style: Prefer pure functions (data in, data out) and immutability for logic.

# C++ Specific Notes

- Structs over Classes: Strictly separate data from logic. Use struct for pure data containers (PODs). Behavior belongs in stateless functions within namespaces, not member functions.
- No Constructors/Destructors in Data: Data structs should be aggregates. Initialize them using C++20 Designated Initializers (e.g., `Point p{ .x = 10, .y = 20 };`).
- Functional Core: Functions should be pure with no side effects. Isolate state mutation and I/O to the system boundaries.
- Immutability: Data is immutable by default. Create new modified instances rather than mutating existing ones whenever performance allows.
- RAII Everywhere: Never manually manage resources (new/delete, malloc/free, open/close). Wrap every resource (files, sockets, mutexes) in a RAII struct that cleans up in its destructor.
- Ownership = `std::unique_ptr`: Default to `std::unique_ptr` for heap allocations. It expresses clear, exclusive ownership.
- Shared Ownership is a Smell: Avoid `std::shared_ptr` unless ownership is truly cyclic or shared across threads with indeterminate lifetimes. It introduces atomic overhead and obscures object lifecycles.
- Raw Pointers are Views: Raw pointers (`T*`) are strictly for non-owning observation. They must never be deleted.
- No `new`/`delete`: Use `std::make_unique` or `std::make_shared`. Never use raw new or delete.
- Explicit Fixed-Width Types: Use `std::int32_t`, `std::uint64_t`, etc. Ban `int`, `long`, `short` and `unsigned`.
- Strict Const Correctness: Variables, pointers and methods are `const` by default. `constexpr` everything that can be computed at compile time.
- No C-Style Casts: Ban `(type)value`. Use `static_cast` (safe conversions), `reinterpret_cast` (system boundaries) or `std::bit_cast` (type punning).
- Null Safety: Prefer references (`T&`) for required values. Use `std::optional<T>` or `T*` only when "absence" is a valid logic state.
- Strong Typing: Do not use bool or primitive types for distinct concepts. Use enum class or strong typedefs to prevent argument swapping errors (e.g. `struct Width { std::int32_t v; };`).
- Return Values over Exceptions: For recoverable errors, return `std::expected<T, E>` (C++23) or `std::optional<T>`. Reserve exceptions for fatal, unrecoverable system states (e.g., Out of Memory).
- No Output Parameters: Functions return results. Return a struct or `std::pair/tuple` for multiple outputs. Never mutate arguments passed by pointer/reference.
- Ranges & Algorithms: Prefer `std::ranges` and `<algorithm>` (e.g., `std::ranges::transform`) over raw for loops. It reduces off-by-one errors and clearly expresses intent.
- String Views: Use `std::string_view` for read-only string arguments to avoid allocations.
- Concepts: Use C++20 concepts to constrain template parameters instead of SFINAE or `typename T`.
- Structured Binding: Use `auto [x, y] = point;` when unpacking structs to improve readability.
- Modern I/O: Use `fmt::print` (or C++23 `std::print`) exclusively. Ban `std::cout` and `<<`.
- Container Defaults: Default to `std::vector`. Avoid `std::list` (cache misses) or `std::map` (allocations) unless specific algorithmic complexity is required.
- Avoid `auto`: Explicitly write types to aid code reading, except for: Unspellable types (lambdas), Structured bindings, Extremely verbose iterator types where the type is obvious from context.
- Header Hygiene: `#include` exactly what you use. Use `#pragma once`. Forward declare types to minimize compilation dependency.
