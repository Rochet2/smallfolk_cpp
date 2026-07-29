# smallfolk_cpp

Smallfolk_cpp is a library for representing `Lua` values in `C++` and (de)serializing them. The serialization is made to work with smallfolk serializer made for lua. Most serializer logic is borrowed from gvx/Smallfolk. [https://github.com/gvx/Smallfolk](https://github.com/gvx/Smallfolk)

Smallfolk_cpp does not have dependencies other than `C++11` and it **does not need lua**. It simply uses same format and logic as gvx/Smallfolk for serialization.

Smallfolk_cpp has its own type `LuaVal` to represent `Lua` values in `C++`.
They allow representing bool, number, string, nil and table.

Due to implementation difficulties and security some features of gvx/Smallfolk are not supported. A version of smallfolk for lua with the unsupported features removed can be found at [https://github.com/Rochet2/Smallfolk](https://github.com/Rochet2/Smallfolk)

You use, distribute and extend Smallfolk_cpp under the terms of the MIT license.

See [ASSUMPTIONS.md](ASSUMPTIONS.md) for documented behavioral assumptions (copy semantics, comparison, limits, and security).

See [CHANGELOG.md](CHANGELOG.md) for release history (current version **2.0.1**).

## Add to your project

**CMake (recommended)** — add this repository as a subdirectory or fetch it, then link the library target:

```cmake
add_subdirectory(path/to/smallfolk_cpp)

add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE smallfolk_cpp::smallfolk)
```

Build and test from the repository root:

```bash
cmake -B build -DSMALLFOLK_BUILD_TESTS=ON -DSMALLFOLK_BUILD_BENCHMARK=ON
cmake --build build
ctest --test-dir build --output-on-failure   # if you enable CTest
./build/smallfolk_tests
./build/smallfolk_schema_tests
./build/smallfolk_demo
./build/smallfolk_benchmark
```

**Manual integration** — copy `smallfolk.h`, `smallfolk.cpp`, and (optionally) `smallfolk_schema.h`, `smallfolk_schema.cpp`, `smallfolk_convert.h` into your tree and compile them as a static library or directly into your target. The library requires C++11 and has no other dependencies.

**Install** — after building:

```bash
cmake --install build --prefix /path/to/prefix
```

This installs `smallfolk.h`, `smallfolk_convert.h`, `smallfolk_schema.h`, the `smallfolk` library, and CMake package files under `lib/cmake/smallfolk_cpp/` so consumers can use `find_package`:

```cmake
find_package(smallfolk_cpp REQUIRED)
target_link_libraries(my_app PRIVATE smallfolk_cpp::smallfolk)
```

## Usage

```C++
#include "smallfolk.h"

// create a lua table and set some values to it
LuaVal table = LuaVal::table();
table[1] = "Hello"; // the value is automatically converted to LuaVal
table["test"] = "world";
table[67.5] = -234.5;

// serialize the table
std::string serialized = table.dumps();

// print the serialization, it should be rather human readable
// Example output: {"Hello","test":"world",67.5:-234.5}
std::cout << serialized << std::endl;

// form lua values from the string
LuaVal deserialized = LuaVal::loads(serialized);

// print the values from deserialized result table
// Example output: Hello world -234.5
std::cout << deserialized[1].str() << " ";
std::cout << deserialized["test"].str() << " ";
std::cout << deserialized[67.5].num() << std::endl;
```

## Fast

Run the included benchmark to measure your machine (build with `-DSMALLFOLK_BUILD_BENCHMARK=ON`):

```bash
./build/smallfolk_benchmark
```

Example output on a typical desktop (10,000 iterations, sample payload shown in the benchmark):

```
serialize avg:   ~0.03 ms
deserialize avg: ~0.05 ms
round-trip avg:  ~0.12 ms
```

Older informal measurements reported ~0.01 ms serialize and ~0.05 ms deserialize for small payloads. Results vary widely by data shape, allocator, and compiler.

The benchmark serializes this sample payload:

```lua
{t,"somestring",123.456,t,{"t":-678,"test":123.45600128173828,"f":268435455,"subtable":{1,2,3}}}
```

## Table cycles

**Note: This feature was disabled because of difficult implementing in C++ and possibly unwanted infinite cycles. All table assigning creates deep copies now in the C++ code and no `@` notation is recognized for serializing or deserializing. Input containing `@` references is rejected on load. Assigning a table into itself (or as a key) always deep-copies; there are no shared cycles.**

From original smallfolk

> Sometimes you have strange, non-euclidean geometries in your table
> constructions. It happens, I don't judge. Smallfolk can deal with that, where
> some other serialization libraries (or anything that produces JSON) cry "Iä!
> Iä! Cthulhu fhtagn!" and give up &mdash; or worse, silently produce incorrect
> data.
>
> ```C++
> #include smallfolk.h
>
> // Essentially {{},{},{}}
> LuaVal cthulhu(TTABLE);
> cthulhu[1] = LuaVal(TTABLE);
> cthulhu[2] = LuaVal(TTABLE);
> cthulhu[3] = LuaVal(TTABLE);
> cthulhu["fhtagn"] = cthulhu;
> cthulhu[1][cthulhu[2]] = cthulhu[3];
> cthulhu[2][cthulhu[1]] = cthulhu[2];
> cthulhu[3][cthulhu[3]] = cthulhu;
> std::cout << cthulhu.dumps() << std::endl;
> // prints:
> // {"fhtagn":@1,1:{{@2:@3}:{@4:@1}},2:@3,3:@4}
> ```

## Security

Deserialization accepts configurable limits via `LoadLimits` (see reference below). Defaults cap total input size, per-string length, nesting depth, and total parsed value count. Trailing input after a valid value is rejected by default.

Tune limits for your deployment. See [ASSUMPTIONS.md](ASSUMPTIONS.md) for what is and is not guaranteed.

## Tested

Automated tests live in `tests/test_smallfolk.cpp` and `tests/test_schema.cpp`, run via the `smallfolk_tests` and `smallfolk_schema_tests` targets. The original interactive walkthrough from `main.cpp` now lives in `examples/demo.cpp` as the `smallfolk_demo` target (also run by CTest when `-DSMALLFOLK_BUILD_EXAMPLES=ON`, default). The demo uses a `DEMO_CHECK` macro instead of `assert()` so runtime verification still runs in Release/NDEBUG builds.

Static analysis (Linux CI and local when tools are installed):

```bash
cmake -B build -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build --target smallfolk_static_analysis
```

This runs **cppcheck** and **clang-tidy** on the library sources when available. Optional: `-DSMALLFOLK_ENABLE_CLANG_TIDY=ON` attaches clang-tidy to normal library builds.

The code has also been in use with a server-client C++-Lua communication system called AIO through which the API has been made more usable and critical issues have been addressed.

- [https://github.com/Rochet2/AIO](https://github.com/Rochet2/AIO)
- [https://github.com/Rochet2/TrinityCore/tree/c_aio](https://github.com/Rochet2/TrinityCore/tree/c_aio)
- [https://github.com/SaiFi0102/TrinityCore/tree/CAIO-3.3.5](https://github.com/SaiFi0102/TrinityCore/tree/CAIO-3.3.5)

## Reference

### try-catch

Most functions can throw `smallfolk_exception` and some string library errors and possibly more.
One method for try catching errors you can use is this:

```C++
try {
  // smallfolk_cpp code
}
catch (smallfolk_exception& e) {
    std::cout << e.what() << std::endl;
}
```

You need to catch exceptions mostly from incorrect handling of LuaVal. For example trying to access a number like a table will cause an exception.

### serializing

Serializing happens by calling the member function `std::string LuaVal::dumps(std::string* errmsg = nullptr)`. When an error occurs with the serialization an empty string is returned and if errmsg points to a string then it is filled with the error message.
This function does not throw.

### deserializing

Deserializing happens by calling `static LuaVal LuaVal::loads(std::string const & string, std::string* errmsg = nullptr)` or the overload that accepts an explicit `LoadLimits` object. When an error occurs with the deserialization a LuaVal representing a nil is returned and if errmsg points to a string then it is filled with the error message.
This function does not throw. Use `LuaVal::loads_or_throw(...)` when you prefer exceptions on parse failure.

Serializing also has a throwing overload: `dumps_or_throw()`.

### LoadLimits

Configure deserialization bounds with `LoadLimits`. `LuaVal::set_load_limits()` and `get_load_limits()` are thread-safe, but **in multi-threaded servers prefer passing explicit limits per call** rather than mutating process-wide defaults at runtime.

For untrusted user input, start from `LuaVal::untrusted_load_limits()` and tune from there:

```C++
LoadLimits limits = LuaVal::untrusted_load_limits();
limits.max_input_size = 128 * 1024;

std::string err;
LuaVal value = LuaVal::loads(payload, limits, &err);
```

Fields:


| Field                      | Default | Purpose                                              |
| -------------------------- | ------- | ---------------------------------------------------- |
| `max_input_size`           | 16 MiB  | Reject inputs larger than this                       |
| `max_string_length`        | 1 MiB   | Reject quoted string contents longer than this       |
| `max_nesting_depth`        | 256     | Reject tables nested deeper than this                |
| `max_value_count`          | 100000  | Reject documents with more parsed values (total)     |
| `max_table_entries`        | 100000  | Reject tables with more entries (0 = disable check)  |
| `require_consumed_input`   | `true`  | Reject trailing bytes after the root value           |
| `reject_non_finite_numbers`| `false` | Reject `I`/`i`/`N`/`Q` non-finite encodings          |

### Thread safety

| API | Concurrent use |
| --- | -------------- |
| `loads(input, limits, &err)` | Safe when each thread has its own `LuaVal` / error string; pass explicit `LoadLimits` |
| `set_load_limits()` / `get_load_limits()` | Synchronized; avoid runtime tuning from many threads |
| Mutable `LuaVal` | **Not thread-safe** — treat parsed values as immutable when shared |
| `CompiledSchema` | Immutable after construction; safe to share read-only |
| `schema::number()` and other presets | Safe after startup |
| `schema::array_of()` / `map_of()` / `one_of()` / `string_enum()` | Self-contained `Schema` values; store in `static Schema const` when taking `&schema` for fields |
| `validate(value, Schema)` | Thread-safe but recompiles each call — use `CompiledSchema` instead |

See `ASSUMPTIONS.md` for full threading and security notes.


### Schema validation

Include `smallfolk_schema.h` to validate parsed `LuaVal` trees against a declarative schema. Parsing limits (`LoadLimits`) and schema validation are separate layers.

```C++
#include "smallfolk_schema.h"

static Schema::Field player_fields[] = {
    { "name", &schema::string(), true },
    { "hp", &schema::number(), true },
};

static Schema const player_schema = {
    SchemaKind::Object,
    nullptr, 0, static_cast<unsigned>(-1),
    player_fields, 2, false
};

// Build once, share across threads (recommended for servers).
static CompiledSchema const compiled(player_schema);

LoadLimits load_limits = LuaVal::untrusted_load_limits();
ValidateLimits validate_limits = untrusted_validate_limits();
std::string err;
LuaVal player = loads_validated(payload, compiled, load_limits, validate_limits, &err);
```

Supported schema features:

- Kinds: `Any`, `Null`, `Bool`, `Number`, `String`, `Array`, `Object`, `OneOf`
- Built-in presets in `namespace schema`: `number()`, `string()`, `array()`, `object()`, `value()` (recursive JSON-like), `number_range()`, `string_length()`, `array_of()`, `map_of()`, `string_enum()`, `one_of()`, and more
- Number min/max bounds
- String min/max length (`string_length(min, max)`)
- Array min/max length and per-element schema
- Object required fields and `allow_extra_keys`
- `enum_strings` on `Schema` for string enums (owned by the schema; use `schema::string_enum({...})`)
- `alternatives` for `OneOf` unions
- `validator` callback for custom checks
- `ValidateLimits` depth/step budgets (`untrusted_validate_limits()`)
- `CompiledSchema` for fast object field lookup (build once, validate many times)

### LuaVal

LuaVal is a type used to represent lua values in C++. LuaVal has a range of functions to access the underlying values and to construct LuaVal from different values. LuaVal is the input for serialization and output of deserialization.

### LuaVal constructors

Constructors allow implicitly constructing values.
Constructors do not throw. Watch out for quirks with initializer list constructor: [http://stackoverflow.com/questions/26947704/implicit-conversion-failure-from-initializer-list](http://stackoverflow.com/questions/26947704/implicit-conversion-failure-from-initializer-list)

```C++
LuaVal implicit_test = -123;
LuaVal copy_test(implicit_test);
LuaVal copy_test2 = implicit_test;
LuaVal n = LuaVal::nil; // nil
LuaVal n2(TNIL); // nil
LuaVal b(true);
LuaVal s("a string");
LuaVal d(123.456);
LuaVal f(123.456f);
LuaVal i(-678);
LuaVal i64(static_cast<int64_t>(9007199254740991LL));
LuaVal u(0xFFFFFFF);
LuaVal t; // defaults to table
LuaVal t2 = LuaVal::table();
LuaVal t3 = { 1, 2, { 1,2,3 } };
LuaVal t4 = {};
LuaVal t5(TTABLE);
LuaVal t6 = LuaVal::LuaTable{ { "key", "value" }, { 2, "value2" } }; // Table can be created with map table initializer list constructor also

// You can mix and match a lot of different types and containers for creating tables.
// For example vectors, lists, maps, arrays are supported for creating LuaVal.
std::vector<std::list<std::string>> vec = {{"a", "b"},{"a", "b"}};
LuaVal t5 = {1,2, "test", vec};
// Resulting table: {1,2,"test",{{"a","b"},{"a","b"}}}
```

Creating sequences is easy, but creating complex tables that contain different types of values can be difficult or take a lot of space in code. To avoid quirks and for convenience you can deserialize strings to create values in a compact way. Here two equivalent values are created with normal style and deserialization:

```c++
LuaVal val1 = { 1,2, LuaVal::mrg({3,4.5}, LuaVal::LuaTable({{"ke","test"}})) };
LuaVal val2 = LuaVal::loads("{1,2,{3,4.5,'ke':'test'}}");
```

### static nil

A static value `static const LuaVal LuaVal::nil` is a preconstructed nil object.
It can be used as a default value or return value when a const nil value reference is needed to avoid constructing unnecessary copies.

### hash

The LuaVal class contains a hasher `LuaVal::LuaValHasher`. You need to use it when you use a LuaVal in a hash container for example: `std::unordered_set<LuaVal, LuaVal::LuaValHasher> myset;` or `std::unordered_map<LuaVal, int, LuaVal::LuaValHasher> mymap;`.
Currently there are no order operators implemented to be used for sorted sets and maps however.
May throw if LuaVal is not valid for some reason (which should not be possible).

### typetag

There are definitions for typetags used to identify each value type. These can be used in the constructor of a `LuaVal` as well.
For example a table can be created with `LuaVal table(TTABLE)`. You can get the typetag of an object with the member function `LuaTypeTag LuaVal::typetag()`.
`typetag()` does not throw.

```C++
enum LuaTypeTag
{
    TNIL,
    TSTRING,
    TNUMBER,
    TTABLE,
    TBOOL,
};
```

### tostring

The member function `std::string LuaVal::tostring()` returns a string representation of the object. This is similar to tostring in lua.
You can get a string representation of the typetag of a value with `value.type()`.
You can get a string representation of a typetag with `LuaVal::type(tag)`.
All of these may throw if LuaVal or tag is not valid for some reason (which should not be possible).

### operators

The LuaVal class offers a few operators.  
You can use == and != operators to compare scalar values by value. **Tables compare by internal identity (pointer), not structural contents** — two tables with the same data are unequal after copying. For content comparison, compare `dumps()` output or implement a deep `equiv()` helper (not built in).
LuaVal has the bool operator implemented so that nil and false will return false if a LuaVal is in a conditional statement. The assignment operator is also implemented and works as you would expect.
May throw if LuaVal is not valid for some reason (which should not be possible).

`**operator[]` auto-vivification:** reading a missing key inserts an empty table. Prefer `get()`, `has()`, and `set()` when building maps without stray entries. Avoid chaining `a[b][c]` in one expression — intermediate references can be invalidated if a nested table rehashes; use `set()`/`get()` or build subtables locally first.

### isvalue

There is a collection of member functions you can use to check whether the object is really of some type.
These functions do not throw.

```C++
luaval.isstring()
luaval.isnumber()
luaval.istable()
luaval.isbool()
luaval.isnil()
```

### LuaVal values

LuaVal can represent different types of data like a string and a number. To access the underlying value you must use specific functions.
The functions will throw if you use them on the wrong type object, for example using the str function on a table will throw.

```C++
luaval.num()
luaval.str()
luaval.boolean()
luaval.tbl()
```

### table access

There are several methods for accessing and editing a table.
**Note Inserted values are deep-copied via const lvalue setters. Use move overloads (`set(key, std::move(value))`, `insert(std::move(value))`) to avoid redundant copies of large tables.**

The way of accessing and inserting map elements are the get and set member functions `luaval.get(key)`, `luaval.set(key, value)`.
The function `set` returns the accessed table itself, so you can chain it to set multiple values.
When a value is attempted to be set as nil, it will be erased from the table instead.
These functions do not throw unless you use them on non table objects or with nil keys. `luaval.setignore(key, value)` works like `luaval.set(key, value)`, except it will not do anything if a value already exists in the table for that key.

The get method above will provide only const reference access to the table elements. For non const access to elements you must use the `[]` operator like so `luaval[key]`. If the accessed key does not exist in the accessed table then a table value is created to the table for that key. This means that accessing nonexisting elements will create clutter to the table. Setting a value to nil using brackets will store a nil value to the table instead of removing the key from the table.
This operator does not throw unless you use it on non table objects or with nil keys.

`luaval.has(key)` can be used to check if a value can be found in a table.
This function do not throw unless you use it on non table objects or with nil keys.

#### Safe lookup (no auto-vivification)

Prefer these when you do not want missing keys to create empty tables:

| Method | Missing key | Notes |
| --- | --- | --- |
| `try_get(key)` / `find(key)` | returns `nullptr` | safe optional access |
| `get(key)` | returns `LuaVal::nil` | const reference |
| `at(key)` | throws | mutable reference when present |
| `has(key)` | returns `false` | existence check |

Typed reads without exceptions: `try_as_number`, `try_as_string`, `try_as_bool`.

#### Path lookup and nested set/erase

Nested access uses the same tiers as single-key lookup. Paths never auto-vivify on read; `set_path` creates missing intermediate tables.

```C++
LuaVal doc = LuaVal::loads_or_throw("{stats:{hp:100}}");

if (LuaVal const * hp = doc.try_get_path("stats", "hp"))
    std::cout << hp->num() << std::endl;

double hp = doc.get_path("stats", "hp").num();          // nil if missing
double hp2 = doc.at_path("stats", "hp").num();         // throws with $.stats.hp

doc.set_path({ "player", "name" }, std::string("Ada")); // auto-vivify intermediates
doc.erase_path("stats", "hp");                          // no-op if path missing
```

Variadic segments (`try_get_path("a", "b", 1)`) and `std::initializer_list<LuaVal>` overloads are both available.

#### `lua_val` factories

Scalar helpers such as `lua_val::string(...)` and `lua_val::nil()` live in `smallfolk.h`. Include `smallfolk_convert.h` for STL-friendly factories such as `lua_val::map(...)` and `lua_val::array(...)` from `std::map` / `std::vector` / similar containers.

A method for erasing data with a key is `luaval.rem(key)` which also returns the accessed table.
This function do not throw unless you use it on non table objects or with nil keys.

Example usage of the functions:

```C++
LuaVal table(TTABLE); // create an empty table
table.set(1, "test").set(2, 77.234).set(3, -324); // set multiple values
table.set("self copy", table); // attempting to set a table into itself will create a deep copy
table.set(table, "table as key?"); // table will work as a key, but it will be a deep copy so you can not access it later
std::cout << table.get("self copy").get(3).num() << std::endl; // get a value from a nested table
table["number"] = 234; // Use table access operator to assign a value
LuaVal & value = table["number"]; // Use table access operator to get a value
table.set("self copy", LuaVal::nil).rem("number"); // remove some values through set and rem functions
if (table.has(100) and table[100].isstring())
	std::cout << table[100].str() << std::endl;
```

For convenience tables also have the methods `luaval.insert(value[, pos])`, `luaval.remove([pos])` and `luaval.len()`.
The len function returns the number of consecutive integer key elements in the table starting at index 1. It is similar to the # operator in lua.
Insert and remove shift the values on the right side of the given position and insert or remove a value to or at the given position. If position is omitted, the value is inserted to the end of the list or the last element is removed.
Insert and remove both return the accessed table.
Each function throws if used on a non table object or pos is not valid.

### table merging

You can merge two tables with `LuaVal::mrg(tbl1, tbl2)`. This will make a new table that contains values from both tables. If they have same keys then tbl2 will overwrite tbl1 value in the new table.