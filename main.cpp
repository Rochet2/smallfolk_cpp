// main.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include smallfolk.h

int main()
{
    // test values
    LuaVal n; // nil
    LuaVal b(true);
    LuaVal s("somestring");
    LuaVal d(123.456);
    LuaVal f(123.456f);
    LuaVal i(-678);
    LuaVal u(0xFFFFFFF);
    LuaVal t = LuaVal::maketable();
    assert(n.isnil());
    assert(b.isbool());
    assert(s.isstring());
    assert(d.isnumber());
    assert(f.isnumber());
    assert(i.isnumber());
    assert(u.isnumber());
    assert(t.istable());

    // test table
    LuaVal subtable = LuaVal::maketable();
    subtable.set(1, 1);
    subtable.set(2, 2);
    subtable.set(3, 3);
    subtable.set(4, 4);

    LuaVal table = LuaVal::maketable();
    table.set(6.0, "OVERWRITE");
    table.set(6.0, "overwritten");
    table.set(false, 123.0);
    table.set(n, 0xFFFFFFFF);
    table.set("teststring", "testval");
    table.set("TEST", i);
    table.set("subtable", subtable);
    table.set("quote\"instring", n);
    table.set("tableref1", table.tbl());
    table.set("tableref2", table.tbl());
    table.set("tableref3", table.tbl());

    std::string dumped = table.dumps(); // serialize
    LuaVal loaded = LuaVal::loads(dumped); // deserialize

    std::cout << (dumped) << std::endl << std::endl;
    std::cout << (loaded.dumps()) << std::endl; // testing if serialization worked by comparing serialized data

    // on error a LuaVal::nil() is returned. can catch error messages like this:
    std::string errmsg;
    LuaVal ld = LuaVal::loads("", &errmsg);
    std::cout << (errmsg) << std::endl;
    return 0;
}
