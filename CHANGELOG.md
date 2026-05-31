# Changelog

All notable changes to this project are documented in this file.

## [Unreleased]

### Added

- **README** — v2 API docs (safe lookup, path API, throwing loads/dumps, typed accessors, `lua_val` factories); link to `CHANGELOG.md`; static analysis usage.
- **Static analysis** — `.clang-tidy`, `cmake/StaticAnalysis.cmake`, `cppcheck-suppressions.txt`, and Ubuntu CI job (cppcheck + clang-tidy).
- **Tests** — expanded `smallfolk_tests` / `smallfolk_schema_tests` coverage; fixed gvx/Lua Smallfolk wire interop fixtures in `test_lua_smallfolk_interop_wires()`.

### Fixed

- **Non-finite wire on glibc/libstdc++** — NaN/Inf values serialize as gvx tokens (`N`/`Q`/`I`/`i`) instead of libc `nan`/`inf` text that broke round-trip on Linux/macOS; parse `nan` literals and map `N`/`Q` via `std::nan`.
- **Schema depth-limit test** — avoid dangling `CompiledSchema` reference to a temporary nested schema (validation spuriously passed on GCC).

### Changed

- Restored pre-v2 API and serializer comments in `smallfolk.h` / `smallfolk.cpp`.
- README build instructions: fix `SMALLFOLK_BUILD_TESTS` cmake typo.

## [2.0.0] - 2026-05-31

### Added

- **CMake library target** (`smallfolk_cpp::smallfolk`), install/export, CTest, benchmark, and multi-platform CI (Linux, macOS, Windows).
- **`ASSUMPTIONS.md`** — documented copy semantics, comparison, limits, threading, and security expectations.
- **`LoadLimits`** — configurable deserialization bounds (`max_input_size`, `max_string_length`, `max_nesting_depth`, `max_value_count`, `max_table_entries`, `require_consumed_input`, `reject_non_finite_numbers`).
- **`LuaVal::untrusted_load_limits()`** — tighter defaults for untrusted input.
- **Thread-safe** `LuaVal::get_load_limits()` / `set_load_limits()` (prefer per-call limits in servers).
- **Safe lookup API** — `try_get`, `find`, `at`, `has` (no auto-vivification).
- **Path API** — `try_get_path`, `get_path`, `at_path`, `has_path`, `set_path`, `erase_path` (variadic and `initializer_list` overloads).
- **Typed accessors** — `try_as_number`, `try_as_string`, `try_as_bool`.
- **Throwing API** — `loads_or_throw`, `dumps_or_throw`.
- **Aliases** — `merge`/`mrg`, `erase`/`rem`.
- **`smallfolk_convert.h` / `namespace lua_val`** — STL-friendly factories (`map`, `array`, etc.).
- **Schema validation** (`smallfolk_schema.h`) — `Schema`, `CompiledSchema`, `validate`, `loads_validated`, `ValidateLimits`.
- **`namespace schema`** presets — `number_range`, `string_length`, `string_enum`, `array_of`, `map_of`, `one_of`, `value()`, and more.
- **Interactive demo** restored as `examples/demo.cpp` (`smallfolk_demo` CTest target) with Release-safe `DEMO_CHECK` checks.

### Changed

- Parser/serializer fixes (non-finite numbers, quoted strings, whitespace, trailing input rejection).
- **`schema::array_of` / `map_of` / `one_of` / `string_enum`** return self-contained owned schemas (no global factory deque).
- Schema factories take **`Schema` by value** where appropriate (move-friendly).

### Removed

- **`schema::one_of(Schema const * const *, size_t)`** — use `std::vector<Schema>`, `std::initializer_list<Schema>`, or braced alternatives instead.

### Notes

- Table **`@` circular references** remain unsupported (deep copy on assign; `@` ignored on load).
- **`operator==` for tables** compares internal pointer identity, not structural equality.
- **`LuaVal` mutation is not thread-safe** — treat parsed values as immutable when shared across threads.
- See [ASSUMPTIONS.md](ASSUMPTIONS.md) for full behavioral contract.

## [1.0.0] - 2022-10-13

Initial public release: `LuaVal` type, Smallfolk wire-format serialize/deserialize, header + single `.cpp` integration.
