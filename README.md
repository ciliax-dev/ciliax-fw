# ciliax-fw

## Project description

TODO

## Build instructions

The canonical build runs inside a digest-pinned Docker container so
the produced binary is reproducible across machines:

```sh
./scripts/build.sh
```

This bind-mounts the west workspace into the container, fetches the
manifest's modules with `west update`, and runs
`west build -b nrf5340dk/nrf5340/cpuapp app --build-dir build-docker
--pristine=auto`. The hex artifact lands at
`build-docker/app/zephyr/zephyr.hex` (sysbuild layout).

The container build uses a dedicated `build-docker/` so it doesn't
collide with a native `west build` that targets the default
`build/` — the two cache paths from incompatible filesystem views
(host paths vs the container's `/workdir/...`).

Prerequisites: Docker, and the workspace bootstrapped once with
`west init -l ciliax-fw` from the parent directory.

## Test instructions

Three test paths, in increasing order of runtime. The first two run
without an nRF5340 DK plugged in.

### 1. Host unit tests (no Zephyr)

Domain code (`app/src/domain/`) compiles on the host with vanilla
`g++` and is exercised by GoogleTest. This is the inner TDD loop —
edit-compile-run cycle in milliseconds.

```sh
cmake -S tests/host -B tests/host/build
cmake --build tests/host/build -j
ctest --test-dir tests/host/build --output-on-failure
```

Same tests under AddressSanitizer + UBSan (slower, catches more):

```sh
cmake -S tests/host -B tests/host/build-asan -DENABLE_SANITIZERS=ON
cmake --build tests/host/build-asan -j
ctest --test-dir tests/host/build-asan --output-on-failure
```

Prerequisites: a host C++20 compiler (`g++` 13+ or `clang++` 16+),
CMake 3.20+, and `libasan` / `libubsan` for the sanitizer build
(`apt install libasan8 libubsan1` on Debian/Ubuntu).

### 2. Integration tests on `native_sim`

ztests built as native Linux binaries via `west twister`. Verifies
real Zephyr integration without flashing.

```sh
west twister -T tests/integration -p native_sim --inline-logs
```

Prerequisites: an NCS workspace already bootstrapped (the same one
used for the firmware build).

### 3. Hardware-in-the-loop on the DK

Flash the binary and observe behavior on real silicon:

```sh
west build -b nrf5340dk/nrf5340/cpuapp app
west flash
```

Confirm via a serial terminal on `/dev/ttyACM*` at 115200 baud (or
`JLinkRTTClient` over RTT).

### Format and lint

```sh
# Check formatting (CI runs the same command):
clang-format --dry-run --Werror $(find app/src -name '*.cpp' -o -name '*.hpp')

# Lint domain sources via the host build's compile_commands.json:
cmake -S tests/host -B tests/host/build
clang-tidy -p tests/host/build app/src/domain/*.cpp
```

See [`docs/development.md`](docs/development.md) for the full TDD
workflow, the test pyramid, sanitizer notes, and the verification
block to run before declaring any task done.
