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
`west build -b nrf5340dk/nrf5340/cpuapp app --pristine=auto`. The hex
artifact lands at `build/app/zephyr/zephyr.hex` (sysbuild layout).

Prerequisites: Docker, and the workspace bootstrapped once with
`west init -l ciliax-fw` from the parent directory.

## Test instructions

TODO
