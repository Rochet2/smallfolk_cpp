// main.cpp : Defines the entry point for the console application.
//

#include "smallfolk.h"

int main()
{
    {
        std::cout << "Test values" << std::endl;

        double _zero = 0.0;
        LuaVal tn = LuaVal::table();
        tn[1] = -(0 / _zero);
        tn[2] = (0 / _zero);
        tn[3] = (1 / _zero);
        tn[4] = -(1 / _zero);
        std::cout << tn.dumps() << std::endl;
        std::cout << -(0 / _zero) << " " << (0 / _zero) << " " << (1 / _zero) << " " << -(1 / _zero) << std::endl;
        std::cout << tn[1].tostring() << " " << tn[2].tostring() << " " << tn[3].tostring() << " " << tn[4].tostring() << std::endl;
        std::cout << std::endl;

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
        std::cout << std::endl;

        std::string errmsg;
        try
        {
            LuaVal i(-7);
            std::string str = i.str(); // error
        }
        catch (std::exception& e)
        {
            errmsg = e.what();
        }
        catch (...)
        {
            errmsg = "Smallfolk_cpp error";
        }
        if (!errmsg.empty())
            std::cout << errmsg << std::endl << std::endl;
    }

    {
        std::cout << "Example usage" << std::endl;

        // create a lua table and set some values to it
        LuaVal table = LuaVal::table();
        table[1] = "Hello";
        table["test"] = "world";
        table[67.5] = -234.5;

        // serialize the table
        std::string serialized = table.dumps();

        // print the serialization, it should be rather human readable
        std::cout << serialized << std::endl;
        // Example output: {"Hello","test":"world",67.5:-234.5}

        // form lua values from the string
        LuaVal deserialized = LuaVal::loads(serialized);

        // print the values from deserialized result table
        std::cout << deserialized[1].str() << " " << deserialized["test"].str() << " " << deserialized[67.5].num() << std::endl;
        // Example output: Hello world -234.5
        std::cout << std::endl;
    }

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
        std::cout << "test []" << std::endl;

        LuaVal table(TTABLE);
        table[1] = "test";
        table[2] = 77.234;
        table[3] = -324;
        table[false] = table[2];
        table["self copy"] = table;
        table[table] = "table as key?";
        std::cout << table["self copy"][3].num() << std::endl;
        std::cout << std::endl;
    }

    {
        std::cout << "test .get.set.rem" << std::endl;

        LuaVal table(TTABLE);
        table.set(1, "test").set(2, 77.234).set(3, -324);
        table.set(false, table.get(2));
        table.set("self copy", table);
        table.set(table, "table as key?");
        std::cout << table.get("self copy").get(3).num() << std::endl;
        table.rem(3).rem(2);
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
    return 0;
}
