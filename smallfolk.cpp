#include "smallfolk.h"
#include <map>
#include <climits>
#include <iomanip> // std::setprecision
#include <sstream> // std::stringstream
#include <cmath> // std::floor, std::isfinite
#include <cstdlib> // std::strtod
#include <cstdio> // std::snprintf
#include <stdarg.h> // va_start
#include <functional> // std::hash
#include <mutex>
#include <vector>

namespace
{
    LoadLimits g_load_limits;
    std::mutex g_load_limits_mutex;

    std::string path_segment(LuaVal const & key)
    {
        if (key.isstring())
            return std::string(".") + key.str();
        if (key.isnumber())
        {
            char buffer[32];
            std::snprintf(buffer, sizeof(buffer), "[%u]", static_cast<unsigned>(key.num()));
            return buffer;
        }
        return "[?]";
    }

    std::string format_path(std::initializer_list<LuaVal> const & keys, size_t segment_count)
    {
        std::string path = "$";
        size_t i = 0;
        for (LuaVal const & key : keys)
        {
            if (i >= segment_count)
                break;
            path += path_segment(key);
            ++i;
        }
        return path;
    }

    void ensure_path_key(LuaVal const & key, char const * api_name)
    {
        if (key.isnil())
            throw smallfolk_exception("using %s with nil key segment", api_name);
    }

    enum class PathWalkResult
    {
        Found,
        MissingKey,
        NotTable,
    };

    PathWalkResult walk_path_read(
        LuaVal const & root,
        std::initializer_list<LuaVal> const & keys,
        LuaVal const ** out,
        size_t * fail_index,
        PathWalkResult * fail_reason)
    {
        LuaVal const * cur = &root;
        size_t i = 0;
        for (LuaVal const & key : keys)
        {
            ensure_path_key(key, "path lookup");
            if (!cur->istable())
            {
                if (fail_index)
                    *fail_index = i;
                if (fail_reason)
                    *fail_reason = PathWalkResult::NotTable;
                return PathWalkResult::NotTable;
            }
            LuaVal const * next = cur->try_get(key);
            if (!next)
            {
                if (fail_index)
                    *fail_index = i;
                if (fail_reason)
                    *fail_reason = PathWalkResult::MissingKey;
                return PathWalkResult::MissingKey;
            }
            cur = next;
            ++i;
        }
        *out = cur;
        return PathWalkResult::Found;
    }

    LoadLimits read_load_limits()
    {
        std::lock_guard<std::mutex> lock(g_load_limits_mutex);
        return g_load_limits;
    }

    struct ParseContext
    {
        LoadLimits const & limits;
        size_t value_count;
        unsigned depth;

        void on_value_created()
        {
            ++value_count;
            if (value_count > limits.max_value_count)
                throw smallfolk_exception(
                    "load limit exceeded: max value count %zu",
                    limits.max_value_count);
        }

        struct DepthGuard
        {
            ParseContext & ctx;
            explicit DepthGuard(ParseContext & c) : ctx(c)
            {
                ++ctx.depth;
                if (ctx.depth > ctx.limits.max_nesting_depth)
                    throw smallfolk_exception(
                        "load limit exceeded: max nesting depth %u",
                        ctx.limits.max_nesting_depth);
            }
            ~DepthGuard() { --ctx.depth; }
        };
    };

    void skip_whitespace(std::string const & string, size_t & i)
    {
        while (i < string.length() && (string[i] == ' ' || string[i] == '\t'))
            ++i;
    }
}

namespace Serializer
{
    typedef std::vector<LuaVal> TABLES;
    typedef std::unordered_map<LuaVal, unsigned int, LuaVal::LuaValHasher> MEMO;
    typedef std::stringstream ACC;

    inline std::string tostring(const double d)
    {
        char arr[128];
        std::snprintf(arr, sizeof(arr), "%.17g", d);
        return arr;
    }
    inline std::string tostring(LuaVal::TblPtr const & ptr)
    {
        char arr[128];
        std::snprintf(arr, sizeof(arr), "table: %p", static_cast<void*>(ptr.get()));
        return arr;
    }

    unsigned int dump_type_table(LuaVal const & object, unsigned int nmemo, MEMO& memo, ACC& acc);
    unsigned int dump_object(LuaVal const & object, unsigned int nmemo, MEMO& memo, ACC& acc);
    std::string escape_quotes(const std::string &before, char quote);
    std::string unescape_quotes(const std::string &before, char quote);
    bool nonzero_digit(char c);
    bool is_digit(char c);
    char strat(std::string const & string, std::string::size_type i);
    LuaVal expect_number(std::string const & string, size_t& start, ParseContext & ctx);
    LuaVal expect_object(std::string const & string, size_t& i, TABLES& tables, ParseContext & ctx);
}

LoadLimits const & LuaVal::default_load_limits()
{
    static LoadLimits const defaults;
    return defaults;
}

LoadLimits LuaVal::untrusted_load_limits()
{
    LoadLimits limits;
    limits.max_input_size = 256 * 1024;
    limits.max_string_length = 64 * 1024;
    limits.max_nesting_depth = 64;
    limits.max_value_count = 10000;
    limits.max_table_entries = 10000;
    limits.require_consumed_input = true;
    limits.reject_non_finite_numbers = true;
    return limits;
}

LoadLimits LuaVal::get_load_limits()
{
    return read_load_limits();
}

void LuaVal::set_load_limits(LoadLimits limits)
{
    std::lock_guard<std::mutex> lock(g_load_limits_mutex);
    g_load_limits = limits;
}

LuaVal const LuaVal::nil(TNIL);

LuaVal::LuaVal(std::initializer_list<LuaVal> const & l)
    : tag(TTABLE), tbl_ptr(new LuaTable()), d(0), b(false)
{
    InitializeSequence(l);
}

void LuaVal::InitializeSequence(std::initializer_list<LuaVal> const & l)
{
    unsigned int i = 0;
    for (auto const & v : l)
    {
        LuaVal vv(v);
        if (vv.isnil())
            ++i;
        else
            set(static_cast<int>(++i), std::move(vv));
    }
}

LuaVal lua_val::map(std::initializer_list<std::pair<LuaVal, LuaVal>> const & entries)
{
    LuaVal value(TTABLE);
    for (auto const & entry : entries)
        value.set(entry.first, entry.second);
    return value;
}

LuaVal LuaVal::merge(LuaVal const & l, LuaVal const & r)
{
    LuaVal result = l;
    for (auto const & entry : r.tbl())
        result.set(entry.first, entry.second);
    return result;
}

LuaVal LuaVal::merge(LuaVal && l, LuaVal && r)
{
    return merge(std::move(l), static_cast<LuaVal const &>(r));
}

LuaVal LuaVal::merge(LuaVal && l, LuaVal const & r)
{
    for (auto const & entry : r.tbl())
        l.set(entry.first, entry.second);
    return std::move(l);
}

LuaVal LuaVal::merge(LuaVal const & l, LuaVal && r)
{
    for (auto const & entry : l.tbl())
        r.setignore(entry.first, entry.second);
    return std::move(r);
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
        return Serializer::tostring(d);
    case TTABLE:
        return Serializer::tostring(tbl_ptr);
    }
    throw smallfolk_exception("tostring invalid or unhandled tag %i", tag);
}

size_t LuaVal::LuaValHasher::operator()(LuaVal const & v) const
{
    return LuaValHash(v);
}

size_t LuaValHash(LuaVal const & v)
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
        return std::hash<LuaVal::TblPtr>()(v.tbl_ptr);
    }
    return std::hash<std::string>()(v.tostring());
}

LuaVal & LuaVal::operator[](LuaVal const & k)
{
    if (!istable())
        throw smallfolk_exception("using [] on non table object");
    if (k.isnil())
        throw smallfolk_exception("using [] with nil key");
    LuaTable & tbl = (*tbl_ptr);
    return tbl[k];
}

LuaVal const & LuaVal::operator[](LuaVal const & k) const
{
    return get(k);
}

LuaVal const & LuaVal::get(LuaVal const & k) const
{
    if (!istable())
        throw smallfolk_exception("using get on non table object");
    if (k.isnil())
        throw smallfolk_exception("using get with nil key");
    LuaTable & tbl = (*tbl_ptr);
    auto it = tbl.find(k);
    if (it != tbl.end())
        return it->second;
    return nil;
}

LuaVal const & LuaVal::get(std::string const & k) const
{
    return get(LuaVal(k));
}

LuaVal const & LuaVal::get(int k) const
{
    return get(LuaVal(k));
}

LuaVal const * LuaVal::try_get(LuaVal const & k) const
{
    if (!istable())
        throw smallfolk_exception("using try_get on non table object");
    if (k.isnil())
        throw smallfolk_exception("using try_get with nil key");
    LuaTable const & tbl = (*tbl_ptr);
    auto it = tbl.find(k);
    if (it == tbl.end())
        return nullptr;
    return &it->second;
}

LuaVal const * LuaVal::try_get(std::string const & k) const
{
    return try_get(LuaVal(k));
}

LuaVal const * LuaVal::try_get(int k) const
{
    return try_get(LuaVal(k));
}

LuaVal & LuaVal::at(LuaVal const & k)
{
    if (!istable())
        throw smallfolk_exception("using at on non table object");
    if (k.isnil())
        throw smallfolk_exception("using at with nil key");
    LuaTable & tbl = (*tbl_ptr);
    auto it = tbl.find(k);
    if (it == tbl.end())
        throw smallfolk_exception("at: key not found");
    return it->second;
}

LuaVal const & LuaVal::at(LuaVal const & k) const
{
    if (!istable())
        throw smallfolk_exception("using at on non table object");
    if (k.isnil())
        throw smallfolk_exception("using at with nil key");
    LuaTable const & tbl = (*tbl_ptr);
    auto it = tbl.find(k);
    if (it == tbl.end())
        throw smallfolk_exception("at: key not found");
    return it->second;
}

LuaVal & LuaVal::at(std::string const & k)
{
    return at(LuaVal(k));
}

LuaVal const & LuaVal::at(std::string const & k) const
{
    return at(LuaVal(k));
}

LuaVal & LuaVal::at(int k)
{
    return at(LuaVal(k));
}

LuaVal const & LuaVal::at(int k) const
{
    return at(LuaVal(k));
}

bool LuaVal::has(LuaVal const & k) const
{
    if (!istable())
        throw smallfolk_exception("using has on non table object");
    if (k.isnil())
        throw smallfolk_exception("using has with nil key");
    LuaTable & tbl = (*tbl_ptr);
    auto it = tbl.find(k);
    return it != tbl.end();
}

bool LuaVal::has(std::string const & k) const
{
    return has(LuaVal(k));
}

bool LuaVal::has(int k) const
{
    return has(LuaVal(k));
}

LuaVal const * LuaVal::try_get_path(std::initializer_list<LuaVal> keys) const
{
    if (keys.size() == 0)
        return this;
    if (!istable())
        throw smallfolk_exception("using try_get_path on non table object");
    LuaVal const * found = nullptr;
    PathWalkResult const result = walk_path_read(*this, keys, &found, nullptr, nullptr);
    if (result == PathWalkResult::Found)
        return found;
    return nullptr;
}

LuaVal const & LuaVal::get_path(std::initializer_list<LuaVal> keys) const
{
    if (keys.size() == 0)
        return *this;
    LuaVal const * found = try_get_path(keys);
    if (found)
        return *found;
    return nil;
}

LuaVal & LuaVal::at_path(std::initializer_list<LuaVal> keys)
{
    if (keys.size() == 0)
        return *this;
    if (!istable())
        throw smallfolk_exception("using at_path on non table object");
    LuaVal const * found = nullptr;
    size_t fail_index = 0;
    PathWalkResult fail_reason = PathWalkResult::MissingKey;
    PathWalkResult const result = walk_path_read(*this, keys, &found, &fail_index, &fail_reason);
    if (result == PathWalkResult::Found)
        return const_cast<LuaVal &>(*found);
    if (fail_reason == PathWalkResult::NotTable)
        throw smallfolk_exception("at_path: not a table at %s", format_path(keys, fail_index).c_str());
    throw smallfolk_exception("at_path: key not found at %s", format_path(keys, fail_index + 1).c_str());
}

LuaVal const & LuaVal::at_path(std::initializer_list<LuaVal> keys) const
{
    if (keys.size() == 0)
        return *this;
    if (!istable())
        throw smallfolk_exception("using at_path on non table object");
    LuaVal const * found = nullptr;
    size_t fail_index = 0;
    PathWalkResult fail_reason = PathWalkResult::MissingKey;
    PathWalkResult const result = walk_path_read(*this, keys, &found, &fail_index, &fail_reason);
    if (result == PathWalkResult::Found)
        return *found;
    if (fail_reason == PathWalkResult::NotTable)
        throw smallfolk_exception("at_path: not a table at %s", format_path(keys, fail_index).c_str());
    throw smallfolk_exception("at_path: key not found at %s", format_path(keys, fail_index + 1).c_str());
}

bool LuaVal::has_path(std::initializer_list<LuaVal> keys) const
{
    if (keys.size() == 0)
        return true;
    if (!istable())
        throw smallfolk_exception("using has_path on non table object");
    LuaVal const * found = nullptr;
    size_t fail_index = 0;
    PathWalkResult fail_reason = PathWalkResult::MissingKey;
    PathWalkResult const result = walk_path_read(*this, keys, &found, &fail_index, &fail_reason);
    (void)fail_index;
    (void)fail_reason;
    return result == PathWalkResult::Found;
}

LuaVal & LuaVal::set_path(std::initializer_list<LuaVal> keys, LuaVal const & v)
{
    if (keys.size() == 0)
        throw smallfolk_exception("using set_path with empty path");
    if (!istable())
        throw smallfolk_exception("using set_path on non table object");

    LuaVal * cur = this;
    auto it = keys.begin();
    auto const end = keys.end();
    LuaVal const * last = end - 1;
    size_t index = 0;

    for (; it != last; ++it, ++index)
    {
        ensure_path_key(*it, "set_path");
        if (!cur->istable())
            throw smallfolk_exception("set_path: not a table at %s", format_path(keys, index).c_str());
        cur = &(*cur)[*it];
    }

    ensure_path_key(*last, "set_path");
    return cur->set(*last, v);
}

LuaVal & LuaVal::set_path(std::initializer_list<LuaVal> keys, LuaVal && v)
{
    if (keys.size() == 0)
        throw smallfolk_exception("using set_path with empty path");
    if (!istable())
        throw smallfolk_exception("using set_path on non table object");

    LuaVal * cur = this;
    auto it = keys.begin();
    auto const end = keys.end();
    LuaVal const * last = end - 1;
    size_t index = 0;

    for (; it != last; ++it, ++index)
    {
        ensure_path_key(*it, "set_path");
        if (!cur->istable())
            throw smallfolk_exception("set_path: not a table at %s", format_path(keys, index).c_str());
        cur = &(*cur)[*it];
    }

    ensure_path_key(*last, "set_path");
    return cur->set(*last, std::move(v));
}

LuaVal & LuaVal::erase_path(std::initializer_list<LuaVal> keys)
{
    if (keys.size() == 0)
        throw smallfolk_exception("using erase_path with empty path");
    if (!istable())
        throw smallfolk_exception("using erase_path on non table object");

    if (keys.size() == 1)
        return erase(*keys.begin());

    LuaVal * cur = this;
    auto it = keys.begin();
    auto const end = keys.end();
    LuaVal const * last = end - 1;

    for (; it != last; ++it)
    {
        ensure_path_key(*it, "erase_path");
        if (!cur->istable())
            return *this;
        LuaVal const * next = cur->try_get(*it);
        if (!next)
            return *this;
        cur = const_cast<LuaVal *>(next);
    }

    if (!cur->istable())
        return *this;
    return cur->erase(*last);
}

LuaVal & LuaVal::set(LuaVal const & k, LuaVal const & v)
{
    if (!istable())
        throw smallfolk_exception("using set on non table object");
    if (k.isnil())
        throw smallfolk_exception("using set with nil key");
    LuaTable & tbl = (*tbl_ptr);
    if (v.isnil())
        tbl.erase(k);
    else
        tbl[k] = v;
    return *this;
}

LuaVal & LuaVal::set(LuaVal const & k, LuaVal && v)
{
    if (!istable())
        throw smallfolk_exception("using set on non table object");
    if (k.isnil())
        throw smallfolk_exception("using set with nil key");
    LuaTable & tbl = (*tbl_ptr);
    if (v.isnil())
        tbl.erase(k);
    else
        tbl[k] = std::move(v);
    return *this;
}

LuaVal & LuaVal::set(std::string const & k, LuaVal const & v)
{
    return set(LuaVal(k), v);
}

LuaVal & LuaVal::set(std::string const & k, LuaVal && v)
{
    return set(LuaVal(k), std::move(v));
}

LuaVal & LuaVal::set(int k, LuaVal const & v)
{
    return set(LuaVal(k), v);
}

LuaVal & LuaVal::set(int k, LuaVal && v)
{
    return set(LuaVal(k), std::move(v));
}

LuaVal & LuaVal::setignore(LuaVal const & k, LuaVal const & v)
{
    if (!istable())
        throw smallfolk_exception("using setignore on non table object");
    if (k.isnil())
        throw smallfolk_exception("using setignore with nil key");
    if (v.isnil())
        return *this;
    LuaTable & tbl = (*tbl_ptr);
    tbl.emplace(k, v);
    return *this;
}

LuaVal & LuaVal::setignore(LuaVal const & k, LuaVal && v)
{
    if (!istable())
        throw smallfolk_exception("using setignore on non table object");
    if (k.isnil())
        throw smallfolk_exception("using setignore with nil key");
    if (v.isnil())
        return *this;
    LuaTable & tbl = (*tbl_ptr);
    tbl.emplace(k, std::move(v));
    return *this;
}

LuaVal & LuaVal::erase(LuaVal const & k)
{
    if (!istable())
        throw smallfolk_exception("using erase on non table object");
    if (k.isnil())
        throw smallfolk_exception("using erase with nil key");
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
    for (;;)
    {
        if (i == UINT_MAX)
            return i;
        ++i;
        auto it = tbl.find(i);
        if (it == tbl.end() || it->second.isnil())
            return i - 1;
    }
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

LuaVal & LuaVal::insert(LuaVal && v, LuaVal const & pos)
{
    if (!istable())
        throw smallfolk_exception("using insert on non table object");
    LuaTable & tbl = (*tbl_ptr);
    if (pos.isnil())
    {
        if (!v.isnil())
            tbl[len() + 1] = std::move(v);
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
        tbl[i] = std::move(tbl[i - 1]);
    if (v.isnil())
        tbl.erase(val);
    else
        tbl[val] = std::move(v);
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
    if (!istable() || !tbl_ptr)
        throw smallfolk_exception("using tbl on non table object");
    return *tbl_ptr;
}

bool LuaVal::try_as_number(double & out) const
{
    if (!isnumber())
        return false;
    out = d;
    return true;
}

bool LuaVal::try_as_string(std::string const *& out) const
{
    if (!isstring())
        return false;
    out = &s;
    return true;
}

bool LuaVal::try_as_bool(bool & out) const
{
    if (!isbool())
        return false;
    out = b;
    return true;
}

std::string LuaVal::type(LuaTypeTag tag)
{
    switch (tag)
    {
        case TBOOL:
            return "boolean";
        case TNIL:
            return "nil";
        case TSTRING:
            return "string";
        case TNUMBER:
            return "number";
        case TTABLE:
            return "table";
    }
    throw smallfolk_exception("tostring invalid or unhandled tag %i", tag);
}

std::string LuaVal::dumps(std::string * errmsg) const
{
    try
    {
        Serializer::ACC acc;
        acc << std::setprecision(17); // min lua precision
        unsigned int nmemo = 0;
        Serializer::MEMO memo;
        Serializer::dump_object(*this, nmemo, memo, acc);
        return acc.str();
    }
    catch (smallfolk_exception const & e)
    {
        if (errmsg)
            *errmsg = e.what();
    }
    return std::string();
}

std::string LuaVal::dumps_or_throw() const
{
    std::string err;
    std::string value = dumps(&err);
    if (!err.empty())
        throw smallfolk_exception("%s", err.c_str());
    return value;
}

LuaVal LuaVal::loads(std::string const & string, std::string * errmsg)
{
    return loads(string, read_load_limits(), errmsg);
}

LuaVal LuaVal::loads(std::string const & string, LoadLimits const & limits, std::string * errmsg)
{
    try
    {
        if (string.length() > limits.max_input_size)
            throw smallfolk_exception(
                "load limit exceeded: max input size %zu",
                limits.max_input_size);

        Serializer::TABLES tables;
        ParseContext ctx{ limits, 0, 0 };
        size_t i = 0;
        LuaVal result = Serializer::expect_object(string, i, tables, ctx);
        skip_whitespace(string, i);
        if (limits.require_consumed_input && i != string.length())
            throw smallfolk_exception("unexpected trailing input at position %zu", i);
        return result;
    }
    catch (smallfolk_exception const & e)
    {
        if (errmsg)
            *errmsg = e.what();
    }
    return LuaVal::nil;
}

LuaVal LuaVal::loads_or_throw(std::string const & string)
{
    return loads_or_throw(string, read_load_limits());
}

LuaVal LuaVal::loads_or_throw(std::string const & string, LoadLimits const & limits)
{
    std::string err;
    LuaVal value = loads(string, limits, &err);
    if (!err.empty())
        throw smallfolk_exception("%s", err.c_str());
    return value;
}

bool LuaVal::operator==(LuaVal const& rhs) const
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
    }
    throw smallfolk_exception("operator== invalid or unhandled tag %i", tag);
}

LuaVal::operator bool() const
{
    return !isnil() && (!isbool() || boolean());
}

LuaVal& LuaVal::operator=(LuaVal const& val)
{
    tag = val.tag;
    if (istable())
        tbl_ptr.reset(new LuaTable(*val.tbl_ptr));
    else
        tbl_ptr = nullptr;
    s = val.s;
    d = val.d;
    b = val.b;
    return *this;
}

unsigned int Serializer::dump_type_table(LuaVal const & object, unsigned int nmemo, MEMO & memo, ACC & acc)
{
    if (!object.istable())
        throw smallfolk_exception("using dump_type_table on non table object");

    acc << '{';
    bool first = true;
    std::map<unsigned int, const LuaVal*> arr;
    std::unordered_map<const LuaVal*, const LuaVal*> hash;
    for (auto&& v : object.tbl())
    {
        if (v.first.isnumber() && v.first.num() >= 1 && std::floor(v.first.num()) == v.first.num())
            arr[static_cast<unsigned int>(v.first.num())] = &v.second;
        else
            hash[&v.first] = &v.second;
    }
    unsigned int i = 1;
    for (auto&& v : arr)
    {
        if (!first)
            acc << ',';
        first = false;
        if (v.first != i)
        {
            nmemo = dump_object(v.first, nmemo, memo, acc);
            acc << ':';
        }
        else
            ++i;
        nmemo = dump_object(*v.second, nmemo, memo, acc);
    }
    for (auto&& v : hash)
    {
        if (!first)
            acc << ',';
        first = false;
        nmemo = dump_object(*v.first, nmemo, memo, acc);
        acc << ':';
        nmemo = dump_object(*v.second, nmemo, memo, acc);
    }
    acc << '}';
    return nmemo;
}

unsigned int Serializer::dump_object(LuaVal const & object, unsigned int nmemo, MEMO & memo, ACC & acc)
{
    switch (object.typetag())
    {
    case TBOOL:
        acc << (object.boolean() ? 't' : 'f');
        break;
    case TNIL:
        acc << 'n';
        break;
    case TSTRING:
        acc << '"';
        acc << escape_quotes(object.str(), '"');
        acc << '"';
        break;
    case TNUMBER:
        if (std::isnan(object.num()))
            acc << (std::signbit(object.num()) ? 'Q' : 'N');
        else if (std::isinf(object.num()))
            acc << (object.num() < 0 ? 'i' : 'I');
        else
            acc << object.num();
        break;
    case TTABLE:
        return dump_type_table(object, nmemo, memo, acc);
    default:
        throw smallfolk_exception("dump_object invalid or unhandled tag %i", object.typetag());
    }
    return nmemo;
}

std::string Serializer::escape_quotes(const std::string & before, char quote)
{
    std::string after;
    after.reserve(before.length() + 4);

    for (std::string::size_type i = 0; i < before.length(); ++i)
    {
        if (before[i] == quote)
        {
            after += quote;
            after += quote;
        }
        else
            after += before[i];
    }

    return after;
}

std::string Serializer::unescape_quotes(const std::string & before, char quote)
{
    std::string after;
    after.reserve(before.length());

    for (std::string::size_type i = 0; i < before.length(); ++i)
    {
        if (before[i] == quote)
        {
            if (i + 1 < before.length() && before[i + 1] == quote)
            {
                after += quote;
                ++i;
            }
            else
                after += before[i];
        }
        else
            after += before[i];
    }

    return after;
}

bool Serializer::nonzero_digit(char c)
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

bool Serializer::is_digit(char c)
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

char Serializer::strat(std::string const & string, std::string::size_type i)
{
    if (i != std::string::npos &&
        i < string.length())
        return string.at(i);
    return '\0';
}

LuaVal Serializer::expect_number(std::string const & string, size_t & start, ParseContext & ctx)
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
        throw smallfolk_exception("expect_number at %zu unexpected character %c", i, head);
    if (head == '.')
    {
        size_t oldi = i;
        do
        {
            head = strat(string, ++i);
        } while (is_digit(head));
        if (i == oldi + 1)
            throw smallfolk_exception("expect_number at %zu no numbers after decimal", i);
    }
    if (head == 'e' || head == 'E')
    {
        head = strat(string, ++i);
        if (head == '+' || head == '-')
            head = strat(string, ++i);
        if (!is_digit(head))
            throw smallfolk_exception("expect_number at %zu not a digit part %c", i, head);
        do
        {
            head = strat(string, ++i);
        } while (is_digit(head));
    }
    size_t temp = start;
    start = i;
    char * end = nullptr;
    double value = std::strtod(string.c_str() + temp, &end);
    if (end != string.c_str() + i)
        throw smallfolk_exception("expect_number at %zu failed to parse number", temp);
    ctx.on_value_created();
    return value;
}

LuaVal Serializer::expect_object(std::string const & string, size_t & i, Serializer::TABLES & tables, ParseContext & ctx)
{
    static double _zero = 0.0;

    char cc = strat(string, i++);
    switch (cc)
    {
    case ' ':
    case '\t':
        return expect_object(string, i, tables, ctx);
    case 't':
        ctx.on_value_created();
        return true;
    case 'f':
        ctx.on_value_created();
        return false;
    case 'n':
        ctx.on_value_created();
        return LuaVal::nil;
    case 'Q':
    case 'N':
    case 'I':
    case 'i':
        if (ctx.limits.reject_non_finite_numbers)
            throw smallfolk_exception("non-finite number encoding rejected at %zu", i - 1);
        ctx.on_value_created();
        if (cc == 'Q')
            return -(0 / _zero);
        if (cc == 'N')
            return (0 / _zero);
        if (cc == 'I')
            return (1 / _zero);
        return -(1 / _zero);
    case '\'':
    case '"':
    {
        size_t nexti = i - 1;
        do
        {
            nexti = string.find(cc, nexti + 1);
            if (nexti == std::string::npos)
            {
                throw smallfolk_exception("expect_object at %zu was %c eof before string ends", i, cc);
            }
            ++nexti;
        } while (strat(string, nexti) == cc);
        size_t temp = i;
        size_t content_length = nexti - temp - 1;
        if (content_length > ctx.limits.max_string_length)
        {
            throw smallfolk_exception(
                "load limit exceeded: max string length %zu",
                ctx.limits.max_string_length);
        }
        i = nexti;
        ctx.on_value_created();
        return unescape_quotes(string.substr(temp, content_length), cc);
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
        return expect_number(string, --i, ctx);
    case '{':
    {
        ParseContext::DepthGuard depth_guard(ctx);
        (void)depth_guard;
        LuaVal nt(TTABLE);
        ctx.on_value_created();
        unsigned int j = 1;
        tables.push_back(nt);
        if (strat(string, i) == '}')
        {
            ++i;
            return nt;
        }
        unsigned int entry_count = 0;
        while (true)
        {
            if (ctx.limits.max_table_entries != 0 && ++entry_count > ctx.limits.max_table_entries)
            {
                throw smallfolk_exception(
                    "load limit exceeded: max table entries %zu",
                    ctx.limits.max_table_entries);
            }

            LuaVal k = expect_object(string, i, tables, ctx);
            char at = strat(string, i);
            while (at == ' ')
                at = strat(string, ++i);
            if (at == ':')
            {
                nt.set(k, expect_object(string, ++i, tables, ctx));
            }
            else
            {
                nt.set(static_cast<int>(j), k);
                ++j;
            }
            char head = strat(string, i);
            while (head == ' ')
                head = strat(string, ++i);
            if (head == ',')
                ++i;
            else if (head == '}')
            {
                ++i;
                return nt;
            }
            else
            {
                throw smallfolk_exception("expect_object at %zu was %c unexpected character %c", i, cc, head);
            }
        }
    }
    default:
        throw smallfolk_exception("expect_object at %zu was %c", i, cc);
    }
}

smallfolk_exception::smallfolk_exception(const char * format, ...) : std::logic_error("Smallfolk exception")
{
    char buffer[buffer_size];
    va_list args;
    va_start(args, format);
    std::vsnprintf(buffer, buffer_size, format, args);
    va_end(args);
    errmsg = std::string("Smallfolk: ") + buffer;
}

const char * smallfolk_exception::what() const noexcept
{
    return errmsg.c_str();
}
