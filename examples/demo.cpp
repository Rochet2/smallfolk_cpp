#include "smallfolk.h"
#include "smallfolk_convert.h"
#include "smallfolk_schema.h"

#include <cstdlib>
#include <deque>
#include <forward_list>
#include <iostream>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

// Runtime checks in this file use DEMO_CHECK instead of assert(). Release builds
// define NDEBUG and strip assert(), which would skip side effects inside checks.

namespace {

[[noreturn]] void demo_fail(char const * expr, char const * file, int line)
{
    std::cerr << "demo check failed: " << expr << " (" << file << ':' << line << ')' << std::endl;
    std::abort();
}

inline void demo_check(bool condition, char const * expr, char const * file, int line)
{
    if (!condition)
        demo_fail(expr, file, line);
}

} // namespace

#define DEMO_CHECK(expr) demo_check(static_cast<bool>(expr), #expr, __FILE__, __LINE__)

int main()
{
    {
        std::cout << "Test values" << std::endl;
        LuaVal asd = { "number", "string", "table", LuaVal::mrg({"number", "string"}, LuaVal::LuaTable({{"ke", "test"},{"ke2", "test"}})) };
        std::cout << asd.dumps() << std::endl;
        std::string err;
        LuaVal v = LuaVal::loads(" { 1 , 2 , { 3 , 4, ' k e ' : ' t e s t ' } } ", &err);
        std::cout << v.dumps(&err) << std::endl;
        std::cout << err << std::endl;

        std::cout << "Testing different double corner values" << std::endl;
        double _zero = 0.0;
        LuaVal tn = { -(0 / _zero), (0 / _zero), (1 / _zero), -(1 / _zero) };
        std::cout << tn.dumps() << std::endl;
        std::cout << -(0 / _zero) << " " << (0 / _zero) << " " << (1 / _zero) << " " << -(1 / _zero) << std::endl;
        std::cout << tn.get(1).tostring() << " " << tn.get(2).tostring() << " " << tn.get(3).tostring() << " " << tn.get(4).tostring() << std::endl;
        tn = LuaVal::loads(tn.dumps());
        std::cout << tn.get(1).tostring() << " " << tn.get(2).tostring() << " " << tn.get(3).tostring() << " " << tn.get(4).tostring() << std::endl;
        std::cout << std::endl;

        std::cout << "Testing creation testing and printing of all value types" << std::endl;
        LuaVal implicit_test = -123;
        LuaVal copy_test(implicit_test);
        LuaVal copy_test2 = implicit_test;
        LuaVal n = LuaVal::nil; // nil
        LuaVal n2(TNIL); // nil
        LuaVal b(true);
        LuaVal s("somestring");
        LuaVal d(123.456);
        LuaVal f(123.456f);
        LuaVal i(-678);
        LuaVal u(0xFFFFFFFF);
        LuaVal t; // defaults to table
        LuaVal t2 = LuaVal::table();
        LuaVal t3 = { 1, 2, 3 };
        LuaVal t4 = {}; // curly braces are table
        LuaVal t5(TTABLE);

        DEMO_CHECK(implicit_test.isnumber());
        DEMO_CHECK(copy_test.isnumber());
        DEMO_CHECK(copy_test2.isnumber());
        DEMO_CHECK(n.isnil());
        DEMO_CHECK(n2.isnil());
        DEMO_CHECK(b.isbool());
        DEMO_CHECK(s.isstring());
        DEMO_CHECK(d.isnumber());
        DEMO_CHECK(f.isnumber());
        DEMO_CHECK(i.isnumber());
        DEMO_CHECK(u.isnumber());
        DEMO_CHECK(t.istable());
        DEMO_CHECK(t2.istable());
        DEMO_CHECK(t3.istable());
        DEMO_CHECK(t4.istable());
        DEMO_CHECK(t5.istable());

        std::cout << implicit_test.tostring() << std::endl;
        std::cout << copy_test.tostring() << std::endl;
        std::cout << copy_test2.tostring() << std::endl;
        std::cout << n.tostring() << std::endl;
        std::cout << b.tostring() << std::endl;
        std::cout << s.tostring() << std::endl;
        std::cout << d.tostring() << std::endl;
        std::cout << f.tostring() << std::endl;
        std::cout << i.tostring() << std::endl;
        std::cout << u.tostring() << std::endl;
        std::cout << t.tostring() << std::endl;
        std::cout << t2.tostring() << std::endl;
        std::cout << t3.tostring() << std::endl;
        std::cout << std::endl;

        std::cout << "Testing exception handling" << std::endl;
        std::string errmsg;
        try
        {
            LuaVal h(-7);
            (void)h.str(); // error, h is not a string
        }
        catch (smallfolk_exception const & e)
        {
            // caught an exception
            errmsg = e.what();
        }
        // printing caught error if any
        if (!errmsg.empty())
            std::cout << errmsg << std::endl << std::endl;
    }

    {
        std::cout << "Example usage" << std::endl;

        // create a lua table and set some values to it
        LuaVal table = LuaVal::table();
        table.set(1, "Hello");
        table.set("test", "world");
        table.set(67.5, -234.5);

        // serialize the table
        std::string serialized = table.dumps();

        // print the serialization, it should be rather human readable
        std::cout << serialized << std::endl;
        // Example output: {"Hello","test":"world",67.5:-234.5}

        // form lua values from the string
        LuaVal deserialized = LuaVal::loads(serialized);

        // print the values from deserialized result table
        std::cout << deserialized.get(1).str() << " " << deserialized.get("test").str() << " " << deserialized.get(67.5).num() << std::endl;
        // Example output: Hello world -234.5
        std::cout << std::endl;
    }

    /*
    // This is disabled because circular references cause memleak or need complex handling for memory management
    // Circular references should not be used and are ignored (nil) when parsed
    // Using a circular reference in C++ code will cause an exception to be thrown
    {
        std::cout << "Cthulhu" << std::endl;

        // Essentially {{},{},{}}
        LuaVal cthulhu(TTABLE);
        cthulhu[1] = LuaVal(TTABLE);
        cthulhu[2] = LuaVal(TTABLE);
        cthulhu[3] = LuaVal(TTABLE);
        cthulhu["fhtagn"] = cthulhu;
        cthulhu[1][cthulhu[2]] = cthulhu[3];
        cthulhu[2][cthulhu[1]] = cthulhu[2];
        cthulhu[3][cthulhu[3]] = cthulhu;
        std::cout << cthulhu.dumps() << std::endl;
        // prints:
        // {"fhtagn":@1,1:{{@2:@3}:{@4:@1}},2:@3,3:@4}
        std::cout << std::endl;
    }

    {
        std::cout << "Table inside itself" << std::endl;
        try
        {
            LuaVal tbl(TTABLE);
            tbl.set(1, tbl);
            std::cout << tbl.dumps() << std::endl;
        }
        catch (smallfolk_exception const & e)
        {
            std::cout << e.what() << std::endl;
        }
        std::cout << std::endl;
    }
    */

    {
        std::cout << "Table initializer list coolness" << std::endl;
        std::cout << "{} evaluates to table" << std::endl;
        std::cout << "{{}} evaluates to a table inside a table" << std::endl;
        std::cout << "{5} evaluates to 5 being inside a table" << std::endl;
        std::cout << "{LuaVal(5)} evaluates to 5 inside a table" << std::endl;

        LuaVal nested = { {}, {{}}, { 3 }, { LuaVal(4) } };
        std::cout << nested.dumps() << std::endl; // Outputs {{},{{}},{3},{4}}
        std::cout << std::endl;
    }

    {
        std::cout << "test accessing table with [] operator" << std::endl;
        std::cout << "Note that table keys cannot be accessed!" << std::endl;
        std::cout << "Notice the excessive amount of tables left behind!" << std::endl;
        LuaVal nested = {};
        nested[1];
        nested[2];
        nested[3];
        nested[4][5][6]; // handy for quick creation of nested indexes
        std::cout << nested.dumps() << std::endl; // Outputs {{},{},{},{5:{6:{}}}}
        std::cout << std::endl;

        // To avoid unnecessary tables, use set and get (similar to at in c++ for map)
        LuaVal table(TTABLE);
        table.set(1, "test");
        std::cout << table.dumps() << std::endl;
        table.set(1, LuaVal::nil); // removing value through setting it to nil
        std::cout << table.dumps() << std::endl;
        table.set(table, "table as key?");
        std::cout << table.dumps() << std::endl;
        std::cout << table.get(table).tostring() << std::endl;
        std::cout << std::endl;
    }

    {
        std::cout << "test .(key).(key, val).rem(key)" << std::endl;
        std::cout << "Note that table keys cannot be accessed!" << std::endl;

        LuaVal table(TTABLE);
        table.set(1, "test").set(2, table.get(1)).set(3, -324);
        std::cout << table.dumps() << std::endl;
        table.rem(1).rem(2).set(3, LuaVal());
        std::cout << table.dumps() << std::endl;
        table.set(table, "table as key?");
        std::cout << table.get(table).tostring() << std::endl;
        std::cout << std::endl;
    }

    {
        std::cout << "test .insert.remove.len" << std::endl;

        LuaVal table(TTABLE);
        table.insert("test");
        table.insert(123);
        table.set("rand", 893);
        table.insert(345);
        std::cout << table.len() << std::endl;
        table.remove();
        table.remove();
        std::cout << table.len() << std::endl;
        std::cout << std::endl;
    }

    {
        LuaVal val = 5;
        LuaVal val2 = LuaVal::nil;
        if (val)
            std::cout << "bool() works" << std::endl;
        if (!val2)
            std::cout << "bool() works" << std::endl;
        if (val == 5)
            std::cout << "== works" << std::endl;
        if (val != 6)
            std::cout << "!= works" << std::endl;
    }

    {
        LuaVal val(TTABLE);
        val[123] = 5;
        val["test"] = 5;
        val[1.5] = 5;
    }

    {
        LuaVal val1 = { 1,2, LuaVal::mrg({ 3,4 }, LuaVal::LuaTable({ { "ke","test" } })) };
        LuaVal val2 = LuaVal::loads("{1,2,{3,4,'ke':'test'}}");
        std::cout << val1.dumps() << std::endl;
        std::cout << val2.dumps() << std::endl;
    }

    {
        LuaVal t1 = { 1, 2, { 1,2,3 } };
        LuaVal t2 = LuaVal::LuaTable{ { "key", "value" }, { 2, "value2" } };
        std::cout << t1.dumps() << std::endl;
        std::cout << t2.dumps() << std::endl;
    }

    std::forward_list<std::deque<std::string>> vec = { { "a", "b" },{ "a", "b" } };
    std::unordered_map<std::string, std::string> m;
    m["test"] = "asd";
    LuaVal t441 = vec;
    std::cout << t441.dumps() << std::endl;

    {
        std::cout << "lua_val factories and STL conversion" << std::endl;
        LuaVal from_factories = lua_val::map({
            { lua_val::string("name"), lua_val::string("Ada") },
            { lua_val::string("scores"), lua_val::array({ lua_val::number(9), lua_val::number(10) }) },
            { lua_val::string("active"), lua_val::boolean(true) }
        });
        LuaVal from_stl = lua_val::map(m);
        std::cout << from_factories.dumps() << std::endl;
        std::cout << from_stl.dumps() << std::endl;
        std::cout << std::endl;
    }

    {
        std::cout << "safe lookup: try_get / has / at (no auto-vivification)" << std::endl;
        LuaVal player = LuaVal::table();
        player.set("name", "Lin");
        player.set("hp", 42);

        DEMO_CHECK(player.try_get(std::string("missing")) == nullptr);
        DEMO_CHECK(player.has(std::string("name")));
        DEMO_CHECK(player.find(std::string("hp")) != nullptr);
        DEMO_CHECK(player.at(std::string("name")).str() == "Lin");

        // operator[] would insert an empty table for a missing key; try_get does not.
        LuaVal & vivified = player["would_create_empty_table"];
        DEMO_CHECK(vivified.istable());
        std::cout << "try_get/.has/at ok; operator[] auto-vivifies: "
                  << player.dumps() << std::endl;
        std::cout << std::endl;
    }

    {
        std::cout << "typed accessors: try_as_number / try_as_string / try_as_bool" << std::endl;
        LuaVal row = lua_val::map({
            { lua_val::string("n"), lua_val::number(3.14) },
            { lua_val::string("s"), lua_val::string("pi") },
            { lua_val::string("b"), lua_val::boolean(true) }
        });

        double n = 0;
        std::string const * s = nullptr;
        bool b = false;
        LuaVal const * const nv = row.try_get(std::string("n"));
        LuaVal const * const sv = row.try_get(std::string("s"));
        LuaVal const * const bv = row.try_get(std::string("b"));
        bool const got_n = nv && nv->try_as_number(n);
        bool const got_s = sv && sv->try_as_string(s);
        bool const got_b = bv && bv->try_as_bool(b);
        DEMO_CHECK(got_n && n > 3.0);
        DEMO_CHECK(got_s && s && *s == "pi");
        DEMO_CHECK(got_b && b);
        DEMO_CHECK(sv && !sv->try_as_number(n));
        std::cout << "typed reads: n=" << n << " s=" << *s << " b=" << b << std::endl;
        std::cout << std::endl;
    }

    {
        std::cout << "throwing serialize/parse API" << std::endl;
        LuaVal payload = lua_val::map({ { lua_val::string("ok"), lua_val::boolean(true) } });
        std::string wire = payload.dumps_or_throw();
        LuaVal parsed = LuaVal::loads_or_throw(wire);
        LuaVal const * const ok_flag = parsed.try_get(std::string("ok"));
        DEMO_CHECK(ok_flag && ok_flag->boolean());
        std::cout << "loads_or_throw/dumps_or_throw round-trip: " << wire << std::endl;
        std::cout << std::endl;
    }

    {
        std::cout << "move merge (no extra copies)" << std::endl;
        LuaVal left = lua_val::array({ lua_val::number(1), lua_val::string("keep") });
        LuaVal right = lua_val::map({ { lua_val::string("add"), lua_val::number(7) } });
        LuaVal merged = LuaVal::merge(std::move(left), std::move(right));
        DEMO_CHECK(merged.has(1));
        DEMO_CHECK(merged.has(std::string("add")));
        DEMO_CHECK(left.len() == 0); // moved-from table stays valid but empty
        std::cout << merged.dumps() << std::endl;
        std::cout << std::endl;
    }

    {
        std::cout << "quoted strings and keys" << std::endl;
        LuaVal quoted = lua_val::string("say \"hello\"");
        LuaVal table = lua_val::map({ { lua_val::string("a\"b"), lua_val::number(1) } });
        std::string wire = table.dumps_or_throw();
        LuaVal round_trip = LuaVal::loads_or_throw(wire);
        DEMO_CHECK(round_trip.at(std::string("a\"b")).num() == 1.0);
        std::cout << quoted.dumps() << " | " << wire << std::endl;
        std::cout << std::endl;
    }

    {
        std::cout << "LoadLimits: reject oversized hostile input" << std::endl;
        LoadLimits tight = LuaVal::untrusted_load_limits();
        tight.max_input_size = 8;
        std::string err;
        LuaVal rejected = LuaVal::loads("{'to':'o','long':1}", tight, &err);
        DEMO_CHECK(rejected.isnil());
        DEMO_CHECK(!err.empty());
        std::cout << "rejected: " << err << std::endl;
        std::cout << std::endl;
    }

    {
        std::cout << "schema validation: loads_validated with presets" << std::endl;
        static Schema const hp_schema = schema::number_range(1.0, 999.0);
        static Schema::Field player_fields[] = {
            { "name", &schema::string(), true },
            { "hp", &hp_schema, true },
            { "title", &schema::string(), false },
        };
        static Schema const player_schema = [] {
            Schema s;
            s.kind = SchemaKind::Object;
            s.fields = player_fields;
            s.field_count = 3;
            s.allow_extra_keys = false;
            return s;
        }();
        static CompiledSchema const compiled(player_schema);

        std::string good = "{'name':'Ada','hp':40}";
        std::string bad = "{'name':'Ada'}";
        std::string err;

        LuaVal ok = loads_validated(good, compiled, &err);
        DEMO_CHECK(err.empty() && ok.at(std::string("name")).str() == "Ada");

        LuaVal nope = loads_validated(bad, compiled, &err);
        DEMO_CHECK(nope.isnil() && err.find(".hp") != std::string::npos);
        std::cout << "valid payload ok; invalid payload: " << err << std::endl;
        std::cout << std::endl;
    }

    {
        std::cout << "schema: string_length (like number_range for strings)" << std::endl;
        Schema const username = schema::string_length(3, 8);
        Schema::Field signup_fields[] = {
            { "user", &username, true },
        };
        Schema const signup_schema = [&] {
            Schema s;
            s.kind = SchemaKind::Object;
            s.fields = signup_fields;
            s.field_count = 1;
            s.allow_extra_keys = false;
            return s;
        }();

        std::string err;
        LuaVal ok = loads_validated("{'user':'Lin'}", signup_schema, &err);
        DEMO_CHECK(err.empty() && ok.at(std::string("user")).str() == "Lin");

        LuaVal short_name = loads_validated("{'user':'Li'}", signup_schema, &err);
        DEMO_CHECK(short_name.isnil() && err.find("too short") != std::string::npos);
        std::cout << "too short: " << err << std::endl;

        LuaVal long_name = loads_validated("{'user':'waytoolong'}", signup_schema, &err);
        DEMO_CHECK(long_name.isnil() && err.find("too long") != std::string::npos);
        std::cout << "too long: " << err << std::endl;
        std::cout << std::endl;
    }

    {
        std::cout << "schema: string_enum, array_of, map_of, OneOf" << std::endl;
        Schema const color = schema::string_enum({ "red", "green", "blue" });
        Schema const tags = schema::array_of(schema::string_length(1, 16), 1, 4);
        Schema const stats = schema::map_of(schema::number_or_string());

        LuaVal palette = LuaVal::table();
        palette.set(std::string("color"), LuaVal("green"));
        palette.set(std::string("tags"), LuaVal{ LuaVal("ui"), LuaVal("v2") });
        LuaVal stat_map = LuaVal::table();
        stat_map.set(std::string("fps"), LuaVal(60));
        stat_map.set(std::string("mode"), LuaVal("fast"));
        palette.set(std::string("stats"), stat_map);

        std::string err;
        DEMO_CHECK(validate(palette.get(std::string("color")), color, &err));
        DEMO_CHECK(validate(palette.get(std::string("tags")), tags, &err));
        DEMO_CHECK(validate(palette.get(std::string("stats")), stats, &err));

        palette.set(std::string("color"), LuaVal("purple"));
        DEMO_CHECK(!validate(palette.get(std::string("color")), color, &err));
        std::cout << "enum reject: " << err << std::endl;

        DEMO_CHECK(validate(LuaVal(42), schema::number_or_string()));
        DEMO_CHECK(validate(LuaVal("text"), schema::number_or_string()));
        DEMO_CHECK(!validate(LuaVal(true), schema::number_or_string(), &err));
        std::cout << "OneOf rejects bool: " << err << std::endl;
        std::cout << std::endl;
    }

    {
        std::cout << "loads_validated_or_throw (parse + validate in one step)" << std::endl;
        Schema const payload_schema = schema::array_of(schema::number(), 1, 5);
        LuaVal scores = loads_validated_or_throw("{10,20,30}", payload_schema);
        DEMO_CHECK(scores.len() == 3);
        std::cout << scores.dumps() << std::endl;
        std::cout << std::endl;
    }

    {
        std::cout << "setignore / erase (skip nil inserts, explicit removal)" << std::endl;
        LuaVal bag = LuaVal::table();
        bag.set(std::string("keep"), LuaVal(1))
            .setignore(std::string("skip_nil"), LuaVal::nil)
            .setignore(std::string("add"), LuaVal(2))
            .set(std::string("remove_me"), LuaVal(9));
        bag.erase(std::string("remove_me"));
        DEMO_CHECK(bag.has(std::string("keep")));
        DEMO_CHECK(bag.has(std::string("add")));
        DEMO_CHECK(!bag.has(std::string("skip_nil")));
        DEMO_CHECK(!bag.has(std::string("remove_me")));
        std::cout << bag.dumps() << std::endl;
        std::cout << std::endl;
    }

    {
        std::cout << "type() / typetag() introspection" << std::endl;
        LuaVal samples[] = {
            LuaVal::nil,
            LuaVal(true),
            LuaVal(3.14),
            LuaVal("text"),
            LuaVal::table()
        };
        for (LuaVal const & sample : samples)
            std::cout << sample.type() << " (tag " << static_cast<int>(sample.typetag()) << ")" << std::endl;
        std::cout << std::endl;
    }

    {
        std::cout << "ValidateLimits: cap validation work on untrusted input" << std::endl;
        CompiledSchema const compiled(schema::number_array());

        ValidateLimits tight = untrusted_validate_limits();
        tight.max_validation_steps = 2;

        std::string err;
        LuaVal many = LuaVal{ LuaVal(1), LuaVal(2), LuaVal(3) };
        DEMO_CHECK(!compiled.validate(many, tight, &err));
        std::cout << "validation budget exceeded: " << err << std::endl;
        std::cout << std::endl;
    }

    std::cout << "demo finished successfully" << std::endl;
    return 0;
}
