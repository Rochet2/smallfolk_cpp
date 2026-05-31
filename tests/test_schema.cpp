#include "smallfolk.h"
#include "smallfolk_schema.h"

#include <iostream>
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

    void expect_false(bool condition, char const * message)
    {
        expect_true(!condition, message);
    }

    void expect_contains(std::string const & haystack, char const * needle, char const * message)
    {
        expect_true(haystack.find(needle) != std::string::npos, message);
    }
}

static Schema const number_schema = [] {
    Schema s;
    s.kind = SchemaKind::Number;
    return s;
}();
static Schema const string_schema = [] {
    Schema s;
    s.kind = SchemaKind::String;
    return s;
}();
static Schema const bool_schema = [] {
    Schema s;
    s.kind = SchemaKind::Bool;
    return s;
}();
static Schema const null_schema = [] {
    Schema s;
    s.kind = SchemaKind::Null;
    return s;
}();

static Schema const bounded_number_schema = schema::number_range(0.0, 100.0);

static Schema const enum_string_schema = schema::string_enum({ "red", "blue" });

static Schema const array_schema = schema::array_of(schema::string(), 1, 3);

static Schema::Field player_fields[] = {
    { "name", &string_schema, true },
    { "hp", &bounded_number_schema, true },
    { "title", &string_schema, false },
};

static bool positive_hp_validator(LuaVal const & value, std::string * err, char const * path)
{
    if (!value.istable())
        return true;
    LuaVal const * hp = value.try_get(std::string("hp"));
    if (hp && hp->isnumber() && hp->num() <= 0.0)
    {
        if (err)
            *err = std::string(path) + ": hp must be positive";
        return false;
    }
    return true;
}

static Schema const player_schema = [] {
    Schema s;
    s.kind = SchemaKind::Object;
    s.fields = player_fields;
    s.field_count = 3;
    s.allow_extra_keys = false;
    return s;
}();

static Schema const number_or_string_schema = schema::number_or_string();

static Schema const custom_player_schema = [] {
    Schema schema;
    schema.kind = SchemaKind::Object;
    schema.fields = player_fields;
    schema.field_count = 2;
    schema.allow_extra_keys = true;
    schema.validator = positive_hp_validator;
    return schema;
}();

static void test_basic_schema_kinds()
{
    expect_true(validate(LuaVal::nil, null_schema), "nil schema accepts nil");
    expect_false(validate(LuaVal(true), null_schema), "nil schema rejects bool");

    expect_true(validate(LuaVal(true), bool_schema), "bool schema accepts bool");
    expect_false(validate(LuaVal(1), bool_schema), "bool schema rejects number");

    expect_true(validate(LuaVal(42.5), number_schema), "number schema accepts number");
    expect_false(validate(LuaVal("x"), number_schema), "number schema rejects string");

    expect_true(validate(LuaVal("hello"), string_schema), "string schema accepts string");
    expect_false(validate(LuaVal(1), string_schema), "string schema rejects number");
}

static void test_number_bounds()
{
    expect_true(validate(LuaVal(50), bounded_number_schema), "bounded number accepts mid value");
    expect_false(validate(LuaVal(-1), bounded_number_schema), "bounded number rejects below min");
    expect_false(validate(LuaVal(101), bounded_number_schema), "bounded number rejects above max");
}

static void test_string_length()
{
    Schema const username_schema = schema::string_length(3, 12);

    expect_true(validate(LuaVal("Ada"), username_schema), "string_length accepts in-range value");
    expect_false(validate(LuaVal("ab"), username_schema), "string_length rejects too short");
    expect_false(validate(LuaVal("way_too_long_username"), username_schema), "string_length rejects too long");
    expect_false(validate(LuaVal(42), username_schema), "string_length rejects non-string");
}

static void test_array_schema()
{
    expect_true(validate(LuaVal{ LuaVal("a"), LuaVal("b") }, array_schema), "array schema accepts valid array");

    expect_false(validate(LuaVal::table(), array_schema), "array schema rejects empty array");
    expect_false(validate(LuaVal{ 1, 2, 3, 4 }, array_schema), "array schema rejects too many items");
    expect_false(validate(LuaVal{ 1 }, array_schema), "array schema rejects non-string item");

    Schema const other_array = schema::array_of(schema::number(), 1, 2);
    LuaVal const one_number = LuaVal(std::initializer_list<LuaVal>{ LuaVal(1) });
    expect_true(validate(one_number, other_array), "second array schema is independent");
    expect_false(validate(LuaVal(std::initializer_list<LuaVal>{ LuaVal("x") }), other_array), "second array schema uses its own element type");
}

static void test_object_schema()
{
    LuaVal valid = LuaVal::table();
    valid.set(std::string("name"), LuaVal("Ada"));
    valid.set(std::string("hp"), LuaVal(75));
    expect_true(validate(valid, player_schema), "object schema accepts required fields");

    LuaVal missing = LuaVal::table();
    missing.set(std::string("name"), LuaVal("Ada"));
    std::string err;
    expect_false(validate(missing, player_schema, &err), "object schema rejects missing required field");
    expect_contains(err, ".hp", "object schema error mentions missing field path");

    LuaVal extra = valid;
    extra.set(std::string("unknown"), LuaVal(1));
    err.clear();
    expect_false(validate(extra, player_schema, &err), "object schema rejects extra keys");
    expect_contains(err, ".unknown", "object schema error mentions unexpected field");
}

static void test_enum_string()
{
    expect_true(validate(LuaVal("red"), enum_string_schema), "enum accepts allowed value");
    expect_false(validate(LuaVal("green"), enum_string_schema), "enum rejects disallowed value");

    Schema const other_enum = schema::string_enum({ "green", "yellow" });
    expect_true(validate(LuaVal("green"), other_enum), "second enum schema is independent");
    expect_false(validate(LuaVal("red"), other_enum), "second enum schema does not share first values");
}

static void test_one_of()
{
    expect_true(validate(LuaVal(5), number_or_string_schema), "one_of accepts number branch");
    expect_true(validate(LuaVal("x"), number_or_string_schema), "one_of accepts string branch");
    expect_false(validate(LuaVal(true), number_or_string_schema), "one_of rejects bool");
}

static void test_custom_validator()
{
    LuaVal valid = LuaVal::table();
    valid.set(std::string("name"), LuaVal("Bob"));
    valid.set(std::string("hp"), LuaVal(10));
    expect_true(validate(valid, custom_player_schema), "custom validator accepts valid hp");

    LuaVal invalid = valid;
    invalid.set(std::string("hp"), LuaVal(0));
    std::string err;
    expect_false(validate(invalid, custom_player_schema, &err), "custom validator rejects non-positive hp");
    expect_contains(err, "hp must be positive", "custom validator error message");
}

static void test_compiled_schema()
{
    CompiledSchema compiled(player_schema);

    LuaVal valid = LuaVal::table();
    valid.set(std::string("name"), LuaVal("Ada"));
    valid.set(std::string("hp"), LuaVal(50));

    expect_true(compiled.validate(valid), "compiled schema accepts valid object");
    expect_true(validate(valid, compiled), "free validate overload accepts compiled schema");

    LuaVal invalid = valid;
    invalid.set(std::string("extra"), LuaVal(1));
    expect_false(compiled.validate(invalid), "compiled schema rejects extra keys");
}

static void test_loads_validated()
{
    std::string payload = "{'name':'Ada','hp':50}";

    std::string err;
    LuaVal value = loads_validated(payload, player_schema, &err);
    expect_true(err.empty(), "loads_validated succeeds without error");
    expect_true(value.get(std::string("name")).str() == "Ada", "loads_validated returns parsed value");

    err.clear();
    LuaVal bad = loads_validated("{bad", player_schema, &err);
    expect_true(bad.isnil(), "loads_validated returns nil on parse error");
    expect_true(!err.empty(), "loads_validated sets parse error");

    err.clear();
    LuaVal invalid = loads_validated("{'name':'Ada'}", player_schema, &err);
    expect_true(invalid.isnil(), "loads_validated returns nil on schema error");
    expect_contains(err, ".hp", "loads_validated schema error mentions path");
}

static void test_any_with_custom_validator()
{
    Schema const any_positive = [] {
        Schema schema;
        schema.kind = SchemaKind::Any;
        schema.validator = positive_hp_validator;
        return schema;
    }();

    LuaVal table = LuaVal::table();
    table.set(std::string("hp"), LuaVal(5));
    expect_true(validate(table, any_positive), "Any kind runs custom validator on table");

    table.set(std::string("hp"), LuaVal(-1));
    expect_false(validate(table, any_positive), "Any kind custom validator can reject value");
}

static void test_loads_validated_compiled()
{
    CompiledSchema compiled(player_schema);
    std::string err;
    LuaVal value = loads_validated("{'name':'Lin','hp':12}", compiled, &err);
    expect_true(err.empty(), "loads_validated works with compiled schema");
    expect_true(value.get(std::string("name")).str() == "Lin", "compiled loads_validated parses value");
}

static void test_one_of_error_detail()
{
    std::string err;
    expect_false(validate(LuaVal(true), number_or_string_schema, &err), "one_of rejects invalid type");
    expect_contains(err, "one_of mismatch", "one_of includes summary error");
}

static void test_array_boundaries()
{
    LuaVal min_array = LuaVal::table();
    min_array.set(1, LuaVal("only"));
    expect_true(validate(min_array, array_schema), "array accepts minimum size");

    LuaVal max_array = LuaVal::table();
    max_array.set(1, LuaVal("a"));
    max_array.set(2, LuaVal("b"));
    max_array.set(3, LuaVal("c"));
    expect_true(validate(max_array, array_schema), "array accepts maximum size");
}

static void test_schema_presets()
{
    expect_true(validate(LuaVal(42.5), schema::number()), "schema::number accepts number");
    expect_true(validate(LuaVal("hello"), schema::string()), "schema::string accepts string");
    expect_true(validate(LuaVal(true), schema::boolean()), "schema::boolean accepts bool");
    expect_true(validate(LuaVal::nil, schema::null()), "schema::null accepts nil");
    expect_true(validate(LuaVal(1), schema::any()), "schema::any accepts anything");

    expect_true(
        validate(LuaVal{ LuaVal("a"), LuaVal("b") }, schema::string_array()),
        "schema::string_array accepts string elements");
    expect_false(
        validate(LuaVal{ LuaVal(1) }, schema::string_array()),
        "schema::string_array rejects non-string elements");

    Schema const string_int_map = schema::map_of(schema::number_or_string());
    LuaVal map = LuaVal::table();
    map.set(std::string("count"), LuaVal(3));
    map.set(std::string("label"), LuaVal("items"));
    expect_true(validate(map, string_int_map), "schema::map_of accepts matching values");
    map.set(std::string("bad"), LuaVal(true));
    expect_false(validate(map, string_int_map), "schema::map_of rejects invalid value");

    std::string nested = "{'ok':t,'n':1,'s':'x','a':{1,2},'m':{'k':'v'}}";
    std::string err;
    LuaVal parsed = loads_validated(nested, schema::value(), &err);
    expect_true(err.empty(), "schema::value accepts nested json-like payload");
    if (!parsed.isnil())
        expect_true(parsed.get(std::string("ok")).isbool(), "schema::value parsed nested bool");
}

static void test_validation_limits()
{
    CompiledSchema compiled(schema::number_array());
    ValidateLimits tight;
    tight.max_validation_steps = 2;

    std::string err;
    expect_false(
        compiled.validate(LuaVal{ LuaVal(1), LuaVal(2), LuaVal(3) }, tight, &err),
        "validation step limit rejects large array");
    expect_contains(err, "step limit", "validation step limit error message");
}

static void test_loads_validated_or_throw()
{
    try
    {
        LuaVal value = loads_validated_or_throw("{'name':'Ada','hp':40}", player_schema);
        expect_true(value.get(std::string("hp")).num() == 40.0, "loads_validated_or_throw parses valid payload");
    }
    catch (smallfolk_exception const &)
    {
        expect_true(false, "loads_validated_or_throw should not throw for valid payload");
    }

    try
    {
        loads_validated_or_throw("{'name':'Ada'}", player_schema);
        expect_true(false, "loads_validated_or_throw should throw for invalid payload");
    }
    catch (smallfolk_exception const & e)
    {
        expect_contains(std::string(e.what()), "required field missing", "loads_validated_or_throw throws schema error");
    }
}

int main()
{
    std::cout << "Running schema tests..." << std::endl;

    test_basic_schema_kinds();
    test_number_bounds();
    test_string_length();
    test_array_schema();
    test_object_schema();
    test_enum_string();
    test_one_of();
    test_custom_validator();
    test_compiled_schema();
    test_any_with_custom_validator();
    test_loads_validated_compiled();
    test_one_of_error_detail();
    test_array_boundaries();
    test_schema_presets();
    test_validation_limits();
    test_loads_validated();
    test_loads_validated_or_throw();

    if (failures == 0)
    {
        std::cout << "All schema tests passed." << std::endl;
        return 0;
    }

    std::cout << failures << " schema test(s) failed." << std::endl;
    return 1;
}
