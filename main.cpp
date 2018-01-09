#include "smallfolk.h"
#include <iostream> // std::cout
#include <cassert> // assert
#include <map>

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
        LuaVal n; // nil
        LuaVal b(true);
        LuaVal s("somestring");
        LuaVal d(123.456);
        LuaVal f(123.456f);
        LuaVal i(-678);
        LuaVal u(0xFFFFFFFF);
        LuaVal t(TTABLE);
        LuaVal t2 = LuaVal::table();
        LuaVal t3 = { 1, 2, 3 };

        assert(implicit_test.isnumber());
        assert(copy_test.isnumber());
        assert(copy_test2.isnumber());
        assert(n.isnil());
        assert(b.isbool());
        assert(s.isstring());
        assert(d.isnumber());
        assert(f.isnumber());
        assert(i.isnumber());
        assert(u.isnumber());
        assert(t.istable());
        assert(t2.istable());
        assert(t3.istable());

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
            std::string str = h.str(); // error, h is not a string
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
        std::cout << "Watch out for quirks though!" << std::endl;
        std::cout << "What stuff evaluates to depends on your compiler and C++ version!" << std::endl;
        std::cout << "{} evaluates to nil" << std::endl;
        std::cout << "{{}} evaluates to a nil inside a table" << std::endl;
        std::cout << "{5} evaluates to 5 being inside a table" << std::endl;
        std::cout << "{LuaVal(5)} evaluates to just 5 instead of being a 5 inside a table" << std::endl;

        // Watch out for quirks though as seen in the serialized output: http://stackoverflow.com/questions/26947704/implicit-conversion-failure-from-initializer-list
        LuaVal nested = { {}, {{}}, { 3 }, { LuaVal(4) } };
        std::cout << nested.dumps() << std::endl;
        std::cout << std::endl;
    }

    {
        std::cout << "test accessing table with [] operator" << std::endl;
        std::cout << "Note that table keys cannot be accessed!" << std::endl;
        std::cout << "Notice the excessive amount of nils left behind!" << std::endl;

        LuaVal table(TTABLE);
        table.set(1, "test");
        std::cout << table.dumps() << std::endl;
        table.set(1, LuaVal()); // removing value through setting it to nil
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
        LuaVal val2;
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
    return 0;
}
