
#ifndef SMALLFOLK_H
#define SMALLFOLK_H

#include <string>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <map>
#include <unordered_map>
// #include <cmath>
#include <memory>
#include <cassert>
#include <functional>

#include <stdint.h>

typedef int64_t int64;
typedef int32_t int32;
typedef int16_t int16;
typedef int8_t int8;
typedef uint64_t uint64;
typedef uint32_t uint32;
typedef uint16_t uint16;
typedef uint8_t uint8;

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

enum TTT
{
    TNIL,
    TSTRING,
    TNUMBER,
    TTABLE,
    TBOOL,
};

class TT
{
public:

    static TT const & nil()
    {
        static TT const nil;
        return nil;
    }

    std::string const & tostring() const
    {
        return tostr;
    }

    static struct hash_tt
    {
        size_t operator()(TT const& v) const
        {
            return std::hash<std::string>()(v.tostring());
        }
    };

    //typedef std::map< int64, TT > MARR;
    //typedef std::shared_ptr< MARR > ARR;
    // std::hash<std::unique_ptr<TT>>
    typedef std::unordered_map< TT, TT, hash_tt> MHASHTBL;
    typedef std::shared_ptr< MHASHTBL > HASHTBL;
    typedef std::unordered_map<HASHTBL, size_t> MEMO;
    typedef std::stringstream ACC;

    TT(TTT tag) : tag(tag), hash_table(tag == TTABLE ? new TT::MHASHTBL : nullptr), d(0), b(false)
    {
        makestr();
    }

    TT() : tag(TNIL), hash_table(nullptr), d(0), b(false)
    {
        makestr();
    }
    TT(double d) : tag(TNUMBER), hash_table(nullptr), d(d), b(false)
    {
        makestr();
    }
    TT(int32 d) : tag(TNUMBER), hash_table(nullptr), d(d), b(false)
    {
        makestr();
    }
    TT(uint32 d) : tag(TNUMBER), hash_table(nullptr), d(d), b(false)
    {
        makestr();
    }
    TT(std::string const & s) : tag(TSTRING), hash_table(nullptr), s(s), d(0), b(false)
    {
        makestr();
    }
    TT(const char* s) : tag(TSTRING), hash_table(nullptr), s(s), d(0), b(false)
    {
        makestr();
    }
    TT(bool b) : tag(TBOOL), hash_table(nullptr), b(b), d(0)
    {
        makestr();
    }
    TT(HASHTBL arr) : tag(TTABLE), hash_table(arr), d(0), b(false)
    {
        makestr();
    }

    bool isstring() const { return tag == TSTRING; }
    bool isnumber() const { return tag == TNUMBER; }
    bool istable() const { return tag == TTABLE; }
    bool isbool() const { return tag == TBOOL; }
    bool isnil() const { return tag == TNIL; }

    // settable
    void set(TT const & k, TT const & v)
    {
        assert(istable());
        MHASHTBL & tbl = (*hash_table);
        tbl[k] = v;
    }

    // gettable
    TT get(TT const & k)
    {
        assert(istable());
        MHASHTBL & tbl = (*hash_table);
        auto it = tbl.find(k);
        if (it != tbl.end())
            return it->second;
        return nil();
    }

    double num() const
    {
        assert(isnumber());
        return d;
    }
    std::string str() const
    {
        assert(isstring());
        return s;
    }
    const char* cstr() const
    {
        assert(isstring());
        return s.c_str();
    }
    bool boolean() const
    {
        assert(isbool());
        return b;
    }
    TT::HASHTBL tbl() const
    {
        assert(istable());
        return hash_table;
    }

    std::string dumps() const
    {
        ACC acc;
        acc << std::setprecision(17); // min lua percision
        size_t nmemo = 0;
        MEMO memo;
        dump_object(*this, nmemo, memo, acc);
        return acc.str();
    }

    bool operator==(TT const& rhs) const
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
                assert(false);
        }
        return false;
    }

    bool operator!=(TT const& rhs) const
    {
        return !(*this == rhs);
    }

private:

    TTT tag;

    HASHTBL hash_table;
    std::string tostr;
    std::string s;
    // int64 i; // lua 5.3 support?
    double d;
    bool b;

    void makestr()
    {
        std::ostringstream oss;
        oss << tag;
        switch (tag)
        {
            case TBOOL:
                oss << b;
                break;
            case TNIL:
                break;
            case TSTRING:
                oss << s;
                break;
            case TNUMBER:
                oss << d;
                break;
            case TTABLE:
                oss << hash_table;
                break;
            default:
                assert(false);
        }
        tostr = oss.str();
    }

    size_t dump_type_table(TT const & object, size_t nmemo, MEMO& memo, ACC& acc) const
    {
        assert(object.istable());

        auto it = memo.find(object.hash_table);
        if (it != memo.end())
        {
            acc << '@' << it->second;
            return nmemo;
        }
        nmemo++;
        memo[object.hash_table] = nmemo;
        acc << '{';
        if (object.hash_table)
        {
            for (auto&& v : *object.hash_table)
            {
                nmemo = dump_object(v.first, nmemo, memo, acc);
                acc << ':';
                nmemo = dump_object(v.second, nmemo, memo, acc);
                acc << ',';
            }
        }
        std::string l = acc.str();
        char c = l[l.length() - 1]; // unsafe?
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

    size_t dump_object(TT const & object, size_t nmemo, MEMO& memo, ACC& acc) const
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
                    std::ostringstream oss;
                    oss << object.d;
                    std::string nn = oss.str();
                    if (nn == "1#INF")
                        acc << 'I';
                    else if (nn == "-1#INF")
                        acc << 'i';
                    else if (nn == "1#IND")
                        acc << 'i';
                    else if (nn == "-1#IND")
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
                assert(false);
                break;
        }
        return nmemo;
    }
};

bool nonzero_digit(char c)
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

bool is_digit(char c)
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
    return '\0';
}

TT expect_number(std::string const & string, size_t& start)
{
    size_t i = start;
    // IF i > strlen -> ERR
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
        assert(false);
    if (head == '.')
    {
        size_t oldi = i;
        do
        {
            head = strat(string, ++i);
        } while (is_digit(head));
        if (i == oldi + 1)
            assert(false);
    }
    if (head == 'e' || head == 'E')
    {
        head = strat(string, ++i);
        if (head == '+' || head == '-')
            head = strat(string, ++i);
        if (!is_digit(head))
            assert(false);
        do
        {
            head = strat(string, ++i);
        } while (is_digit(head));
    }
    size_t temp = start;
    start = i;
    return std::atof(string.substr(temp, i).c_str());
}

typedef std::vector<TT::HASHTBL> TABLES;

TT expect_object(std::string const & string, size_t& i, TABLES& tables)
{
    static double _zero = 0.0;

    // IF i > strlen -> ERR
    char cc = strat(string, i++);
    switch (cc)
    {
        case 't':
            return true;
        case 'f':
            return false;
        case 'n':
            return TT::nil();
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
                    // ERROR
                    assert(false);
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
            TT nt(TTABLE);
            size_t j = 1;
            tables.push_back(nt.tbl());
            // IF i > strlen -> ERR
            if (strat(string, i) == '}')
            {
                ++i;
                return nt;
            }
            while (true)
            {
                TT k = expect_object(string, i, tables);
                // IF i > strlen -> ERR
                if (strat(string, i) == ':')
                {
                    TT v = expect_object(string, ++i, tables);
                    nt.set(k, v);
                }
                else
                {
                    nt.set(j, k);
                    ++j;
                }
                // IF i > strlen -> ERR
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
                    // ERROR at i
                    assert(false);
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
            // ERROR at i
            assert(false);
        }
        default:
        {
            // ERROR at i
            assert(false);
            break;
        }
    }
    return TT::nil();
}

TT loads(std::string const & string, size_t maxsize = 10000)
{
    if (string.length() > maxsize)
        return TT::nil();
    TABLES tables;
    size_t i = 0;
    return expect_object(string, i, tables);
}

#endif
