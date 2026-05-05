# `adapters/zephyr/`

Concrete implementations of [`ports/`](../../ports/) interfaces, built on top
of Zephyr / Nordic / nrfx APIs.

**Rule:** this is the **only** place under `app/src/` (besides
[`main.cpp`](../../main.cpp)) where `#include <zephyr/...>` is allowed.

Adapters here typically `final`-implement a port and wrap a `gpio_dt_spec`,
a Zephyr driver handle, an nrfx peripheral, etc. Composition (which adapter
is wired into which domain object) lives in `main.cpp`, not here.

These files are not exercised by host unit tests — they only build inside the
Zephyr build system. They are exercised by `native_sim` integration tests
under [`tests/integration/`](../../../../tests/integration/) and by running
on real hardware.
