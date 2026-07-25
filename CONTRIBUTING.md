# Contributing

Thank you for your interest in MarketCapture.

Before opening a pull request:

- Use C++20 and keep the public API under `include/marketcapture`.
- Build with warnings enabled and add tests for success and malformed-input paths.
- Run `ctest --test-dir build -C Release --output-on-failure`.
- Update documentation and the changelog for user-visible behavior.
- Include before/after measurements for performance-critical changes.

Protocol parsers must validate message lengths before reading and must not retain
views beyond the lifetime of the input datagram. The SPSC queue is intentionally
single-producer/single-consumer; do not broaden that contract without a separate
design and benchmark.
