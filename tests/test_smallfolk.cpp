#include "smallfolk.h"
#include "smallfolk_convert.h"

#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>

namespace
{
    int failures = 0;

    void expect_true(bool condition, char const * message)
    {
        if (!condition)
        {
            std::cout << "FAIL: " << message << std::endl;
            ++failures;
        }
    }

    void expect_equal(std::string const & actual, std::string const & expected, char const * message)
    {
        if (actual != expected)
        {
            std::cout << "FAIL: " << message << std::endl;
            std::cout << "  expected: " << expected << std::endl;
            std::cout << "  actual:   " << actual << std::endl;
            ++failures;
        }
    }

    void expect_load_error(std::string const & input, LoadLimits const & limits, char const * message)
    {
        std::string err;
        LuaVal value = LuaVal::loads(input, limits, &err);
        expect_true(value.isnil(), message);
        expect_true(!err.empty(), message);
    }
}

static void test_type_tags()
{
    expect_true(LuaVal(-123).isnumber(), "implicit int is number");
    expect_true(LuaVal::nil.isnil(), "nil is nil");
    expect_true(LuaVal(true).isbool(), "bool is bool");
    expect_true(LuaVal("x").isstring(), "string is string");
    expect_true(LuaVal(123.456f).isnumber(), "float is number");
    expect_true(LuaVal(static_cast<int64_t>(9007199254740991LL)).isnumber(), "int64 is number");
    expect_true(LuaVal::table().istable(), "table() is table");
}

static void test_round_trip_basic()
{
    LuaVal table = LuaVal::table();
    table.set(1, "Hello");
    table.set(std::string("test"), std::string("world"));
    table.set(LuaVal(67.5), -234.5);

    std::string serialized = table.dumps();
    expect_true(!serialized.empty(), "basic serialize succeeds");

    std::string err;
    LuaVal deserialized = LuaVal::loads(serialized, &err);
    expect_true(err.empty(), "basic loads has no error");
    expect_true(deserialized.get(1).str() == "Hello", "round-trip index 1");
    expect_true(deserialized.get(std::string("test")).str() == "world", "round-trip string key");
    expect_true(deserialized.get(LuaVal(67.5)).num() == -234.5, "round-trip numeric key");
}

static void test_non_finite_numbers()
{
    LuaVal values = {
        -std::numeric_limits<double>::quiet_NaN(),
        std::numeric_limits<double>::quiet_NaN(),
        std::numeric_limits<double>::infinity(),
        -std::numeric_limits<double>::infinity()
    };
    std::string serialized = values.dumps();
    expect_true(!serialized.empty(), "non-finite values serialize");

    std::string err;
    LuaVal loaded = LuaVal::loads(serialized, &err);
    expect_true(err.empty(), "non-finite loads cleanly");
    expect_true(loaded.istable(), "non-finite loads to table");
    if (!loaded.istable())
        return;

    expect_true(std::isnan(loaded.get(1).num()), "NaN round-trip slot 1");
    expect_true(std::isnan(loaded.get(2).num()), "NaN round-trip slot 2");
    expect_true(loaded.get(3).num() > 0, "positive infinity round-trip");
    expect_true(loaded.get(4).num() < 0, "negative infinity round-trip");
}

static void test_table_operations()
{
    LuaVal table(TTABLE);
    table.set(1, "test").set(2, 77.234).set(3, -324);
    expect_true(table.len() == 3, "len after set");

    table.erase(1);
    expect_true(!table.has(1), "erase removes key");

    table.insert(LuaVal("tail"));
    expect_true(table.has(1), "insert creates sequence head");
    expect_true(table.get(1).str() == "tail", "insert appends at first slot");

    LuaVal nested = LuaVal::table();
    LuaVal inner2 = LuaVal::table();
    inner2.set(6, LuaVal("deep"));
    LuaVal inner = LuaVal::table();
    inner.set(5, std::move(inner2));
    nested.set(4, std::move(inner));
    expect_true(nested.get(4).get(5).get(6).str() == "deep", "nested table access");
}

static void test_move_setters()
{
    LuaVal child = LuaVal::table();
    child.set(1, "moved");

    LuaVal parent = LuaVal::table();
    parent.set(std::string("child"), std::move(child));
    expect_true(child.istable(), "moved-from value remains valid table");
    expect_true(parent.get(std::string("child")).get(1).str() == "moved", "move set stores value");
}

static void test_load_limits()
{
    LoadLimits strict;
    strict.max_input_size = 32;
    strict.max_string_length = 8;
    strict.max_nesting_depth = 2;
    strict.max_value_count = 4;
    strict.require_consumed_input = true;

    expect_load_error(std::string(33, 'a'), strict, "input size limit");
    expect_load_error("{\"123456789\"}", strict, "string length limit");
    expect_load_error("{{{\"too deep\"}}}", strict, "nesting depth limit");
    expect_load_error("{1,2,3,4,5}", strict, "value count limit");
    expect_load_error("{1} trailing", strict, "trailing input rejected");

    LoadLimits table_limits = strict;
    table_limits.max_value_count = 100;
    table_limits.max_table_entries = 2;
    expect_load_error("{1,2,3}", table_limits, "table entry limit");

    LoadLimits finite_only = strict;
    finite_only.max_value_count = 100;
    finite_only.reject_non_finite_numbers = true;
    expect_load_error("I", finite_only, "non-finite number rejected");

    LoadLimits relaxed = strict;
    relaxed.require_consumed_input = false;
    std::string err;
    LuaVal value = LuaVal::loads("{1} trailing", relaxed, &err);
    expect_true(err.empty(), "relaxed mode allows trailing input");
    expect_true(value.get(1).num() == 1.0, "relaxed mode still parses prefix");
}

static void test_exceptions()
{
    std::string err;
    try
    {
        LuaVal number(-7);
        number.str();
        expect_true(false, "str on number should throw");
    }
    catch (smallfolk_exception const & e)
    {
        expect_true(std::string(e.what()).find("non string") != std::string::npos, "type error message");
    }

    LuaVal bad = LuaVal::loads("{unterminated", &err);
    expect_true(bad.isnil(), "invalid input returns nil");
    expect_true(!err.empty(), "invalid input sets errmsg");
}

static void test_quoted_strings()
{
    LuaVal embedded("a\"b");
    expect_equal(embedded.dumps(), "\"a\"\"b\"", "embedded quote serializes");

    std::string err;
    LuaVal loaded = LuaVal::loads("\"hello\"\"world\"", &err);
    expect_true(err.empty(), "escaped quote loads cleanly");
    expect_equal(loaded.str(), "hello\"world", "escaped quote deserializes");

    LuaVal table = LuaVal::table();
    table.set(std::string("a\"b"), 1);
    LuaVal round_trip = LuaVal::loads(table.dumps(), &err);
    expect_true(err.empty(), "table with quoted key round-trips");
    expect_true(round_trip.has(std::string("a\"b")), "quoted key preserved");
    expect_true(round_trip.get(std::string("a\"b")).num() == 1.0, "quoted key value preserved");
}

static void test_merge_edge_cases()
{
    {
        LuaVal left(TTABLE);
        left.set(1, 1);
        LuaVal right(TTABLE);
        right.set(2, 2);

        LuaVal merged = LuaVal::merge(left, right);
        expect_true(left.has(1), "const merge leaves left unchanged");
        expect_true(!left.has(2), "const merge does not mutate left");
        expect_true(merged.has(1) && merged.has(2), "const merge combines both tables");
        expect_true(left != merged, "const merge returns a new table");
    }

    {
        LuaVal left(TTABLE);
        left.set(1, 1);
        left.set(LuaVal("keep"), std::string("left"));
        LuaVal right(TTABLE);
        right.set(1, 2);
        right.set(LuaVal("add"), std::string("right"));

        LuaVal merged = LuaVal::merge(std::move(left), std::move(right));
        expect_true(merged.has(1) && merged.has(LuaVal("keep")) && merged.has(LuaVal("add")),
            "rvalue merge combines all keys");
        expect_true(merged.get(1).num() == 2.0, "rvalue merge lets right override shared keys");
        expect_true(merged.get(LuaVal("keep")).str() == "left", "rvalue merge keeps left-only keys");
        expect_true(merged.get(LuaVal("add")).str() == "right", "rvalue merge adds right-only keys");

        expect_true(left.istable(), "rvalue merge leaves moved-from table valid");
        expect_true(!left.has(1), "rvalue merge moves content out of left");
        expect_true(left.len() == 0, "rvalue merge leaves moved-from table empty");
    }

    {
        LuaVal left(TTABLE);
        left.set(1, std::string("via-mrg"));
        LuaVal right(TTABLE);
        right.set(LuaVal("via-mrg-key"), 7);

        LuaVal merged = LuaVal::mrg(std::move(left), std::move(right));
        expect_true(merged.get(1).str() == "via-mrg", "mrg rvalue alias merges left keys");
        expect_true(merged.get(LuaVal("via-mrg-key")).num() == 7.0, "mrg rvalue alias merges right keys");
    }

    {
        LuaVal left(TTABLE);
        left.set(1, 1);
        LuaVal right(TTABLE);
        right.set(2, 2);

        LuaVal merged = LuaVal::merge(std::move(left), right);
        expect_true(merged.has(1) && merged.has(2), "move-left merge combines tables");
        expect_true(!right.has(1), "move-left merge does not mutate right operand");
    }

    {
        LuaVal left(TTABLE);
        left.set(1, 1);
        LuaVal right(TTABLE);
        right.set(2, 2);

        LuaVal merged = LuaVal::merge(left, std::move(right));
        expect_true(merged.has(1) && merged.has(2), "move-right merge combines tables");
        expect_true(left.has(1), "move-right merge does not mutate left operand");
        expect_true(!left.has(2), "move-right merge does not add right keys to left");
    }
}

static void test_len_edge_cases()
{
    expect_true(LuaVal::table().len() == 0, "empty table len is zero");

    {
        LuaVal table(TTABLE);
        table.set(1, 1);
        table.set(2, 2);
        table.set(3, 3);
        expect_true(table.len() == 3, "contiguous sequence len");

        table.erase(2);
        expect_true(table.len() == 1, "len stops at first missing index");
    }

    {
        LuaVal table(TTABLE);
        table.set(1, 1);
        table.set(2, 2);
        table[3] = LuaVal::nil;
        expect_true(table.len() == 2, "len stops at nil array slot");
    }

    {
        LuaVal table(TTABLE);
        table.set(5, 5);
        expect_true(table.len() == 0, "len ignores sparse high index without index 1");
    }

    {
        LuaVal table(TTABLE);
        table.set(1, 1);
        table.set(3, 3);
        expect_true(table.len() == 1, "len stops before first gap in sequence");
    }

    {
        LuaVal table(TTABLE);
        for (unsigned int i = 1; i <= 1000; ++i)
            table.set(static_cast<int>(i), static_cast<int>(i));
        expect_true(table.len() == 1000, "len handles long contiguous sequence");
    }
}

static void test_number_parse_edge_cases()
{
    std::string err;
    LuaVal table = LuaVal::loads("{ 123 , 456.5 , -0 , 1e2 }", &err);
    expect_true(err.empty(), "whitespace-padded array parses cleanly");
    expect_true(table.get(1).num() == 123.0, "leading whitespace number slot 1");
    expect_true(table.get(2).num() == 456.5, "leading whitespace number slot 2");
    expect_true(table.get(3).num() == 0.0, "leading whitespace number slot 3");
    expect_true(table.get(4).num() == 100.0, "leading whitespace scientific notation");
}

static void test_new_api()
{
    LuaVal table = LuaVal::table();
    table.set(std::string("name"), std::string("Ada"));
    table.set(1, 42);

    expect_true(table.try_get(std::string("missing")) == nullptr, "try_get returns nullptr when missing");
    expect_true(table.try_get(std::string("name")) != nullptr, "try_get finds existing key");
    expect_true(table.try_get(std::string("name"))->str() == "Ada", "try_get dereferences value");

    try
    {
        table.at(std::string("missing"));
        expect_true(false, "at should throw for missing key");
    }
    catch (smallfolk_exception const &)
    {
    }

    expect_true(table.at(std::string("name")).str() == "Ada", "at returns mutable reference");
    expect_true(table.get(std::string("name")).str() == "Ada", "string key get shortcut");

    double number = 0.0;
    expect_true(table.try_get(1)->try_as_number(number), "try_as_number on index");
    expect_true(number == 42.0, "try_as_number value");

    std::string const * text = nullptr;
    expect_true(table.try_get(std::string("name"))->try_as_string(text), "try_as_string");
    expect_true(text && *text == "Ada", "try_as_string value");

    LuaVal from_factory = lua_val::map({ { LuaVal("x"), 1 }, { LuaVal("y"), 2 } });
    expect_true(from_factory.get(LuaVal("x")).num() == 1.0, "lua_val::map factory");

    std::vector<int> values = { 3, 4, 5 };
    LuaVal from_vector = lua_val::array(values);
    expect_true(from_vector.get(2).num() == 4.0, "lua_val::array from vector");

    LuaVal merged = LuaVal::merge(
        LuaVal{ 1, 2 },
        lua_val::map({ { LuaVal("a"), LuaVal("b") } }));
    expect_true(merged.get(1).num() == 1.0, "merge keeps left array");
    expect_true(merged.get(LuaVal("a")).str() == "b", "merge adds right map entries");

    try
    {
        LuaVal::loads_or_throw("{bad");
        expect_true(false, "loads_or_throw should throw on invalid input");
    }
    catch (smallfolk_exception const & e)
    {
        expect_true(std::string(e.what()).find("Smallfolk:") != std::string::npos, "loads_or_throw message");
    }

    LuaVal loaded = LuaVal::loads_or_throw("{\"ok\":t}");
    expect_true(loaded.get(LuaVal("ok")).boolean(), "loads_or_throw parses valid input");

    LuaVal payload = lua_val::string("test");
    expect_equal(payload.dumps_or_throw(), "\"test\"", "dumps_or_throw");
}

static void test_path_api()
{
    LuaVal root = LuaVal::table();
    root.set(std::string("stats"), lua_val::map({ { LuaVal("hp"), 100 }, { LuaVal("mp"), 50 } }));
    root.set(1, std::string("first"));

    expect_true(root.try_get_path() == &root, "empty path returns root");
    expect_true(root.get_path().istable(), "get_path empty returns root");

    LuaVal const * hp = root.try_get_path(std::string("stats"), std::string("hp"));
    expect_true(hp != nullptr, "try_get_path finds nested string key");
    expect_true(hp->num() == 100.0, "try_get_path nested value");

    expect_true(root.try_get_path(std::string("stats"), std::string("missing")) == nullptr, "try_get_path missing leaf");
    expect_true(root.try_get_path(std::string("missing"), std::string("hp")) == nullptr, "try_get_path missing parent");
    expect_true(root.try_get_path(1)->str() == "first", "try_get_path int segment");

    expect_true(root.get_path(std::string("stats"), std::string("hp")).num() == 100.0, "get_path nested value");
    expect_true(root.get_path(std::string("stats"), std::string("missing")).isnil(), "get_path missing returns nil");

    expect_true(root.has_path(std::string("stats"), std::string("hp")), "has_path existing path");
    expect_true(!root.has_path(std::string("stats"), std::string("missing")), "has_path missing leaf");
    expect_true(!root.has_path(std::string("missing"), std::string("hp")), "has_path missing parent");

    expect_true(root.at_path(std::string("stats"), std::string("hp")).num() == 100.0, "at_path nested value");
    root.at_path(std::string("stats"), std::string("hp")) = 120;
    expect_true(root.get_path(std::string("stats"), std::string("hp")).num() == 120.0, "at_path mutable reference");

    try
    {
        root.at_path(std::string("stats"), std::string("missing"));
        expect_true(false, "at_path should throw for missing key");
    }
    catch (smallfolk_exception const & e)
    {
        expect_true(std::string(e.what()).find("$.stats.missing") != std::string::npos, "at_path missing key path");
    }

    root.set(std::string("scalar"), 42);
    try
    {
        root.at_path(std::string("scalar"), std::string("x"));
        expect_true(false, "at_path should throw when traversing non-table");
    }
    catch (smallfolk_exception const & e)
    {
        expect_true(std::string(e.what()).find("not a table at $.scalar") != std::string::npos, "at_path non-table path");
    }

    LuaVal fresh = LuaVal::table();
    fresh.set_path({ LuaVal("player"), LuaVal("name") }, std::string("Ada"));
    expect_true(fresh.has_path(std::string("player"), std::string("name")), "set_path creates intermediate tables");
    expect_true(fresh.get_path(std::string("player"), std::string("name")).str() == "Ada", "set_path nested value");

    fresh.set_path({ LuaVal("player"), LuaVal("stats"), LuaVal("hp") }, 88);
    expect_true(fresh.get_path(std::string("player"), std::string("stats"), std::string("hp")).num() == 88.0, "set_path deep path");

    expect_true(fresh.has_path(std::string("player"), std::string("stats"), std::string("hp")), "set_path deep has_path");
    fresh.erase_path(std::string("player"), std::string("stats"), std::string("hp"));
    expect_true(!fresh.has_path(std::string("player"), std::string("stats"), std::string("hp")), "erase_path removes nested key");
    expect_true(fresh.has_path(std::string("player"), std::string("stats")), "erase_path keeps parent table");

    fresh.erase_path(std::string("player"), std::string("missing"));
    expect_true(fresh.has_path(std::string("player"), std::string("name")), "erase_path missing path is no-op");

    try
    {
        LuaVal number = 5;
        number.has_path(std::string("x"));
        expect_true(false, "has_path should throw on non-table root");
    }
    catch (smallfolk_exception const &)
    {
    }
}

static void test_equality_and_bool()
{
    LuaVal five = 5;
    LuaVal six = 6;
    LuaVal also_five = 5;
    expect_true(five == also_five, "numbers compare by value");
    expect_true(five != six, "different numbers are unequal");

    LuaVal left = LuaVal::table();
    LuaVal right = LuaVal::table();
    left.set(1, 1);
    right.set(1, 1);
    expect_true(left != right, "tables compare by identity");

    expect_true(static_cast<bool>(five), "truthy number");
    expect_true(!static_cast<bool>(LuaVal::nil), "nil is falsy");
    expect_true(!static_cast<bool>(false), "false is falsy");
}

static void test_nil_bool_and_type()
{
    std::string err;
    LuaVal loaded = LuaVal::loads("{t,f}", &err);
    expect_true(err.empty(), "bool literals parse");
    expect_true(loaded.get(1).boolean(), "true round-trip");
    expect_true(!loaded.get(2).boolean(), "false round-trip");

    bool value = false;
    expect_true(LuaVal(true).try_as_bool(value) && value, "try_as_bool on true");
    expect_true(LuaVal(false).try_as_bool(value) && !value, "try_as_bool on false");
    expect_true(!LuaVal(1).try_as_bool(value), "try_as_bool rejects number");

    expect_equal(LuaVal::type(TTABLE), "table", "type tag string for table");
    expect_equal(LuaVal(5).type(), "number", "instance type string");
}

static void test_set_nil_vs_bracket_nil()
{
    LuaVal table = LuaVal::table();
    table.set(std::string("removed"), std::string("x"));
    table.set(std::string("removed"), LuaVal::nil);
    expect_true(!table.has(std::string("removed")), "set(nil) erases key");

    table["stored"] = LuaVal::nil;
    expect_true(table.has(std::string("stored")), "bracket nil stores explicit nil entry");
    expect_true(table.get(std::string("stored")).isnil(), "bracket nil value is nil");
}

static void test_setignore()
{
    LuaVal table = LuaVal::table();
    table.set(std::string("keep"), std::string("first"));
    table.setignore(std::string("keep"), std::string("second"));
    expect_true(table.get(std::string("keep")).str() == "first", "setignore does not overwrite");

    table.setignore(std::string("new"), std::string("added"));
    expect_true(table.get(std::string("new")).str() == "added", "setignore inserts missing key");

    table.setignore(std::string("skip"), LuaVal::nil);
    expect_true(!table.has(std::string("skip")), "setignore ignores nil values");
}

static void test_copy_semantics()
{
    LuaVal original = LuaVal::table();
    original.set(std::string("x"), 1);
    LuaVal copy = original;
    copy.set(std::string("x"), 2);
    expect_true(original.get(std::string("x")).num() == 1.0, "copy is deep for tables");

    LuaVal assigned = LuaVal::table();
    assigned = original;
    assigned.set(std::string("x"), 3);
    expect_true(original.get(std::string("x")).num() == 1.0, "assignment copy is deep");
}

static void test_insert_remove_positions()
{
    LuaVal table = LuaVal::table();
    table.set(1, std::string("a")).set(2, std::string("b")).set(3, std::string("c"));
    table.insert(LuaVal("middle"), LuaVal(2));
    expect_true(table.get(2).str() == "middle", "insert shifts sequence");
    expect_true(table.get(3).str() == "b", "insert preserves trailing values");
    expect_true(table.len() == 4, "insert grows sequence length");

    table.remove(2);
    expect_true(table.get(2).str() == "b", "remove shifts sequence back");
    expect_true(table.len() == 3, "remove shrinks sequence length");
}

static void test_load_limits_global()
{
    LoadLimits saved = LuaVal::get_load_limits();
    LoadLimits custom;
    custom.max_input_size = 64;
    LuaVal::set_load_limits(custom);
    expect_true(LuaVal::get_load_limits().max_input_size == 64, "set_load_limits updates global default");

    std::string err;
    expect_load_error(std::string(65, 'x'), LuaVal::get_load_limits(), "global default applies to loads");
    LuaVal::set_load_limits(saved);
}

static void test_untrusted_load_limits()
{
    LoadLimits limits = LuaVal::untrusted_load_limits();
    expect_true(limits.max_input_size < LuaVal::default_load_limits().max_input_size, "untrusted input cap is tighter");
    expect_true(limits.reject_non_finite_numbers, "untrusted rejects non-finite numbers");
}

static void test_single_quoted_strings()
{
    std::string err;
    LuaVal loaded = LuaVal::loads("'it''s fine'", &err);
    expect_true(err.empty(), "single-quoted string loads");
    expect_equal(loaded.str(), "it's fine", "single-quoted apostrophe unescapes");

    LuaVal table = LuaVal::table();
    table.set(std::string("key"), std::string("value"));
    expect_true(LuaVal::loads(table.dumps(), &err).has(std::string("key")), "single-quoted key round-trip");
}

static void test_path_errors()
{
    LuaVal table = LuaVal::table();
    try
    {
        table.set_path({}, std::string("x"));
        expect_true(false, "set_path empty path throws");
    }
    catch (smallfolk_exception const &)
    {
    }

    try
    {
        table.erase_path({});
        expect_true(false, "erase_path empty path throws");
    }
    catch (smallfolk_exception const &)
    {
    }

    table.set(std::string("mid"), 42);
    try
    {
        table.set_path({ LuaVal("mid"), LuaVal("leaf"), LuaVal("x") }, std::string("bad"));
        expect_true(false, "set_path through scalar throws");
    }
    catch (smallfolk_exception const & e)
    {
        expect_true(std::string(e.what()).find("not a table at $.mid") != std::string::npos, "set_path scalar path message");
    }
}

static void test_empty_table_round_trip()
{
    expect_equal(LuaVal::table().dumps(), "{}", "empty table serializes");

    std::string err;
    LuaVal loaded = LuaVal::loads("{}", &err);
    expect_true(err.empty(), "empty table loads");
    expect_true(loaded.istable(), "empty table loads to table");
    expect_true(loaded.len() == 0, "empty table stays empty");
}

// Fixed wire payloads in gvx/Smallfolk token form (dump_object / dump_type).
static void test_lua_smallfolk_interop_wires()
{
    std::string err;

    {
        LuaVal value = LuaVal::loads("t", &err);
        expect_true(err.empty(), "lua wire scalar true loads");
        expect_true(value.isbool() && value.boolean(), "lua wire scalar true value");
    }

    {
        LuaVal value = LuaVal::loads("f", &err);
        expect_true(err.empty(), "lua wire scalar false loads");
        expect_true(value.isbool() && !value.boolean(), "lua wire scalar false value");
    }

    {
        LuaVal value = LuaVal::loads("n", &err);
        expect_true(err.empty(), "lua wire scalar nil loads");
        expect_true(value.isnil(), "lua wire scalar nil value");
    }

    {
        LuaVal value = LuaVal::loads("{t,f,n}", &err);
        expect_true(err.empty(), "lua wire scalar array loads");
        expect_true(value.get(1).boolean(), "lua wire array true");
        expect_true(!value.get(2).boolean(), "lua wire array false");
        expect_true(value.get(3).isnil(), "lua wire array nil");
    }

    {
        LuaVal value = LuaVal::loads("{I,i,N,Q}", &err);
        expect_true(err.empty(), "lua wire non-finite array loads");
        expect_true(std::isinf(value.get(1).num()) && value.get(1).num() > 0.0, "lua wire I is +inf");
        expect_true(std::isinf(value.get(2).num()) && value.get(2).num() < 0.0, "lua wire i is -inf");
        expect_true(std::isnan(value.get(3).num()), "lua wire N is nan");
        expect_true(std::isnan(value.get(4).num()), "lua wire Q is nan");
    }

    {
        LuaVal value = LuaVal::loads("\"a\"\"b\"", &err);
        expect_true(err.empty(), "lua wire doubled-quote string loads");
        expect_equal(value.str(), "a\"b", "lua wire doubled-quote string value");
    }

    {
        LuaVal value = LuaVal::loads("{\"Hello\",\"test\":\"world\",67.5:-234.5}", &err);
        expect_true(err.empty(), "lua wire mixed array/map table loads");
        expect_equal(value.get(1).str(), "Hello", "lua wire array slot 1");
        expect_equal(value.get(std::string("test")).str(), "world", "lua wire map key test");
        expect_true(value.get(LuaVal(67.5)).num() == -234.5, "lua wire numeric map key");
    }

    {
        LuaVal value = LuaVal::loads("{1,2,{3,4.5,'ke':'test'}}", &err);
        expect_true(err.empty(), "lua wire nested compact table loads");
        expect_true(value.get(1).num() == 1.0, "lua wire nested index 1");
        expect_true(value.get(2).num() == 2.0, "lua wire nested index 2");
        expect_true(value.get(3).get(1).num() == 3.0, "lua wire nested child array");
        expect_true(value.get(3).get(2).num() == 4.5, "lua wire nested child number");
        expect_equal(value.get(3).get(LuaVal("ke")).str(), "test", "lua wire nested child map key");
    }

    {
        char const * benchmark_wire =
            "{t,\"somestring\",123.456,t,"
            "{\"t\":-678,\"test\":123.45600128173828,\"f\":268435455,\"subtable\":{1,2,3}}}";
        LuaVal value = LuaVal::loads(benchmark_wire, &err);
        expect_true(err.empty(), "lua wire README benchmark payload loads");
        expect_true(value.get(1).isbool() && value.get(1).boolean(), "benchmark slot 1 bool");
        expect_equal(value.get(2).str(), "somestring", "benchmark slot 2 string");
        expect_true(value.get(3).num() == 123.456, "benchmark slot 3 number");
        expect_true(value.get(5).get(LuaVal("t")).num() == -678.0, "benchmark nested t");
        expect_true(value.get(5).get(LuaVal("f")).num() == 268435455.0, "benchmark nested f");
        expect_true(value.get(5).get(LuaVal("subtable")).get(2).num() == 2.0, "benchmark nested subtable");
    }

    {
        LuaVal value = LuaVal::loads("{1,\t2}", &err);
        expect_true(err.empty(), "lua wire tab whitespace loads");
        expect_true(value.get(2).num() == 2.0, "lua wire tab whitespace value");
    }

    {
        LuaVal original = LuaVal::loads("{1,2,{3,4.5,'ke':'test'}}", &err);
        expect_true(err.empty(), "interop round-trip source loads");
        std::string dumped = original.dumps(&err);
        expect_true(err.empty(), "interop round-trip dumps");
        LuaVal again = LuaVal::loads(dumped, &err);
        expect_true(err.empty(), "interop round-trip reloads");
        expect_true(again.get(1).num() == 1.0, "interop round-trip index 1");
        expect_true(again.get(3).get(LuaVal("ke")).str() == "test", "interop round-trip nested key");
    }

    expect_load_error("{@1}", LuaVal::default_load_limits(), "lua wire @ reference rejected");
}

int main()
{
    std::cout << "Running smallfolk tests..." << std::endl;

    test_type_tags();
    test_round_trip_basic();
    test_non_finite_numbers();
    test_table_operations();
    test_move_setters();
    test_load_limits();
    test_exceptions();
    test_quoted_strings();
    test_merge_edge_cases();
    test_len_edge_cases();
    test_number_parse_edge_cases();
    test_new_api();
    test_path_api();
    test_equality_and_bool();
    test_nil_bool_and_type();
    test_set_nil_vs_bracket_nil();
    test_setignore();
    test_copy_semantics();
    test_insert_remove_positions();
    test_load_limits_global();
    test_untrusted_load_limits();
    test_single_quoted_strings();
    test_path_errors();
    test_empty_table_round_trip();
    test_lua_smallfolk_interop_wires();

    if (failures == 0)
    {
        std::cout << "All tests passed." << std::endl;
        return 0;
    }

    std::cout << failures << " test(s) failed." << std::endl;
    return 1;
}
