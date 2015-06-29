#ifndef SMALLFOLK_H
#define SMALLFOLK_H

#include <string>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <map>
#include <unordered_map>
#include <cmath>
#include <memory>
#include <cassert>
#include <functional>
#include <exception>
#include <stdarg.h>
#include <stdio.h>

class smallfolk_exception : public std::exception
{
public:
    static size_t const size = 2048;

    smallfolk_exception(const char * format, ...) : std::exception()
    {
        char buffer[size];
        va_list args;
        va_start(args, format);
        vsnprintf_s(buffer, size, format, args);
        errmsg = buffer;
        va_end(args);
    }

    virtual const char* what() const throw()
    {
        return errmsg.c_str();
    }

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
    static LuaVal const & nil()
    {
        static LuaVal const nil;
        return nil;
    }

    // returns the string representation of the value info similar to lua tostring
    std::string tostring() const
    {
        switch (tag)
        {
            case TBOOL:
                if (b)
                    return "1";
                else
                    return "0";
            case TNIL:
                return "nil";
            case TSTRING:
                return s;
            case TNUMBER:
                return tostring(d);
            case TTABLE:
                return tostring(hash_table);
            default:
                break;
        }
        throw smallfolk_exception("tostring invalid or unhandled tag %i", tag);
        return std::string();
    }

    // returns the string representation of the value info
    size_t hash() const
    {
        return hash_val;
    }

    static struct LuaValHasher
    {
        size_t operator()(LuaVal const& v) const
        {
            return v.hash();
        }
    };

    typedef std::unordered_map< LuaVal, LuaVal, LuaValHasher> HashTable;
    typedef std::shared_ptr< HashTable > LuaTable;

    LuaVal(const LuaTypeTag tag) : tag(tag), hash_table(tag == TTABLE ? new HashTable : nullptr), d(0), b(false)
    {
        if (istable() && !hash_table)
            throw smallfolk_exception("creating table LuaVal with nullptr table");
        makehash();
    }
    LuaVal() : tag(TNIL), hash_table(nullptr), d(0), b(false)
    {
        makehash();
    }
    LuaVal(const long d) : tag(TNUMBER), hash_table(nullptr), d(d), b(false)
    {
        makehash();
    }
    LuaVal(const unsigned long d) : tag(TNUMBER), hash_table(nullptr), d(d), b(false)
    {
        makehash();
    }
    LuaVal(const int d) : tag(TNUMBER), hash_table(nullptr), d(d), b(false)
    {
        makehash();
    }
    LuaVal(const unsigned int d) : tag(TNUMBER), hash_table(nullptr), d(d), b(false)
    {
        makehash();
    }
    LuaVal(const float d) : tag(TNUMBER), hash_table(nullptr), d(d), b(false)
    {
        makehash();
    }
    LuaVal(const double d) : tag(TNUMBER), hash_table(nullptr), d(d), b(false)
    {
        makehash();
    }
    LuaVal(const std::string& s) : tag(TSTRING), hash_table(nullptr), s(s), d(0), b(false)
    {
        makehash();
    }
    LuaVal(const char* s) : tag(TSTRING), hash_table(nullptr), s(s), d(0), b(false)
    {
        makehash();
    }
    LuaVal(const bool b) : tag(TBOOL), hash_table(nullptr), b(b), d(0)
    {
        makehash();
    }
    LuaVal(LuaTable hashtable) : tag(TTABLE), hash_table(hashtable), d(0), b(false)
    {
        if (!hash_table)
            throw smallfolk_exception("creating table LuaVal with nullptr table");
        makehash();
    }

    bool isstring() const { return tag == TSTRING; }
    bool isnumber() const { return tag == TNUMBER; }
    bool istable() const { return tag == TTABLE; }
    bool isbool() const { return tag == TBOOL; }
    bool isnil() const { return tag == TNIL; }

    // create a LuaTable (same as LuaTable(new HashTable());)
    static LuaTable makeluatable()
    {
        return LuaTable(new HashTable());
    }

    // create a table (same as LuaVal(TTABLE);)
    static LuaVal maketable(LuaTable arr = LuaTable(new HashTable()))
    {
        return LuaVal(arr);
    }

    // settable, return self
    LuaVal set(LuaVal const & k, LuaVal const & v)
    {
        if (!istable())
            throw smallfolk_exception("using set on non table object");
        if (k.isnil()) // on nil key do nothing
            return *this;
        HashTable & tbl = (*hash_table);
        if (v.isnil()) // on nil value erase key
            tbl.erase(k);
        else
            tbl[k] = v; // normally set pair
        return *this;
    }

    // gettable - note, adds key-nil pair if not existing
    // nil key throws error
    LuaVal operator [](LuaVal const & k) const
    {
        if (!istable())
            throw smallfolk_exception("using [] on non table object");
        if (k.isnil())
            throw smallfolk_exception("using [] with nil key");
        if (k.isnil()) // on nil key do nothing
            return nil();
        HashTable & tbl = (*hash_table);
        return tbl[k];
    }

    // get-set-table - note, adds key-nil pair if not existing
    // nil key throws error
    LuaVal & operator [](LuaVal const & k)
    {
        if (!istable())
            throw smallfolk_exception("using [] on non table object");
        if (k.isnil())
            throw smallfolk_exception("using [] with nil key");
        HashTable & tbl = (*hash_table);
        return tbl[k];
    }

    // gettable
    LuaVal get(LuaVal const & k) const
    {
        if (!istable())
            throw smallfolk_exception("using get on non table object");
        if (k.isnil()) // on nil key do nothing
            return nil();
        HashTable & tbl = (*hash_table);
        auto it = tbl.find(k);
        if (it != tbl.end())
            return it->second;
        return nil();
    }

    // get a number value
    double num() const
    {
        if (!isnumber())
            throw smallfolk_exception("using num on non number object");
        return d;
    }
    // get a string value
    std::string str() const
    {
        if (!isstring())
            throw smallfolk_exception("using str on non string object");
        return s;
    }
    // get a cstring value
    const char* cstr() const
    {
        if (!isstring())
            throw smallfolk_exception("using cstr on non string object");
        return s.c_str();
    }
    // get a boolean value
    bool boolean() const
    {
        if (!isbool())
            throw smallfolk_exception("using boolean on non bool object");
        return b;
    }
    // get a table value
    LuaTable tbl() const
    {
        if (!istable())
            throw smallfolk_exception("using tbl on non table object");
        return hash_table;
    }

    // serializes the value into string
    // errmsg is optional value to output error message to on failure
    // returns empty string on error
    std::string dumps(std::string* errmsg = nullptr) const
    {
        try
        {
            ACC acc;
            acc << std::setprecision(17); // min lua percision
            unsigned int nmemo = 0;
            MEMO memo;
            dump_object(*this, nmemo, memo, acc);
            return acc.str();
        }
        catch (std::exception& e)
        {
            if (errmsg)
            {
                *errmsg += "Smallfolk_cpp error: ";
                *errmsg += e.what();
            }
        }
        catch (...)
        {
            if (errmsg)
                *errmsg += "Smallfolk_cpp error";
        }
        return std::string();
    }

    // deserialize a string into a LuaVal
    // string param is deserialized string
    // errmsg is optional value to output error message to on failure
    // maxsize is optional max length for the deserialized string
    static LuaVal loads(std::string const & string, std::string* errmsg = nullptr, size_t maxsize = 10000)
    {
        try
        {
            if (string.length() > maxsize)
                return nil();
            TABLES tables;
            size_t i = 0;
            return expect_object(string, i, tables);
        }
        catch (std::exception& e)
        {
            if (errmsg)
            {
                *errmsg += "Smallfolk_cpp error: ";
                *errmsg += e.what();
            }
        }
        catch (...)
        {
            if (errmsg)
                *errmsg += "Smallfolk_cpp error";
        }
        return nil();
    }

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
                return hash_table == rhs.hash_table;
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
        return !isnil() || (isbool() && boolean());
    }

private:
    typedef std::vector<LuaVal> TABLES;
    typedef std::unordered_map<LuaVal, unsigned int, LuaValHasher> MEMO;
    typedef std::stringstream ACC;

    LuaTypeTag tag;

    LuaTable hash_table;
    size_t hash_val;
    std::string s;
    // int64_t i; // lua 5.3 support?
    double d;
    bool b;

    void makehash()
    {
        hash_val = std::hash<std::string>()(tostring());
    }

    // sprintf is ~50% faster than other solutions
    static std::string tostring(const double d)
    {
        char arr[128];
        sprintf_s(arr, "%.17g", d);
        return arr;
    }
    static std::string tostring(LuaTable ptr)
    {
        char arr[128];
        sprintf_s(arr, "table: %p", ptr);
        return arr;
    }

    static size_t dump_type_table(LuaVal const & object, unsigned int nmemo, MEMO& memo, ACC& acc)
    {
        if (!object.istable())
            throw smallfolk_exception("using dump_type_table on non table object");

        auto it = memo.find(object);
        if (it != memo.end())
        {
            acc << '@' << it->second;
            return nmemo;
        }
        memo[object] = ++nmemo;
        acc << '{';
        std::map<unsigned int, const LuaVal*> arr;
        std::unordered_map<const LuaVal*, const LuaVal*> hash;
        for (auto&& v : *object.hash_table)
        {
            if (v.first.isnumber() && v.first.num() >= 1 && std::floor(v.first.num()) == v.first.num())
                arr[static_cast<unsigned int>(v.first.num())] = &v.second;
            else
                hash[&v.first] = &v.second;
        }
        unsigned int i = 1;
        for (auto&& v : arr)
        {
            if (v.first != i)
            {
                nmemo = dump_object(v.first, nmemo, memo, acc);
                acc << ':';
            }
            else
                ++i;
            nmemo = dump_object(*v.second, nmemo, memo, acc);
            acc << ',';
        }
        for (auto&& v : hash)
        {
            nmemo = dump_object(*v.first, nmemo, memo, acc);
            acc << ':';
            nmemo = dump_object(*v.second, nmemo, memo, acc);
            acc << ',';
        }
        std::string l = acc.str();
        char c = strat(l, l.length() - 1);
        if (c != '{')
        {
            // remove last char
            l.pop_back();
            acc.clear();
            acc.str(std::string());
            acc << l;
        }
        acc << '}';
        return nmemo;
    }

    static size_t dump_object(LuaVal const & object, unsigned int nmemo, MEMO& memo, ACC& acc)
    {
        switch (object.tag)
        {
            case TBOOL:
                acc << (object.b ? 't' : 'f');
                break;
            case TNIL:
                acc << 'n';
                break;
            case TSTRING:
                acc << '"';
                acc << escape_quotes(object.s); // change to std::quote() in c++14?
                acc << '"';
                break;
            case TNUMBER:
                if (!isfinite(object.d))
                {
                    // slightly ugly :(
                    std::string nn = tostring(object.d);
                    if (nn == "1.#INF")
                        acc << 'I';
                    else if (nn == "-1.#INF")
                        acc << 'i';
                    else if (nn == "1.#IND")
                        acc << 'i';
                    else if (nn == "-1.#IND")
                        acc << 'N';
                    else
                        acc << 'Q';
                }
                else
                    acc << object.d;
                break;
            case TTABLE:
                return dump_type_table(object, nmemo, memo, acc);
                break;
            default:
                throw smallfolk_exception("dump_object invalid or unhandled tag %i", object.tag);
                break;
        }
        return nmemo;
    }

    static std::string escape_quotes(const std::string &before)
    {
        std::string after;
        after.reserve(before.length() + 4);

        for (std::string::size_type i = 0; i < before.length(); ++i)
        {
            switch (before[i])
            {
                case '"':
                    after += '"'; // no break
                default:
                    after += before[i];
            }
        }

        return after;
    }

    static std::string unescape_quotes(const std::string &before)
    {
        std::string after;
        after.reserve(before.length());

        for (std::string::size_type i = 0; i < before.length(); ++i)
        {
            switch (before[i])
            {
                case '"':
                    if (i + 1 < before.length() && before[i + 1] == '"')
                        ++i;
                default:
                    after += before[i];
            }
        }

        return after;
    }

    static bool nonzero_digit(char c)
    {
        switch (c)
        {
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
            case '8':
            case '9':
                return true;
        }
        return false;
    }

    static bool is_digit(char c)
    {
        switch (c)
        {
            case '0':
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
            case '8':
            case '9':
                return true;
        }
        return false;
    }

    static char strat(std::string const & string, std::string::size_type i)
    {
        if (i != std::string::npos &&
            i < string.length() &&
            i >= 0)
            return string.at(i);
        return '\0'; // bad?
    }

    static LuaVal expect_number(std::string const & string, size_t& start)
    {
        size_t i = start;
        char head = strat(string, i);
        if (head == '-')
            head = strat(string, ++i);
        if (nonzero_digit(head))
        {
            do
            {
                head = strat(string, ++i);
            } while (is_digit(head));
        }
        else if (head == '0')
            head = strat(string, ++i);
        else
            throw smallfolk_exception("expect_number at %u unexpected character %c", i, head);
        if (head == '.')
        {
            size_t oldi = i;
            do
            {
                head = strat(string, ++i);
            } while (is_digit(head));
            if (i == oldi + 1)
                throw smallfolk_exception("expect_number at %u no numbers after decimal", i);
        }
        if (head == 'e' || head == 'E')
        {
            head = strat(string, ++i);
            if (head == '+' || head == '-')
                head = strat(string, ++i);
            if (!is_digit(head))
                throw smallfolk_exception("expect_number at %u not a digit part %c", i, head);
            do
            {
                head = strat(string, ++i);
            } while (is_digit(head));
        }
        size_t temp = start;
        start = i;
        return std::atof(string.substr(temp, i).c_str());
    }

    static LuaVal expect_object(std::string const & string, size_t& i, TABLES& tables)
    {
        static double _zero = 0.0;

        char cc = strat(string, i++);
        switch (cc)
        {
            case 't':
                return true;
            case 'f':
                return false;
            case 'n':
                return nil();
            case 'Q':
                return -(0 / _zero);
            case 'N':
                return (0 / _zero);
            case 'I':
                return (1 / _zero);
            case 'i':
                return -(1 / _zero);
            case '"':
            {
                size_t nexti = i - 1;
                do
                {
                    nexti = string.find('"', nexti + 1);
                    if (nexti == std::string::npos)
                    {
                        throw smallfolk_exception("expect_object at %u was %c eof before string ends", i, cc);
                    }
                    ++nexti;
                } while (strat(string, nexti) == '"');
                size_t temp = i;
                i = nexti;
                return unescape_quotes(string.substr(temp, nexti - temp - 1));
            }
            case '0':
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
            case '8':
            case '9':
            case '-':
            case '.':
                return expect_number(string, --i);
            case '{':
            {
                LuaVal nt(TTABLE);
                unsigned int j = 1;
                tables.push_back(nt);
                if (strat(string, i) == '}')
                {
                    ++i;
                    return nt;
                }
                while (true)
                {
                    LuaVal k = expect_object(string, i, tables);
                    if (strat(string, i) == ':')
                    {
                        nt.set(k, expect_object(string, ++i, tables));
                    }
                    else
                    {
                        nt.set(j, k);
                        ++j;
                    }
                    char head = strat(string, i);
                    if (head == ',')
                        ++i;
                    else if (head == '}')
                    {
                        ++i;
                        return nt;
                    }
                    else
                    {
                        throw smallfolk_exception("expect_object at %u was %c unexpected character %c", i, cc, head);
                    }
                }
                break;
            }
            case '@':
            {
                std::string::size_type x = i;
                for (; x < string.length(); ++x)
                {
                    if (!isdigit(string[x]))
                        break;
                }
                std::string substr = string.substr(i, x - i);
                size_t index = std::stoul(substr.c_str());
                if (index >= 1 && index <= tables.size())
                {
                    i += substr.length();
                    return tables[index - 1];
                }

                throw smallfolk_exception("expect_object at %u was %c invalid index %u", i, cc, index);
                break;
            }
            default:
            {
                throw smallfolk_exception("expect_object at %u was %c", i, cc);
                break;
            }
        }
        return nil();
    }
};

#endif
