#ifndef SMALLFOLK_H
#define SMALLFOLK_H

#include <string>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <map>
#include <vector>
#include <unordered_map>
#include <cmath>
#include <memory>
#include <cassert>
#include <functional>
#include <stdexcept>
#include <stdarg.h>
#include <stdio.h>

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
    static LuaVal const & nil();

    // returns the string representation of the value info similar to lua tostring
    std::string tostring() const;

    struct LuaValHasher
    {
        size_t operator()(LuaVal const & v) const
        {
            switch (v.tag)
            {
            case TBOOL:
                return std::hash<bool>()(v.b);
            case TNIL:
                return std::hash<int>()(0);
            case TSTRING:
                return std::hash<std::string>()(v.s);
            case TNUMBER:
                return std::hash<double>()(v.d);
            case TTABLE:
                return std::hash<TblPtr>()(v.tbl_ptr);
            }
            return std::hash<std::string>()(v.tostring());
        }
    };

    typedef std::unordered_map< LuaVal, LuaVal, LuaValHasher> LuaTable;
    typedef std::unique_ptr< LuaTable > TblPtr; // circular reference memleak if insert self to self

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

    bool isstring() const;
    bool isnumber() const;
    bool istable() const;
    bool isbool() const;
    bool isnil() const;

    // create a table (same as LuaVal(TTABLE);)
    static LuaVal table(LuaTable arr = LuaTable());

    // gettable
    LuaVal get(LuaVal const & k) const;

    // settable, return self
    LuaVal & set(LuaVal const & k, LuaVal const & v);

    // erase, return self
    LuaVal & rem(LuaVal const & k);

    // table array size, not actual element count
    unsigned int len() const;

    // table.insert, return self
    LuaVal & insert(LuaVal const & v, LuaVal const & pos = nil());

    // table.remove, return self
    LuaVal & remove(LuaVal const & pos = nil());

    // get a number value
    double num() const;
    // get a boolean value
    bool boolean() const;
    // get a string value
    std::string const & str() const;
    // get a table value
    LuaTable const & tbl() const;
    LuaTypeTag GetTypeTag() const;

    // serializes the value into string
    // errmsg is optional value to output error message to on failure
    // returns empty string on error
    std::string dumps(std::string* errmsg = nullptr) const;

    // deserialize a string into a LuaVal
    // string param is deserialized string
    // errmsg is optional value to output error message to on failure
    static LuaVal loads(std::string const & string, std::string* errmsg = nullptr);

    bool operator==(LuaVal const& rhs) const
    {
        if (tag != rhs.tag)
            return false;
        switch (tag)
        {
        case TBOOL:
            return b == rhs.b;
        case TNIL:
            return true;
        case TSTRING:
            return s == rhs.s;
        case TNUMBER:
            return d == rhs.d;
        case TTABLE:
            return tbl_ptr == rhs.tbl_ptr;
        default:
            throw smallfolk_exception("operator== invalid or unhandled tag %i", tag);
        }
        return false;
    }

    bool operator!=(LuaVal const& rhs) const
    {
        return !(*this == rhs);
    }

    // You can use !val to check for nil or false
    explicit operator bool()
    {
        return !isnil() && (!isbool() || boolean());
    }

    LuaVal& operator=(LuaVal const& val)
    {
        tag = val.tag;
        if (istable())
        {
            tbl_ptr.reset(new LuaTable(*val.tbl_ptr));
        }
        else
            tbl_ptr = nullptr;
        s = val.s;
        d = val.d;
        b = val.b;

        if (istable())
        {
            if (!tbl_ptr)
                throw smallfolk_exception("creating table LuaVal with nullptr table");
        }
        return *this;
    }

    LuaVal& operator=(LuaVal && val)
    {
        tag = std::move(val.tag);
        tbl_ptr = std::move(val.tbl_ptr);
        s = std::move(val.s);
        d = std::move(val.d);
        b = std::move(val.b);

        if (istable())
        {
            if (!tbl_ptr)
                throw smallfolk_exception("creating table LuaVal with nullptr table");
        }
        return *this;
    }

private:
    typedef std::vector<LuaVal> TABLES;
    typedef std::unordered_map<LuaVal, unsigned int, LuaValHasher> MEMO;
    typedef std::stringstream ACC;

    LuaTypeTag tag;
    TblPtr tbl_ptr;
    std::string s;
    // int64_t i; // lua 5.3 support?
    double d;
    bool b;

    // sprintf is ~50% faster than other solutions
    static std::string tostring(const double d);
    static std::string tostring(TblPtr const & ptr);

    static size_t dump_type_table(LuaVal const & object, unsigned int nmemo, MEMO& memo, ACC& acc);
    static size_t dump_object(LuaVal const & object, unsigned int nmemo, MEMO& memo, ACC& acc);
    static std::string escape_quotes(const std::string &before);
    static std::string unescape_quotes(const std::string &before);
    static bool nonzero_digit(char c);
    static bool is_digit(char c);
    static char strat(std::string const & string, std::string::size_type i);
    static LuaVal expect_number(std::string const & string, size_t& start);
    static LuaVal expect_object(std::string const & string, size_t& i, TABLES& tables);
};

#endif
