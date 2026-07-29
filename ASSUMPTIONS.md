# Assumptions

This document records behavioral assumptions baked into smallfolk_cpp. If you rely on different semantics, adjust your usage or fork the library.

## Format and interoperability

- Serialized output follows the [gvx/Smallfolk](https://github.com/gvx/Smallfolk) format closely enough to exchange data with compatible Lua serializers, but **not every Smallfolk feature is supported**.
- **Circular table references (`@` notation) are not supported.** Serialization does not emit `@` references; deserialization ignores them (treated as invalid input unless future support is added). In C++, assigning a table into itself always produces a **deep copy**, never a shared reference.
- Numbers are stored internally as `double`. Integer values outside exact `double` range may lose precision on round-trip.
- Non-finite floats use Smallfolk's single-letter encodings (`I`, `i`, `N`, `Q`) rather than JSON-style `Infinity`/`NaN`. Set `LoadLimits::reject_non_finite_numbers` to reject these during `loads()`.
- String keys and values use `"` or `'` quoting; embedded quotes are doubled. There is no `\` escape syntax.
- Whitespace between tokens is limited to space and tab at value boundaries. Between a table key and `:` / `,` / `}`, only spaces are skipped (not tabs), to keep the hot path tight.
- Table keys that are positive integers with no fractional part serialize as array elements when consecutive from `1`. Gaps or non-integer numeric keys use explicit `key:value` form.
- **`loads()` assumes the input is trusted only to the extent configured by `LoadLimits`.** Default limits cap input size, nesting depth, value count, per-table entry count, and per-string length. Trailing garbage after a valid value is rejected by default.

## Memory and copying

- **`LuaVal` tables are value types.** Copy construction and copy assignment deep-copy the entire subtree.
- `operator[]` on a missing key **inserts an empty table** (Lua-like auto-vivification). This can leave stray empty entries; prefer `get()`, `has()`, and `set()` when you do not want that behavior.
- `set(key, nil)` removes the key; `table[key] = nil` stores an explicit nil entry instead.
- Table keys stored in a table are themselves deep-copied on insert. You cannot retrieve the same key object later by identity.
- Move assignment transfers ownership of internal table storage without copying. Move-based `set()` / `insert()` overloads avoid redundant deep copies when you no longer need the source value.
- **`try_get_path` / `get_path` / `at_path` / `has_path`** traverse nested tables read-only and never auto-vivify. An empty path refers to the root value. `set_path` auto-vivifies missing intermediate tables (like `operator[]`). `erase_path` is a no-op when any segment of the path is missing.

## Comparison and hashing

- **`operator==` for tables compares internal pointer identity**, not structural equality. Two tables with identical contents are unequal unless they share the same underlying storage (typically never, after copying).
- `LuaValHash` / `std::hash<LuaVal>` for tables hashes the internal table pointer, not contents. Do not use these hashes for content-based deduplication.
- For structural comparison, compare serialized form or implement your own deep equality (not provided by default).

## Error handling

- Most `LuaVal` accessors throw `smallfolk_exception` on type misuse (for example `str()` on a number).
- `dumps()` and `loads()` catch `smallfolk_exception` internally and return failure sentinels (empty string / nil) with an optional `errmsg` out-parameter. They do not rethrow. On failure, `errmsg` is **assigned** (not appended).
- `smallfolk_exception::what()` returns a pointer into internal storage valid for the lifetime of the exception object.

## Thread safety

- **`LuaVal::loads(input, limits, &err)` is safe to call concurrently** when each thread uses its own output `LuaVal`, error string, and (preferably) explicit `LoadLimits`.
- **`LuaVal::set_load_limits()` / `get_load_limits()` are synchronized** with a mutex. Prefer passing `LoadLimits` per call in multi-threaded servers instead of relying on process-wide defaults.
- **`LuaVal` mutation is not thread-safe.** Do not share a mutable `LuaVal` across threads without external locking. Treat parsed values as immutable when cached.
- **`CompiledSchema` is immutable after construction** and safe to share read-only across threads.
- **`schema::` preset references** (`number()`, `value()`, etc.) are safe to share after process startup.
- **`schema::array_of()` / `map_of()` / `one_of()`** return self-contained `Schema` values (owned child schemas; move-friendly).
- **`schema::string_enum()`** returns a self-contained `Schema` that owns its allowed strings.
- Internal cyclic presets such as **`schema::value()`** remain static and use external child pointers.
- **`validate(value, Schema)` recompiles on every call** — thread-safe but slow; prefer a shared `CompiledSchema`.

## Locale and platform

- Number parsing uses the `"C"` locale via `std::strtod` to avoid locale-dependent decimal separators.
- Number output uses `std::snprintf` with `%.17g` (Lua-minimum style precision for finite values).

## Security

- Deserialization is **not hardened against malicious input beyond configurable limits**. Even with defaults, large valid payloads can consume memory proportional to parsed content. Use `LuaVal::untrusted_load_limits()` as a starting point for user input.
- **`max_value_count`** limits the total number of parsed scalar/table values in a document.
- **`max_table_entries`** limits key/value pairs in a single table (0 disables the check).
- **Schema validation** (`smallfolk_schema.h`) is a separate layer with its own **`ValidateLimits`** (depth and step budgets). Use `untrusted_validate_limits()` alongside untrusted load limits.
- **`OneOf` validation skips branches whose scalar kind cannot match** the value tag (null/bool/number/string), reducing CPU on unions like `schema::value()`.
- There is no validation-time wall clock timeout; cap work with `ValidateLimits::max_validation_steps`.

## API design notes (intentional, not bugs)

- **`operator[]` auto-vivification** mirrors Lua ergonomics at the cost of silent table growth. Consider `get()` + `set()` for map-like usage, or `at(key)` which throws when missing.
- **Pointer-identity table equality** keeps `operator==` O(1) and matches typical use as a dynamic value container, not a persistent functional data structure. Consider a free function `equiv(a, b)` for deep structural comparison if needed.
