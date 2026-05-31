#include "smallfolk.h"
#include "smallfolk_convert.h"

#include <cassert>
#include <deque>
#include <forward_list>
#include <iostream>
#include <string>

int main()
{
    {
        std::cout << "Test values" << std::endl;
        LuaVal merge_right = LuaVal::table();
        merge_right.set(LuaVal("ke"), LuaVal("test"));
        merge_right.set(LuaVal("ke2"), LuaVal("test"));
        LuaVal asd = {
            LuaVal("number"),
            LuaVal("string"),
            LuaVal("table"),
            LuaVal::mrg(LuaVal{ LuaVal("number"), LuaVal("string") }, merge_right)
        };
        std::cout << asd.dumps() << std::endl;
        std::string err;
        LuaVal v = LuaVal::loads(" { 1 , 2 , { 3 , 4, ' k e ' : ' t e s t ' } } ", &err);
        std::cout << v.dumps(&err) << std::endl;
        std::cout << err << std::endl;

        std::cout << "Testing different double corner values" << std::endl;
        double zero = 0.0;
        LuaVal tn = { -(zero / zero), (zero / zero), (1.0 / zero), -(1.0 / zero) };
        std::cout << tn.dumps() << std::endl;
        std::cout << tn.get(1).tostring() << " " << tn.get(2).tostring() << " "
                  << tn.get(3).tostring() << " " << tn.get(4).tostring() << std::endl;
        tn = LuaVal::loads(tn.dumps());
        std::cout << tn.get(1).tostring() << " " << tn.get(2).tostring() << " "
                  << tn.get(3).tostring() << " " << tn.get(4).tostring() << std::endl;
        std::cout << std::endl;
    }

    {
        std::cout << "Testing creation testing and printing of all value types" << std::endl;
        LuaVal implicit_test = -123;
        LuaVal copy_test(implicit_test);
        LuaVal copy_test2 = implicit_test;
        LuaVal n = LuaVal::nil;
        LuaVal n2(TNIL);
        LuaVal b(true);
        LuaVal s("somestring");
        LuaVal d(123.456);
        LuaVal f(123.456f);
        LuaVal i(-678);
        LuaVal u(0xFFFFFFFFu);
        LuaVal t;
        LuaVal t2 = LuaVal::table();
        LuaVal t3 = { LuaVal(1), LuaVal(2), LuaVal(3) };
        LuaVal t4 = {};
        LuaVal t5(TTABLE);

        assert(implicit_test.isnumber());
        assert(copy_test.isnumber());
        assert(copy_test2.isnumber());
        assert(n.isnil());
        assert(n2.isnil());
        assert(b.isbool());
        assert(s.isstring());
        assert(d.isnumber());
        assert(f.isnumber());
        assert(i.isnumber());
        assert(u.isnumber());
        assert(t.istable());
        assert(t2.istable());
        assert(t3.istable());
        assert(t4.istable());
        assert(t5.istable());

        std::cout << implicit_test.tostring() << std::endl;
        std::cout << copy_test.tostring() << std::endl;
        std::cout << copy_test2.tostring() << std::endl;
        assert(n.tostring() == "nil");
        assert(b.tostring() == "true");
        assert(s.tostring() == "somestring");
        std::cout << std::endl;
    }

    {
        std::cout << "Testing exception handling" << std::endl;
        std::string errmsg;
        try
        {
            LuaVal h(-7);
            (void)h.str();
        }
        catch (smallfolk_exception const & e)
        {
            errmsg = e.what();
        }
        if (!errmsg.empty())
            std::cout << errmsg << std::endl << std::endl;
    }

    {
        std::cout << "Example usage" << std::endl;

        LuaVal table = LuaVal::table();
        table.set(1, "Hello");
        table.set(std::string("test"), std::string("world"));
        table.set(LuaVal(67.5), -234.5);

        std::string serialized = table.dumps();
        std::cout << serialized << std::endl;

        LuaVal deserialized = LuaVal::loads(serialized);
        std::cout << deserialized.get(1).str() << " " << deserialized.get(std::string("test")).str()
                  << " " << deserialized.get(LuaVal(67.5)).num() << std::endl;
        std::cout << std::endl;
    }

    {
        std::cout << "Table initializer list coolness" << std::endl;
        LuaVal nested = { LuaVal::table(), { LuaVal::table() }, { LuaVal(3) }, { LuaVal(4) } };
        std::cout << nested.dumps() << std::endl;
        std::cout << std::endl;
    }

    {
        std::cout << "test accessing table with [] operator" << std::endl;
        LuaVal nested = LuaVal::table();
        nested[1];
        nested[2];
        nested[3];
        nested[4][5][6];
        std::cout << nested.dumps() << std::endl;

        LuaVal table(TTABLE);
        table.set(1, "test");
        std::cout << table.dumps() << std::endl;
        table.set(1, LuaVal::nil);
        std::cout << table.dumps() << std::endl;
        table.set(table, "table as key?");
        std::cout << table.dumps() << std::endl;
        std::cout << table.get(table).tostring() << std::endl;
        std::cout << std::endl;
    }

    {
        std::cout << "test .(key).(key, val).rem(key)" << std::endl;
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
        table.set(std::string("rand"), LuaVal(893));
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
        assert(val);
        assert(!val2);
        assert(val == 5);
        assert(val != 6);
        std::cout << "comparison and bool() checks passed" << std::endl;
    }

    {
        LuaVal val(TTABLE);
        val[123] = 5;
        val[LuaVal("test")] = 5;
        val[LuaVal(1.5)] = 5;
    }

    {
        LuaVal merge_right = LuaVal::table();
        merge_right.set(LuaVal("ke"), LuaVal("test"));
        LuaVal val1 = {
            LuaVal(1),
            LuaVal(2),
            LuaVal::mrg(LuaVal{ LuaVal(3), LuaVal(4) }, merge_right)
        };
        LuaVal val2 = LuaVal::loads("{1,2,{3,4,'ke':'test'}}");
        assert(val1.dumps() == val2.dumps());
        std::cout << val1.dumps() << std::endl;
    }

    {
        LuaVal t1 = { LuaVal(1), LuaVal(2), { LuaVal(1), LuaVal(2), LuaVal(3) } };
        LuaVal t2 = LuaVal::table();
        t2.set(LuaVal("key"), LuaVal("value"));
        t2.set(LuaVal(2), LuaVal("value2"));
        std::cout << t1.dumps() << std::endl;
        std::cout << t2.dumps() << std::endl;
    }

    {
        std::forward_list<std::deque<std::string>> rows = { { "a", "b" }, { "a", "b" } };
        LuaVal nested(TTABLE);
        unsigned int index = 0;
        for (auto const & row : rows)
            nested.set(++index, lua_val::array(row));
        std::cout << nested.dumps() << std::endl;
    }

    std::cout << "demo finished successfully" << std::endl;
    return 0;
}
