#!/usr/bin/env bash
# Reproducible firmware build for ciliax-fw inside a digest-pinned
# Zephyr CI container. The image is the official zephyrproject-rtos
# CI image, which carries the Zephyr SDK toolchain (arm-zephyr-eabi),
# CMake, Ninja, west, and the Python deps. Nordic-specific modules
# are fetched at build time per the pinned west.yml manifest.
set -euo pipefail

IMAGE="zephyrprojectrtos/ci@sha256:db4d0467fc2782b35135653754a990d7302c7c1ea7540abaf33e975c812a9ddd"

PROJECT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
WORKSPACE_DIR="$(dirname "$PROJECT_DIR")"
PROJECT_NAME="$(basename "$PROJECT_DIR")"

if [[ ! -d "$WORKSPACE_DIR/.west" ]]; then
  echo "error: $WORKSPACE_DIR is not a west workspace (no .west/ found)" >&2
  echo "from $WORKSPACE_DIR, run: west init -l $PROJECT_NAME" >&2
  exit 1
fi

exec docker run --rm \
  -u "$(id -u):$(id -g)" \
  -e HOME=/tmp \
  -e ZEPHYR_SDK_INSTALL_DIR=/opt/toolchains/zephyr-sdk-0.17.4 \
  -v "$WORKSPACE_DIR:/workdir" \
  -w "/workdir/$PROJECT_NAME" \
  "$IMAGE" \
  bash -c "west update && west build -b nrf5340dk/nrf5340/cpuapp app --build-dir build-docker --pristine=auto"
# --build-dir is intentional: native 'west build' uses the default build/,
# the container build uses build-docker/. Each side caches paths from its
# own filesystem view (host paths vs the container's /workdir/...), so a
# shared dir would force a pristine rebuild on every context switch and
# silently feed the other side stale paths in compile_commands.json.
# Both directories are covered by the .gitignore 'build*/' pattern.
