#ifndef SMALLFOLK_H
#define SMALLFOLK_H

#include <string>
#include <vector>
#include <unordered_map>
#include <memory> // std::unique_ptr
#include <stdexcept> // std::logic_error
#include <cstddef> // size_t
#inlcude <utility> // std::move

class smallfolk_exception : public std::logic_error
{
public:
    static size_t const size = 2048;

    smallfolk_exception(const char * format, ...);
    virtual const char* what() const throw();

    std::string errmsg;
};

enum LuaTypeTag
{
    TNIL,
    TSTRING,
    TNUMBER,
    TTABLE,
    TBOOL,
};

class LuaVal
{
public:

    // static nil value, same as LuaVal();
    // You can use it as for example as default const reference
    static LuaVal const nil;

    // returns the string representation of the value info similar to lua tostring
    std::string tostring() const;

    // use as the hasher for containers, for example std::unordered_map<LuaVal, int, LuaVal::LuaValHasher>
    struct LuaValHasher
    {
        size_t operator()(LuaVal const & v) const;
    };

    typedef std::unordered_map<LuaVal, LuaVal, LuaValHasher> LuaTable;
    typedef std::unique_ptr<LuaTable> TblPtr; // circular reference memleak if insert self to self

    LuaVal(const LuaTypeTag tag);
    LuaVal();
    LuaVal(const long d);
    LuaVal(const unsigned long d);
    LuaVal(const int d);
    LuaVal(const unsigned int d);
    LuaVal(const float d);
    LuaVal(const double d);
    LuaVal(const std::string& s);
    LuaVal(const char* s);
    LuaVal(const bool b);
    LuaVal(LuaTable const & luatable);
    LuaVal(LuaVal const & val);
    LuaVal(LuaVal && val);
    LuaVal(std::initializer_list<LuaVal const> const & l);

    bool isstring() const { return tag == TSTRING; }
    bool isnumber() const { return tag == TNUMBER; }
    bool istable() const { return tag == TTABLE; }
    bool isbool() const { return tag == TBOOL; }
    bool isnil() const { return tag == TNIL; }

    // create a table (same as LuaVal(TTABLE);)
    static LuaVal table(LuaTable arr = LuaTable()) { return LuaVal(arr); }

    // gettable, adds key-nil pair if not existing
    // nil key throws error
    LuaVal & operator[](LuaVal const & k);
    // gettable
    LuaVal const & get(LuaVal const & k) const;
    // returns true if value was found with key
    bool has(LuaVal const & k) const;
    // settable, return self
    LuaVal & set(LuaVal const & k, LuaVal const & v);
    // erase, return self
    LuaVal & rem(LuaVal const & k);
    // table array size, not actual element count
    unsigned int len() const;
    // table.insert, return self
    LuaVal & insert(LuaVal const & v, LuaVal const & pos = nil);
    // table.remove, return self
    LuaVal & remove(LuaVal const & pos = nil);

    // get a number value
    double num() const
    {
        if (!isnumber())
            throw smallfolk_exception("using num on non number object");
        return d;
    }
    // get a boolean value
    bool boolean() const
    {
        if (!isbool())
            throw smallfolk_exception("using boolean on non bool object");
        return b;
    }
    // get a string value
    std::string const & str() const
    {
        if (!isstring())
            throw smallfolk_exception("using str on non string object");
        return s;
    }
    // get a table value
    LuaTable const & tbl() const
    {
        if (!istable())
            throw smallfolk_exception("using tbl on non table object");
        return *tbl_ptr;
    }

    LuaTypeTag GetTypeTag() const { return tag; }

    // serializes the value into string
    // errmsg is optional value to output error message to on failure
    // returns empty string on error
    std::string dumps(std::string* errmsg = nullptr) const;

    // deserialize a string into a LuaVal
    // string param is deserialized string
    // errmsg is optional value to output error message to on failure
    static LuaVal loads(std::string const & string, std::string* errmsg = nullptr);

    bool operator==(LuaVal const& rhs) const;
    bool operator!=(LuaVal const& rhs) const { return !(*this == rhs); }

    // You can use !val to check for nil or false
    operator bool() const;

    LuaVal& operator=(LuaVal const& val);
    LuaVal& operator=(LuaVal && val)
    {
        tag = std::move(val.tag);
        tbl_ptr = std::move(val.tbl_ptr);
        s = std::move(val.s);
        d = std::move(val.d);
        b = std::move(val.b);
        return *this;
    }

private:

    LuaTypeTag tag;
    TblPtr tbl_ptr;
    std::string s;
    // int64_t i; // lua 5.3 support?
    double d;
    bool b;
};

#endif
