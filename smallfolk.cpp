#include "smallfolk.h"

LuaVal const & LuaVal::nil()
{
    static LuaVal const nil;
    return nil;
}

std::string LuaVal::tostring() const
{
    switch (tag)
    {
    case TBOOL:
        if (b)
            return "true";
        else
            return "false";
    case TNIL:
        return "nil";
    case TSTRING:
        return s;
    case TNUMBER:
        return tostring(d);
    case TTABLE:
        return tostring(tbl_ptr);
    }
    throw smallfolk_exception("tostring invalid or unhandled tag %i", tag);
    return std::string();
}

LuaVal::LuaVal(const LuaTypeTag tag) : tag(tag), tbl_ptr(tag == TTABLE ? new LuaTable() : nullptr), d(0), b(false)
{
    if (istable() && !tbl_ptr)
        throw smallfolk_exception("creating table LuaVal with nullptr table");
}

LuaVal::LuaVal() : tag(TNIL), tbl_ptr(nullptr), d(0), b(false)
{
}

LuaVal::LuaVal(const long d) : tag(TNUMBER), tbl_ptr(nullptr), d(d), b(false)
{
}

LuaVal::LuaVal(const unsigned long d) : tag(TNUMBER), tbl_ptr(nullptr), d(d), b(false)
{
}

LuaVal::LuaVal(const int d) : tag(TNUMBER), tbl_ptr(nullptr), d(d), b(false)
{
}

LuaVal::LuaVal(const unsigned int d) : tag(TNUMBER), tbl_ptr(nullptr), d(d), b(false)
{
}

LuaVal::LuaVal(const float d) : tag(TNUMBER), tbl_ptr(nullptr), d(d), b(false)
{
}

LuaVal::LuaVal(const double d) : tag(TNUMBER), tbl_ptr(nullptr), d(d), b(false)
{
}

LuaVal::LuaVal(const std::string & s) : tag(TSTRING), tbl_ptr(nullptr), s(s), d(0), b(false)
{
}

LuaVal::LuaVal(const char * s) : tag(TSTRING), tbl_ptr(nullptr), s(s), d(0), b(false)
{
}

LuaVal::LuaVal(const bool b) : tag(TBOOL), tbl_ptr(nullptr), d(0), b(b)
{
}

LuaVal::LuaVal(LuaTable const & luatable) : tag(TTABLE), tbl_ptr(new LuaTable(luatable)), d(0), b(false)
{
    if (!tbl_ptr)
        throw smallfolk_exception("creating table LuaVal with nullptr table");
}

LuaVal::LuaVal(LuaVal const & val) : tag(val.tag), tbl_ptr(val.tag == TTABLE && val.tbl_ptr ? new LuaTable(*val.tbl_ptr) : nullptr), s(val.s), d(val.d), b(val.b)
{
    if (istable())
    {
        if (!tbl_ptr)
            throw smallfolk_exception("creating table LuaVal with nullptr table");
    }
}

LuaVal::LuaVal(LuaVal && val) : tag(val.tag), tbl_ptr(std::move(val.tbl_ptr)), s(std::move(val.s)), d(val.d), b(val.b)
{
}

LuaVal::LuaVal(std::initializer_list<LuaVal const> const & l) : tag(TTABLE), tbl_ptr(new LuaTable()), d(0), b(false)
{
    if (!tbl_ptr)
        throw smallfolk_exception("creating table LuaVal with nullptr table");
    LuaTable & tbl = *tbl_ptr;
    unsigned int i = 0;
    for (auto&& v : l)
    {
        if (v.isnil())
            ++i;
        else
            tbl[++i] = v;
    }
}

bool LuaVal::isstring() const { return tag == TSTRING; }

bool LuaVal::isnumber() const { return tag == TNUMBER; }

bool LuaVal::istable() const { return tag == TTABLE; }

bool LuaVal::isbool() const { return tag == TBOOL; }

bool LuaVal::isnil() const { return tag == TNIL; }

LuaVal LuaVal::table(LuaTable arr)
{
    return LuaVal(arr);
}

LuaVal LuaVal::get(LuaVal const & k) const
{
    if (!istable())
        throw smallfolk_exception("using get on non table object");
    if (k.isnil()) // on nil key do nothing
        return nil();
    LuaTable & tbl = (*tbl_ptr);
    auto it = tbl.find(k);
    if (it != tbl.end())
        return it->second;
    return nil();
}

LuaVal & LuaVal::set(LuaVal const & k, LuaVal const & v)
{
    if (!istable())
        throw smallfolk_exception("using set on non table object");
    if (k.isnil()) // on nil key do nothing
        return *this;
    LuaTable & tbl = (*tbl_ptr);
    if (v.isnil()) // on nil value erase key
        tbl.erase(k);
    else
        tbl[k] = v; // normally set pair
    return *this;
}

LuaVal & LuaVal::rem(LuaVal const & k)
{
    if (!istable())
        throw smallfolk_exception("using rem on non table object");
    LuaTable & tbl = (*tbl_ptr);
    tbl.erase(k);
    return *this;
}

unsigned int LuaVal::len() const
{
    if (!istable())
        throw smallfolk_exception("using len on non table object");
    LuaTable & tbl = (*tbl_ptr);
    unsigned int i = 0;
    while (++i)
    {
        auto it = tbl.find(i);
        if (it == tbl.end() || it->second.isnil())
            break;
    }
    return i - 1;
}

LuaVal & LuaVal::insert(LuaVal const & v, LuaVal const & pos)
{
    if (!istable())
        throw smallfolk_exception("using insert on non table object");
    LuaTable & tbl = (*tbl_ptr);
    if (pos.isnil())
    {
        if (!v.isnil())
            tbl[len() + 1] = v;
        return *this;
    }
    if (!pos.isnumber())
        throw smallfolk_exception("using insert with non number pos");
    if (std::floor(pos.num()) != pos.num())
        throw smallfolk_exception("using insert with invalid number key");
    unsigned int max = len() + 1;
    unsigned int val = static_cast<unsigned int>(pos.num());
    if (val <= 0 || val > max)
        throw smallfolk_exception("using insert with out of bounds key");
    for (unsigned int i = max; i > val; --i)
        tbl[i] = tbl[i - 1];
    if (v.isnil())
        tbl.erase(val);
    else
        tbl[val] = v;
    return *this;
}

LuaVal & LuaVal::remove(LuaVal const & pos)
{
    if (!istable())
        throw smallfolk_exception("using remove on non table object");
    LuaTable & tbl = (*tbl_ptr);
    if (pos.isnil())
    {
        if (unsigned int i = len())
            tbl.erase(i);
        return *this;
    }
    if (!pos.isnumber())
        throw smallfolk_exception("using remove with non number key");
    if (std::floor(pos.num()) != pos.num())
        throw smallfolk_exception("using remove with invalid number key");
    unsigned int max = len();
    unsigned int val = static_cast<unsigned int>(pos.num());
    if (val <= 0 || val > max + 1)
        throw smallfolk_exception("using remove with out of bounds key");
    for (unsigned int i = val; i < max; ++i)
        tbl[i] = tbl[i + 1];
    tbl.erase(max);
    return *this;
}

double LuaVal::num() const
{
    if (!isnumber())
        throw smallfolk_exception("using num on non number object");
    return d;
}

bool LuaVal::boolean() const
{
    if (!isbool())
        throw smallfolk_exception("using boolean on non bool object");
    return b;
}

std::string const & LuaVal::str() const
{
    if (!isstring())
        throw smallfolk_exception("using str on non string object");
    return s;
}

LuaVal::LuaTable const & LuaVal::tbl() const
{
    if (!istable())
        throw smallfolk_exception("using tbl on non table object");
    return *tbl_ptr;
}

LuaTypeTag LuaVal::GetTypeTag() const
{
    return tag;
}

std::string LuaVal::dumps(std::string * errmsg) const
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
            *errmsg += e.what();
    }
    return std::string();
}

LuaVal LuaVal::loads(std::string const & string, std::string * errmsg)
{
    try
    {
        TABLES tables;
        size_t i = 0;
        return std::move(expect_object(string, i, tables));
    }
    catch (std::exception& e)
    {
        if (errmsg)
            *errmsg += e.what();
    }
    return nil();
}

std::string LuaVal::tostring(const double d)
{
    char arr[128];
    sprintf(arr, "%.17g", d);
    return arr;
}

std::string LuaVal::tostring(TblPtr const & ptr)
{
    char arr[128];
    sprintf(arr, "table: %p", static_cast<const void*>(ptr.get()));
    return arr;
}

size_t LuaVal::dump_type_table(LuaVal const & object, unsigned int nmemo, MEMO & memo, ACC & acc)
{
    if (!object.istable())
        throw smallfolk_exception("using dump_type_table on non table object");

    /*
    auto it = memo.find(object);
    if (it != memo.end())
    {
    acc << '@' << it->second;
    return nmemo;
    }
    memo[object] = ++nmemo;
    */
    acc << '{';
    std::map<unsigned int, const LuaVal*> arr;
    std::unordered_map<const LuaVal*, const LuaVal*> hash;
    for (auto&& v : *object.tbl_ptr)
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
    return std::move(nmemo);
}

size_t LuaVal::dump_object(LuaVal const & object, unsigned int nmemo, MEMO & memo, ACC & acc)
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
        if (!std::isfinite(object.d))
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
    return std::move(nmemo);
}

std::string LuaVal::escape_quotes(const std::string & before)
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

    return std::move(after);
}

std::string LuaVal::unescape_quotes(const std::string & before)
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

    return std::move(after);
}

bool LuaVal::nonzero_digit(char c)
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

bool LuaVal::is_digit(char c)
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

char LuaVal::strat(std::string const & string, std::string::size_type i)
{
    if (i != std::string::npos &&
        i < string.length())
        return string.at(i);
    return '\0'; // bad?
}

LuaVal LuaVal::expect_number(std::string const & string, size_t & start)
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

LuaVal LuaVal::expect_object(std::string const & string, size_t & i, TABLES & tables)
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
            return std::move(nt);
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
                return std::move(nt);
            }
            else
            {
                throw smallfolk_exception("expect_object at %u was %c unexpected character %c", i, cc, head);
            }
        }
        break;
    }
    /*
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
    */
    default:
    {
        throw smallfolk_exception("expect_object at %u was %c", i, cc);
        break;
    }
    }
    return nil();
}

smallfolk_exception::smallfolk_exception(const char * format, ...) : std::logic_error("Smallfolk exception")
{
    char buffer[size];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, size, format, args);
    errmsg = std::string("Smallfolk: ") + buffer;
    va_end(args);
}

const char * smallfolk_exception::what() const throw()
{
    return errmsg.c_str();
}
