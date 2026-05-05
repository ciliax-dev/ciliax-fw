# `adapters/mock/`

Test doubles for [`ports/`](../../ports/) interfaces. Used **only** by host
unit tests under [`tests/host/`](../../../../tests/host/).

**Rule:** files here **never** `#include <zephyr/...>` or any Nordic header.
Mocks must compile with vanilla `g++` so that the host test binary stays
Zephyr-free and the test loop stays fast.

A typical mock records call counts and stores synthetic state, so that tests
can assert behavior on the domain object that was driving the port. Keep
mocks dumb — no logic worth testing belongs here.
