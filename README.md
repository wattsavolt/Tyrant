# Tyrant
Work-in-progress game engine. Not currently fit for use.

## Coding Rules

### Memory & containers
- Allocate up front or in large blocks. Avoid heap allocation in steady-state/per-frame paths, even in editor/import-time code that isn't hot-path - the bar is lower there, but still avoid it when a pattern below fits cleanly.
- For temporary/reusable data, in priority order: a `LocalArray<T, N>` with a sane "happy max"; a reusable member on a long-lived owner (`Array`/`HashMap` cleared, not freed, at the start of each use); the stack allocator (`StackAlloc`/`SmartStack<T>`), strict LIFO, simple struct types only. Avoid a fresh function-local heap-backed `Array`/`HashMap` when one of these fits.
- Use `Array`, `LocalArray`, `HashMap` - not STL containers.
- Use `LocalObjectPool`/`ObjectPool` wherever objects can be pooled. Only create/destroy pooled objects on the thread that owns the pool; once created, they can be read from other threads safely.
- Don't reach for chunking/pooling machinery pre-emptively - start with the simple container, add complexity once there's an actual measured or clearly-foreseeable need.
- Render graph data (`RGArray` etc.) allocates on the render graph's own allocator during graph construction, unless it fits on the stack or needs to outlive a frame. `TempAllocator` is for data that needs to live a few frames (used in `AssetManager`).

### Language features
- RTTI is off project-wide (`/GR-`). No `dynamic_cast`/`typeid` - use `Core/Reflection` instead.
- `std::variant`/`std::visit`: fine for import-time/editor-only code, avoid in runtime (hot-path/per-frame) code.
- Avoid `auto` unless there's no explicit-type way to spell it (e.g. a generic lambda visiting a `std::visit`).
- Prefer an anonymous `namespace { }` for helper functions private to one `.cpp`, over a plain `namespace tyr { }` function.
- `Function<Ret(Args...)>` (the type task bodies and similar callbacks are built from) is fixed-size (32 bytes) and never heap-allocates - keep captures to one pointer-sized capture where possible, and never capture anything that isn't trivially copyable (a raw pointer/reference/POD is fine; a `String`, `Array`, or smart pointer is not - it's enforced by a `static_assert`).

### Threading & tasks
- `TaskScheduler::Instance()` owns a fixed pool of worker threads, each pulling work from its own lock-free work-stealing queue and stealing from siblings (falling back to a shared injector queue) when its own is empty.
- Create tasks via `TaskScheduler::CreateTask`/`CreateAndEnqueueTask` from any thread, including from inside a running task - a task created while another task is running automatically becomes its child, and the parent isn't considered finished until all such children finish too.
- `AddDependency(task, dependency)` must be called before `Enqueue(task)` - dependencies can't be added safely once a task might already be running.
- Graphics API wrapper objects are pooled and only ever created on the main thread. All render allocations go through `RenderAllocationManager`, also main-thread only; timeline semaphores signal when it's safe to reclaim them.
