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

TODO
