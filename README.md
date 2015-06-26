smallfolk_cpp
=========

Insipred and supposedly works with the pure lua serializer Smallfolk.
Most serializer logic is borrowed from gvx/Smallfolk.
https://github.com/gvx/Smallfolk

Smallfolk_cpp is a C++ serializer and deserializer for use with smallfolk lua serializer.
Smallfolk_cpp does not have dependencies and does not need lua. It simply uses same format and logic as smallfolk and is thus able to use same strings for deserialization.

Smallfolk_cpp has its own type to represent lua values in C++.
They allow representing bool, number, string, nil and table.

You use, distribute and extend Smallfolk_cpp under the terms of the MIT license.

Usage
-----

```c++
#include smallfolk.h

std::cout << LuaVal(TTABLE).set(1, "Hello").set("world", true).dumps() << std::endl;
std::cout << LuaVal::loads("{\"foo\":\"bar\"}").get("foo").str() << std::endl;
// prints:
// {1:"Hello","world":t}
// bar
```

Fast
----

Its C++, duh!?

Robust
------

> Sometimes you have strange, non-euclidean geometries in your table
> constructions. It happens, I don't judge. Smallfolk can deal with that, where
> some other serialization libraries (or anything that produces JSON) cry "Iä!
> Iä! Cthulhu fhtagn!" and give up &mdash; or worse, silently produce incorrect
> data.

```c++
#include smallfolk.h

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
```

Security
------

No comments.

Tested
------

Tested very little due to copyconvert code.

Reference
---------

-- todo. its 3:33 am atm.
