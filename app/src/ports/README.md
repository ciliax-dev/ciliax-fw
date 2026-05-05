# `ports/`

Abstract interfaces — the seams that make the rest of the system testable.

**Rule:** files here **never** `#include <zephyr/...>` or any Nordic header.
Headers in this directory should be header-only and pure-virtual: an
interface, a virtual destructor, deleted copy/move, and the abstract methods
the domain needs.

Every external dependency the domain talks to (clock, GPIO, BLE, network,
sensor, classifier, non-volatile store, ...) is reached through a port
defined here. Each port has at minimum two implementations:

- [`adapters/zephyr/`](../adapters/zephyr/) — production, Zephyr/Nordic-backed
- [`adapters/mock/`](../adapters/mock/) — test double for host unit tests

If domain code wants to "reach into Zephyr internals just for this one thing,"
the port is missing — add it here.
