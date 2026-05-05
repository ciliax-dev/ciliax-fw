# C++ subset for ciliax-fw

The canonical list of language features we use, use carefully, and don't use. This is one half of the project's C++ contract; the other half is the architectural rules in [architecture.md](architecture.md). The reasoning behind these choices is in [background.md](background.md).

## Use freely

- `constexpr` / `consteval` for compile-time tables, prescalers, CRC tables, sine tables, MAC parsing — anything you'd otherwise express as macros or magic numbers
- `static_assert` for invariants the compiler can check
- `std::array<T, N>` instead of C arrays
- `std::span<T>` for non-owning views over buffers
- `std::optional<T>` instead of sentinel values like `-1` or `nullptr`
- `std::expected<T, E>` (or `tl::expected` for pre-C++23) instead of out-params with error codes
- ETL containers — `etl::vector<T, N>`, `etl::string<N>`, `etl::map<K, V, N>`, `etl::circular_buffer<T, N>`, `etl::delegate` — fixed-capacity, no allocation, the canonical replacement for the heap-using STL containers we don't link
- Strong types / user-defined literals (`auto baud = 115200_baud;`) — prevents the entire family of "I passed milliseconds where it wanted microseconds" bugs
- `enum class`, structured bindings, `auto`, range-`for`, lambdas (non-capturing compile to function pointers)
- RAII for hardware resources (lock guards, scoped IRQ disablers, peripheral handles)
- Templates for compile-time polymorphism (CRTP, policy classes)
- `[[nodiscard]]`, `[[maybe_unused]]`, `[[likely]]` / `[[unlikely]]`

## Use carefully

- **Virtual functions** — fine at module boundaries, avoid in ISRs and tight loops (vtable lookup, no inlining).
- **`std::function`** — type erasure plus heap usage for non-trivial captures. Use templates, function pointers, or `etl::delegate` in hot paths.

## Forbidden / not available

- **Exceptions.** `-fno-exceptions` is set. Use `std::expected` (or `tl::expected`) and error-code returns. Exceptions on a Cortex-M cost flash, RAM, and unwinding overhead you don't need.
- **RTTI** / `dynamic_cast` / `typeid` — typically off in NCS builds.
- **Heap allocation in steady state.** Initialization is fine; the main loop must not allocate.
- **`std::vector`, `std::string`, `std::map`, `<iostream>`, wall-clock `<chrono>`** — the full libstdc++ isn't linked (see "Standard and library" below). The link will fail if you try. Use ETL for containers; use Zephyr's `k_uptime_*` for time. Header-only STL (`std::array`, `std::span`, `std::optional`, `std::variant`, `std::expected`, `<type_traits>`, `<concepts>`, most of `<algorithm>`) works fine.
- **Globals with non-trivial constructors that depend on each other** — static-initialization-order fiasco.

## Standard and library

- **C++20 ceiling.** `CONFIG_STD_CPP20=y`. C++23 isn't yet supported by the NCS build system.
- **Minimal C++ runtime.** `CONFIG_REQUIRES_FULL_LIBCPP` is intentionally *not* set. This provides ABI bits — `new`/`delete`, static initialization, vtable support — without linking the heavy libstdc++ machinery. Header-only STL utilities (templates compiled into your code) work as normal. Heap-using containers (`std::vector`, `std::string`, `std::map`) and `<iostream>` won't link, by design: the **linker** enforces "no heap-using STL," not code review.

### How that's wired

Zephyr's `MINIMAL_LIBCPP` mode sets `-nostdinc++`, which removes both the toolchain's libstdc++ headers *and* its runtime. We want the headers without the runtime, so [`app/CMakeLists.txt`](../app/CMakeLists.txt) re-exposes the toolchain's `c++/<version>/` include directory as a `SYSTEM` include on the `app` target. The directory is located by asking the compiler for its target triple (`-dumpmachine`), so the glue keeps working across SDK versions and target archs.

Net effect:

- `<array>`, `<span>`, `<optional>`, `<expected>`, `<type_traits>`, `<concepts>`, most of `<algorithm>` — compile and link normally.
- `<vector>`, `<string>`, `<map>`, `<iostream>` — compile (the headers are present) but fail at link with undefined references to `std::__throw_*`, `operator new`, etc.

### ETL

`etl::vector<T, N>`, `etl::string<N>`, `etl::map<K, V, N>`, `etl::circular_buffer<T, N>`, `etl::delegate` are the canonical fixed-capacity replacements for the heap-using STL containers. ETL is part of the west manifest (pinned in [`west.yml`](../west.yml), checked out at `<workspace>/modules/lib/etl/`) and `app/CMakeLists.txt` puts its `include/` on the search path. Use `<etl/array.h>`, `<etl/string_view.h>`, etc.

## The litmus test

The **zero-overhead principle**: every feature should compile to code you couldn't reasonably hand-write better. If a feature breaks that, it's a code smell.

When in doubt: prefer compile-time over runtime, prefer `std::array<T, N>` over `T[N]`, prefer `std::span` over raw pointer + size.

## Zephyr integration constraints

- **File extension matters.** Zephyr picks the C++ compiler based on suffix (`.cpp`, `.cxx`).
- **C99 designated initializers don't compile in C++.** Some Zephyr headers (notably `<bluetooth.h>`) require them. Keep BLE setup in a `.c` file with a thin C-callable wrapper, and call it from C++.
- **No `std::thread` / `std::mutex`** out of the box. Use Zephyr primitives (`k_thread`, `k_mutex`, `k_sem`) directly or wrap them in your own thin RAII types.
- **`std::endl` flushes** — use `'\n'` in embedded code.
