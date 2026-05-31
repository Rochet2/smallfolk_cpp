#include "smallfolk.h"

#include <cmath>
#include <cstdint>
#include <iostream>
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
    table.set("test", "world");
    table.set(67.5, -234.5);

    std::string serialized = table.dumps();
    expect_true(!serialized.empty(), "basic serialize succeeds");

    std::string err;
    LuaVal deserialized = LuaVal::loads(serialized, &err);
    expect_true(err.empty(), "basic loads has no error");
    expect_true(deserialized.get(1).str() == "Hello", "round-trip index 1");
    expect_true(deserialized.get("test").str() == "world", "round-trip string key");
    expect_true(deserialized.get(67.5).num() == -234.5, "round-trip numeric key");
}

static void test_non_finite_numbers()
{
    double zero = 0.0;
    LuaVal values = { -(zero / zero), (zero / zero), (1.0 / zero), -(1.0 / zero) };
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

    table.rem(1);
    expect_true(!table.has(1), "rem removes key");

    table.insert("tail");
    expect_true(table.has(1), "insert creates sequence head");
    expect_true(table.get(1).str() == "tail", "insert appends at first slot");

    LuaVal nested = LuaVal::table();
    LuaVal inner2 = LuaVal::table();
    inner2.set(6, "deep");
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
    parent.set("child", std::move(child));
    expect_true(child.istable(), "moved-from value remains valid table");
    expect_true(parent.get("child").get(1).str() == "moved", "move set stores value");
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
    test_equality_and_bool();

    if (failures == 0)
    {
        std::cout << "All tests passed." << std::endl;
        return 0;
    }

    std::cout << failures << " test(s) failed." << std::endl;
    return 1;
}
