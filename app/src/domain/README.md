# `domain/`

Pure C++ application logic. Knows nothing about Zephyr, Nordic, or any
specific chip.

**Rule:** files in this directory **never** `#include <zephyr/...>` or any
Nordic header. They depend only on:

- the C++ standard library (header-only utilities — see
  [`docs/cpp_subset.md`](../../../docs/cpp_subset.md))
- ETL (`<etl/...>`) for fixed-capacity containers
- abstract interfaces from [`ports/`](../ports/)

Domain code compiles with vanilla `g++` and is exercised by host unit tests
under [`tests/host/`](../../../tests/host/) — fast, deterministic, run under
sanitizers in CI. If a piece of logic is hard to test on the host, the
architecture is wrong, not the test.

The composition root ([`main.cpp`](../main.cpp)) is the only place where
domain types meet concrete adapters.
