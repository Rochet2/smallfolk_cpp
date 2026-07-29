#ifndef SMALLFOLK_H
#define SMALLFOLK_H

#include <string>
#include <unordered_map>
#include <memory>
#include <stdexcept>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <initializer_list>
#include <forward_list>
#include <deque>
#include <map>

class smallfolk_exception : public std::logic_error
{
public:
    static size_t const buffer_size = 2048;

    smallfolk_exception(const char * format, ...);
    const char* what() const noexcept override;

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

struct LoadLimits
{
    size_t max_input_size = 16 * 1024 * 1024;
    size_t max_string_length = 1024 * 1024;
    unsigned max_nesting_depth = 256;
    size_t max_value_count = 100000;
    // Maximum key/value pairs in a single table. 0 disables this check.
    size_t max_table_entries = 100000;
    bool require_consumed_input = true;
    // When true, reject I/i/N/Q non-finite number encodings during loads().
    bool reject_non_finite_numbers = false;
};

class LuaVal;
size_t LuaValHash(LuaVal const & v);

namespace std {
    template <>
    struct hash<LuaVal> {
    public:
        size_t operator()(LuaVal const & v) const
        {
            return LuaValHash(v);
        };
    };
}

class LuaVal
{
public:

    static LuaVal const nil;

    // Immutable library defaults (never changes at runtime).
    static LoadLimits const & default_load_limits();
    // Tighter defaults suitable for untrusted user input in servers.
    static LoadLimits untrusted_load_limits();
    // Thread-safe read of the process-wide default used by loads() without an explicit limits argument.
    static LoadLimits get_load_limits();
    // Thread-safe write of the process-wide default. Prefer passing LoadLimits per call in multi-threaded code.
    static void set_load_limits(LoadLimits limits);

    // Static nil value, same as LuaVal(TNIL). Useful as a default const reference.
    // Returns the string representation of the value, similar to lua tostring.
    std::string tostring() const;

    // Use as the hasher for containers, for example std::unordered_map<LuaVal, int, LuaVal::LuaValHasher>.
    struct LuaValHasher
    {
        size_t operator()(LuaVal const & v) const;
    };

    typedef std::unordered_map<LuaVal, LuaVal> LuaTable;
    typedef std::unique_ptr<LuaTable> TblPtr; // Table assign deep-copies; @ circular refs unsupported.

    LuaVal(const LuaTypeTag tag) : tag(tag), tbl_ptr(tag == TTABLE ? new LuaTable() : nullptr), d(0), b(false) {}
    LuaVal() : tag(TTABLE), tbl_ptr(new LuaTable()), d(0), b(false) {}
    LuaVal(const int d) : tag(TNUMBER), tbl_ptr(nullptr), d(d), b(false) {}
    LuaVal(const unsigned int d) : tag(TNUMBER), tbl_ptr(nullptr), d(d), b(false) {}
    LuaVal(const int64_t d) : tag(TNUMBER), tbl_ptr(nullptr), d(static_cast<double>(d)), b(false) {}
    LuaVal(const float d) : tag(TNUMBER), tbl_ptr(nullptr), d(d), b(false) {}
    LuaVal(const double d) : tag(TNUMBER), tbl_ptr(nullptr), d(d), b(false) {}
    LuaVal(const std::string & s) : tag(TSTRING), tbl_ptr(nullptr), s(s), d(0), b(false) {}
    LuaVal(const char * s) : tag(TSTRING), tbl_ptr(nullptr), s(s), d(0), b(false) {}
    LuaVal(const bool b) : tag(TBOOL), tbl_ptr(nullptr), d(0), b(b) {}
    LuaVal(LuaVal const & val) : tag(val.tag), tbl_ptr(val.tag == TTABLE ? val.tbl_ptr ? new LuaTable(*val.tbl_ptr) : new LuaTable() : nullptr), s(val.s), d(val.d), b(val.b) {}
    LuaVal(LuaVal && val) noexcept : tag(std::move(val.tag)), tbl_ptr(std::move(val.tbl_ptr)), s(std::move(val.s)), d(std::move(val.d)), b(std::move(val.b))
    {
        if (val.tag == TTABLE)
            val.tbl_ptr.reset(new LuaTable());
    }
    LuaVal(std::initializer_list<LuaVal> const & l);

    template<typename T>
    LuaVal(std::initializer_list<T> const & l) : tag(TTABLE), tbl_ptr(new LuaTable()), d(0), b(false)
    {
        InitializeSequence(l);
    }

    LuaVal(LuaTable const & l) : tag(TTABLE), tbl_ptr(new LuaTable(l)), d(0), b(false) {}

    template<typename T>
    LuaVal(std::forward_list<T> const & l) : tag(TTABLE), tbl_ptr(new LuaTable()), d(0), b(false)
    {
        InitializeSequence(l);
    }

    template<typename T>
    LuaVal(std::deque<T> const & l) : tag(TTABLE), tbl_ptr(new LuaTable()), d(0), b(false)
    {
        InitializeSequence(l);
    }

    template<typename K, typename V>
    LuaVal(std::unordered_map<K, V> const & l) : tag(TTABLE), tbl_ptr(new LuaTable()), d(0), b(false)
    {
        InitializeMap(l);
    }

    static LuaVal table() { return LuaVal(TTABLE); }

    static LuaVal merge(LuaVal const & l, LuaVal const & r);
    static LuaVal merge(LuaVal && l, LuaVal && r);
    static LuaVal merge(LuaVal && l, LuaVal const & r);
    static LuaVal merge(LuaVal const & l, LuaVal && r);

    static LuaVal mrg(LuaVal const & l, LuaVal const & r) { return merge(l, r); }
    static LuaVal mrg(LuaVal && l, LuaVal && r) { return merge(std::move(l), std::move(r)); }
    static LuaVal mrg(LuaVal && l, LuaVal const & r) { return merge(std::move(l), r); }
    static LuaVal mrg(LuaVal const & l, LuaVal && r) { return merge(l, std::move(r)); }

    ~LuaVal() = default;

    bool isstring() const { return tag == TSTRING; }
    bool isnumber() const { return tag == TNUMBER; }
    bool istable() const { return tag == TTABLE; }
    bool isbool() const { return tag == TBOOL; }
    bool isnil() const { return tag == TNIL; }

    // gettable; adds key-nil pair if not existing. nil key throws error.
    // Inserts an empty table when the key is missing.
    LuaVal & operator[](LuaVal const & k);
    LuaVal const & operator[](LuaVal const & k) const;

    // gettable; returns LuaVal::nil when the key is missing (same reference as static nil).
    LuaVal const & get(LuaVal const & k) const;
    LuaVal const & get(std::string const & k) const;
    LuaVal const & get(int k) const;
    LuaVal const & get(char const * k) const { return get(std::string(k)); }
    LuaVal const & get(double k) const { return get(LuaVal(k)); }

    // nullptr when the key is missing; otherwise points at the stored value (including nil).
    LuaVal const * try_get(LuaVal const & k) const;
    LuaVal const * try_get(std::string const & k) const;
    LuaVal const * try_get(int k) const;
    LuaVal const * find(LuaVal const & k) const { return try_get(k); }

    // Throws when the key is missing; does not insert.
    LuaVal & at(LuaVal const & k);
    LuaVal const & at(LuaVal const & k) const;
    LuaVal & at(std::string const & k);
    LuaVal const & at(std::string const & k) const;
    LuaVal & at(int k);
    LuaVal const & at(int k) const;

    // returns true if value was found with key
    bool has(LuaVal const & k) const;
    bool has(std::string const & k) const;
    bool has(int k) const;

    // Read-only nested lookup; never auto-vivifies. Empty path refers to this value.
    LuaVal const * try_get_path(std::initializer_list<LuaVal> keys) const;
    LuaVal const & get_path(std::initializer_list<LuaVal> keys) const;
    LuaVal & at_path(std::initializer_list<LuaVal> keys);
    LuaVal const & at_path(std::initializer_list<LuaVal> keys) const;
    bool has_path(std::initializer_list<LuaVal> keys) const;

    template<typename... Keys>
    LuaVal const * try_get_path(Keys const &... keys) const
    {
        return try_get_path(std::initializer_list<LuaVal>{ LuaVal(keys)... });
    }

    template<typename... Keys>
    LuaVal const & get_path(Keys const &... keys) const
    {
        return get_path(std::initializer_list<LuaVal>{ LuaVal(keys)... });
    }

    template<typename... Keys>
    LuaVal & at_path(Keys const &... keys)
    {
        return at_path(std::initializer_list<LuaVal>{ LuaVal(keys)... });
    }

    template<typename... Keys>
    LuaVal const & at_path(Keys const &... keys) const
    {
        return at_path(std::initializer_list<LuaVal>{ LuaVal(keys)... });
    }

    template<typename... Keys>
    bool has_path(Keys const &... keys) const
    {
        return has_path(std::initializer_list<LuaVal>{ LuaVal(keys)... });
    }

    // Nested set/erase. set_path auto-vivifies missing intermediate tables; erase_path is a no-op when the path is missing.
    LuaVal & set_path(std::initializer_list<LuaVal> keys, LuaVal const & v);
    LuaVal & set_path(std::initializer_list<LuaVal> keys, LuaVal && v);
    LuaVal & erase_path(std::initializer_list<LuaVal> keys);

    template<typename... Keys>
    LuaVal & erase_path(Keys const &... keys)
    {
        return erase_path(std::initializer_list<LuaVal>{ LuaVal(keys)... });
    }

    // settable; return self
    LuaVal & set(LuaVal const & k, LuaVal const & v);
    LuaVal & set(LuaVal const & k, LuaVal && v);
    LuaVal & set(std::string const & k, LuaVal const & v);
    LuaVal & set(std::string const & k, LuaVal && v);
    LuaVal & set(std::string const & k, std::string const & v) { return set(k, LuaVal(v)); }
    LuaVal & set(std::string const & k, char const * v) { return set(k, LuaVal(v)); }
    LuaVal & set(char const * k, LuaVal const & v) { return set(std::string(k), v); }
    LuaVal & set(char const * k, LuaVal && v) { return set(std::string(k), std::move(v)); }
    LuaVal & set(char const * k, int v) { return set(std::string(k), LuaVal(v)); }
    LuaVal & set(char const * k, double v) { return set(std::string(k), LuaVal(v)); }
    LuaVal & set(char const * k, char const * v) { return set(std::string(k), LuaVal(v)); }
    LuaVal & set(int k, LuaVal const & v);
    LuaVal & set(int k, LuaVal && v);
    LuaVal & set(int k, std::string const & v) { return set(k, LuaVal(v)); }
    LuaVal & set(int k, char const * v) { return set(k, LuaVal(v)); }
    LuaVal & set(double k, LuaVal const & v) { return set(LuaVal(k), v); }
    LuaVal & set(double k, LuaVal && v) { return set(LuaVal(k), std::move(v)); }

    // settable ignore if exists; return self
    LuaVal & setignore(LuaVal const & k, LuaVal const & v);
    LuaVal & setignore(LuaVal const & k, LuaVal && v);

    // erase; return self
    LuaVal & erase(LuaVal const & k);
    LuaVal & rem(LuaVal const & k) { return erase(k); }

    // table array size, not actual element count
    unsigned int len() const;
    // table.insert; return self
    LuaVal & insert(LuaVal const & v, LuaVal const & pos = nil);
    LuaVal & insert(LuaVal && v, LuaVal const & pos = nil);
    LuaVal & insert(char const * v) { return insert(LuaVal(v)); }
    // table.remove; return self
    LuaVal & remove(LuaVal const & pos = nil);

    // get a number value
    double num() const;
    // get a boolean value
    bool boolean() const;
    // get a string value
    std::string const & str() const;
    // get a table value
    LuaTable const & tbl() const;

    bool try_as_number(double & out) const;
    bool try_as_string(std::string const *& out) const;
    bool try_as_bool(bool & out) const;

    // Returns a typetag, the internal identifier for each type.
    LuaTypeTag typetag() const { return tag; }
    // Returns the LuaVal's type as a string.
    std::string type() const { return type(typetag()); }
    // Returns the type tag's type as a string.
    static std::string type(LuaTypeTag tag);

    // Serializes the value into string.
    // errmsg is optional; on failure an empty string is returned and errmsg is assigned (not appended).
    std::string dumps(std::string* errmsg = nullptr) const;
    std::string dumps_or_throw() const;

    // Deserialize a string into a LuaVal.
    // errmsg is optional; on failure nil is returned and errmsg is assigned (not appended).
    static LuaVal loads(std::string const & string, std::string* errmsg = nullptr);
    static LuaVal loads(std::string const & string, LoadLimits const & limits, std::string* errmsg = nullptr);
    static LuaVal loads_or_throw(std::string const & string);
    static LuaVal loads_or_throw(std::string const & string, LoadLimits const & limits);

    bool operator==(LuaVal const& rhs) const;
    bool operator!=(LuaVal const& rhs) const { return !(*this == rhs); }

    // You can use !val to check for nil or false.
    explicit operator bool() const;

    LuaVal& operator=(LuaVal const& val);
    LuaVal& operator=(LuaVal && val)
    {
        tag = std::move(val.tag);
        tbl_ptr = std::move(val.tbl_ptr);
        s = std::move(val.s);
        d = std::move(val.d);
        b = std::move(val.b);
        if (val.tag == TTABLE)
            val.tbl_ptr.reset(new LuaTable());
        else
            val.tbl_ptr = nullptr;
        return *this;
    }

private:

    void InitializeSequence(std::initializer_list<LuaVal> const & l);

    template<typename T>
    void InitializeSequence(T const & l)
    {
        LuaTable & tbl = *tbl_ptr;
        unsigned int i = 0;
        for (auto const & v : l)
        {
            LuaVal vv(v);
            if (vv.isnil())
                ++i;
            else
                tbl[++i] = std::move(vv);
        }
    }

    template<typename T>
    void InitializeMap(T const & l)
    {
        LuaTable & tbl = *tbl_ptr;
        for (auto const & e : l)
        {
            LuaVal k(e.first);
            LuaVal v(e.second);
            if (!k.isnil() && !v.isnil())
                tbl[std::move(k)] = std::move(v);
        }
    }

    friend size_t LuaValHash(LuaVal const & v);

    LuaTypeTag tag;
    TblPtr tbl_ptr;
    std::string s;
    // int64_t i; // lua 5.3 support? Numbers are stored as double today.
    double d;
    bool b;
};

namespace lua_val {

inline LuaVal nil() { return LuaVal(TNIL); }
inline LuaVal number(double v) { return LuaVal(v); }
inline LuaVal number(int v) { return LuaVal(v); }
inline LuaVal number(int64_t v) { return LuaVal(v); }
inline LuaVal number(float v) { return LuaVal(v); }
inline LuaVal string(std::string const & v) { return LuaVal(v); }
inline LuaVal string(char const * v) { return LuaVal(v); }
inline LuaVal boolean(bool v) { return LuaVal(v); }
inline LuaVal table() { return LuaVal::table(); }
inline LuaVal array(std::initializer_list<LuaVal> const & items) { return LuaVal(items); }
LuaVal map(std::initializer_list<std::pair<LuaVal, LuaVal>> const & entries);

} // namespace lua_val

#endif
