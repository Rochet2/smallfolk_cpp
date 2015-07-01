#smallfolk_cpp

Insipred and supposedly works with the pure lua serializer Smallfolk.
Most serializer logic is borrowed from gvx/Smallfolk.
https://github.com/gvx/Smallfolk

Smallfolk_cpp is a C++ serializer and deserializer. It was created to be used with smallfolk lua serializer.
Smallfolk_cpp does not have dependencies and does not need lua. It simply uses same format and logic as smallfolk and produces same data when serializing and can use the same data to deserialize that smallfolk uses.
Smallfolk_cpp can be used for other serializing purposes as well.

Smallfolk_cpp has its own type to represent lua values in C++.
They allow representing bool, number, string, nil and table.

You use, distribute and extend Smallfolk_cpp under the terms of the MIT license.

- [smallfolk_cpp](#smallfolk_cpp)
	- [Usage](#Usage)
	- [Fast](#Fast)
	- [Table cycles](#Table cycles)
	- [Security](#Security)
	- [Tested](#Tested)
	- [Reference](#Reference)
		- [Try-catch](#Try-catch)
		- [(de)serializing](#(de)serializing)
		- [LuaVal constructors](#LuaVal constructors)
		- [static nil](#static nil)
		- [hash](#hash)
		- [typetag](#typetag)
		- [tostring](#tostring)
		- [operators](#operators)
		- [isvalue](#isvalue)
		- [LuaVal values](#LuaVal values)
		- [table functions](#table functions)

##Usage

```C++
#include smallfolk.h

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
```

##Fast

Its C++, duh!?

Some poor benchmarking shows that plain serializing takes ~0.01ms. If creating, serializing and destroying created objects ~0.025ms. Deserializing takes ~0.05ms when destroying the created objects as well.
This is ofcourse completely different depending on what data you serialize and deserialize.
In general it would seem that deserializing is ~50% slower.

To put this into any kind of perspective, here is the print of the serialized data:
```
{t,"somestring",123.456,t:-678,"test":123.45600128173828,f:268435455,"subtable":{1,2,3}}
```

##Table cycles

**Note: This feature was disabled cause of difficult implementing in C++ and possibly unwanted infinite cycles. All table assigning create copies now in the C++ code and no @ notation is recognised for serializing or deserializing.**

From original smallfolk
> Sometimes you have strange, non-euclidean geometries in your table
> constructions. It happens, I don't judge. Smallfolk can deal with that, where
> some other serialization libraries (or anything that produces JSON) cry "Iä!
> Iä! Cthulhu fhtagn!" and give up &mdash; or worse, silently produce incorrect
> data.

```C++
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

##Security

No comments.

##Tested

Tested very little.
_Should_ contain no crashes or memory leaks.

##Reference

###Try-catch
Most functions can throw smallfolk_exception and some string library errors and more.
One method for try catching errors you can use is this:
```C++
const char* errmsg = nullptr;
try {
  // code
}
catch (std::exception& e) {
  errmsg = e.what();
}
catch (...) {
  errmsg = "Smallfolk_cpp error";
}
```

###(de)serializing
The main functionality is provided by `luaval.dumps(std::string* errmsg)` and `LuaVal::loads(std::string const & string, std::string* errmsg)`.
Dumps returns a string serialization from the luaval it is called on. Loads returns a LuaVal from the string passed to it.
Errmsg is an optional pointer to a valid string object that is filled with the error message if any.
The functions have a try-catch in place and should never throw.

###LuaVal constructors
Constructors allow implicit and you can make all possible values by giving it to the constructor.
Constructors do not throw in normal circumstances.
```C++
LuaVal implicit_test = -123;
LuaVal copy_test(implicit_test);
LuaVal copy_test2 = implicit_test;
LuaVal n; // nil
LuaVal b(true);
LuaVal s("somestring");
LuaVal d(123.456);
LuaVal f(123.456f);
LuaVal i(-678);
LuaVal u(0xFFFFFFF);
LuaVal t(TTABLE);
LuaVal t2 = LuaVal::table();
```

###static nil
A static function `LuaVal::nil()` returns a const reference to a preconstructed nil object.
It is used as a return value when a nil value is needed, but it is identical to any other constructed LuaVal that represents nil.
This function does not throw.

###hash
The LuaVal class contains a hasher struct `LuaVal::LuaValHasher` for when you need to use a LuaVal in a hash container for example: `std::unordered_set<LuaVal, LuaVal::LuaValHasher> myset;`.
The hasher uses `LuaVal::tostring()`.

###typetag
There is a definition of type tags used to identify each value type. These can be used in the constructor as well.
For example a table can be created with `LuaValue table(TTABLE)`. You can get the type tag of an object with the member function `luaval.GetTypeTag()`.
GetTypeTag does not throw.
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

###tostring
The member function `luaval.tostring()` returns a string representation of the object. This is similar to tostring in lua.
This function does not throw in normal circumstances.

###operators
The LuaVal class offers a few operators.  
You can use == and != operators to compare, howevever different table objects are copies so they are never equal unless you actually compare with the same object.
LuaVal has the bool operator implemented so that nil and false will return false. Any other object returns true, just like in lua. The assignment operator is also implemented and works as you would expect.
These operators do not throw in normal circumstances.

###isvalue
There is a collection of functions you can use to check whether the object is really of some type.
These functions do not throw.
```C++
luaval.isstring()
luaval.isnumber()
luaval.istable()
luaval.isbool()
luaval.isnil()
```

###LuaVal values
`LuaVal::LuaTable` is used inside a lua table as the storage for values.
You can create one and use it to initialize a lua table object. You can get the LuaTable from a LuaVal with the `luaval.tbl()` member function.  
Since C++ is not able to return _any_ value, there is a set of functions to get the actual value of a LuaVal.
The functions will throw if you use them on the wrong type object.
```C++
luaval.num()
luaval.str()
luaval.boolean()
luaval.tbl()
```

###table functions
LuaVal representing a table offers two ways of getting and setting values.  
You can use lua table like a map. It offers the `[]` operator for accessing an element by key. However the operator creates nils as values by default if the key does not exist. Also it throws if you use nil as a key or use it on a non table object.
Example usage:
```C++
LuaVal table(TTABLE);
table[1] = "test";
table[2] = 77.234;
table[3] = -324;
table[false] = table[2];
table["self copy"] = table;
table[table] = "table as key?";
std::cout << table["self copy"][3].num() << std::endl;
```

The second way of accessing and inserting map elements are the get and set member functions `luaval.get(key)`, `luaval.set(key, value)`.
The function `set` returns the table, so you can chain it to set multiple values.
These functions do not throw unless you use them on non table objects. They also do not create default values for nonexisting keys and when a value is set as nil, it will be erased.
An additional method for erasing data with a key is `luaval.rem(key)` which also returns the table and throws only when used on a non table object.
Example usage:
```C++
LuaVal table(TTABLE);
table.set(1, "test").set(2, 77.234).set(3, -324);
table.set(false, table.get(2));
table.set("self copy", table);
table.set(table, "table as key?");
std::cout << table.get("self copy").get(3).num() << std::endl;
table.rem(3).rem(2);
```

For conveniency tables also have the methods `luaval.insert(value[, pos])`, `luaval.remove([pos])` and `luaval.len()`.
The len function returns the number of consecutive integer key elements in the table starting at index 1. It is similar to the # operator in lua.
Insert and remove shift the values on the right side of the given position and insert or remove a value to or at the given position. If position is omitted, the value is inserted to the end of the list or the last element is removed.
Insert and remove both return self.
Each function throws if used on a non table object or pos is not valid.
