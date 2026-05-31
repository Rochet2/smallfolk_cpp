# Assumptions

This document records behavioral assumptions baked into smallfolk_cpp. If you rely on different semantics, adjust your usage or fork the library.

## Format and interoperability

- Serialized output follows the [gvx/Smallfolk](https://github.com/gvx/Smallfolk) format closely enough to exchange data with compatible Lua serializers, but **not every Smallfolk feature is supported**.
- **Circular table references (`@` notation) are not supported.** Serialization does not emit `@` references; deserialization ignores them (treated as invalid input unless future support is added). In C++, assigning a table into itself always produces a **deep copy**, never a shared reference.
- Numbers are stored internally as `double`. Integer values outside exact `double` range may lose precision on round-trip.
- Non-finite floats use Smallfolk's single-letter encodings (`I`, `i`, `N`, `Q`) rather than JSON-style `Infinity`/`NaN`.
- String keys and values use `"` or `'` quoting; embedded quotes are doubled. There is no `\` escape syntax.
- Whitespace between tokens is limited to space and tab. Other whitespace (newlines, `\r`) is not skipped unless explicitly present in a string literal.
- Table keys that are positive integers with no fractional part serialize as array elements when consecutive from `1`. Gaps or non-integer numeric keys use explicit `key:value` form.
- **`loads()` assumes the input is trusted only to the extent configured by `LoadLimits`.** Default limits cap input size, nesting depth, value count, and per-string length. Trailing garbage after a valid value is rejected by default.

## Memory and copying

- **`LuaVal` tables are value types.** Copy construction and copy assignment deep-copy the entire subtree.
- `operator[]` on a missing key **inserts an empty table** (Lua-like auto-vivification). This can leave stray empty entries; prefer `get()`, `has()`, and `set()` when you do not want that behavior.
- `set(key, nil)` removes the key; `table[key] = nil` stores an explicit nil entry instead.
- Table keys stored in a table are themselves deep-copied on insert. You cannot retrieve the same key object later by identity.
- Move assignment transfers ownership of internal table storage without copying. Move-based `set()` / `insert()` overloads avoid redundant deep copies when you no longer need the source value.

## Comparison and hashing

- **`operator==` for tables compares internal pointer identity**, not structural equality. Two tables with identical contents are unequal unless they share the same underlying storage (typically never, after copying).
- `LuaValHash` / `std::hash<LuaVal>` for tables hashes the internal table pointer, not contents. Do not use these hashes for content-based deduplication.
- For structural comparison, compare serialized form or implement your own deep equality (not provided by default).

## Error handling

- Most `LuaVal` accessors throw `smallfolk_exception` on type misuse (for example `str()` on a number).
- `dumps()` and `loads()` catch `smallfolk_exception` internally and return failure sentinels (empty string / nil) with an optional `errmsg` out-parameter. They do not rethrow.
- `smallfolk_exception::what()` returns a pointer into internal storage valid for the lifetime of the exception object.

## Locale and platform

- Number parsing uses the `"C"` locale via `std::strtod` to avoid locale-dependent decimal separators.
- `sprintf` / `snprintf` formatting for number output uses `% .17g` (Lua-minimum style precision for finite values).

## Security

- Deserialization is **not hardened against malicious input beyond configurable limits**. Even with defaults, extremely large but valid payloads can consume memory proportional to parsed content. Tune `LoadLimits` for your threat model.
- There is no schema validation: any parseable Smallfolk value is accepted.

## API design notes (intentional, not bugs)

- **`operator[]` auto-vivification** mirrors Lua ergonomics at the cost of silent table growth. Consider `get()` + `set()` for map-like usage, or a future `at(key)` that throws or returns optional when missing.
- **Pointer-identity table equality** keeps `operator==` O(1) and matches typical use as a dynamic value container, not a persistent functional data structure. Consider a free function `equiv(a, b)` for deep structural comparison if needed.
