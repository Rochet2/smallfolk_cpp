#ifndef SMALLFOLK_SCHEMA_H
#define SMALLFOLK_SCHEMA_H

#include "smallfolk.h"

#include <cstddef>
#include <initializer_list>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

enum class SchemaKind
{
    Any,
    Null,
    Bool,
    Number,
    String,
    Array,
    Object,
    OneOf,
};

typedef bool (*SchemaValidatorFn)(LuaVal const & value, std::string * err, char const * path);

struct Schema
{
    SchemaKind kind = SchemaKind::Any;

    // External child refs (static presets, Field.schema, cyclic schema::value() graph).
    Schema const * items = nullptr;
    Schema const * values = nullptr;
    Schema const * const * alternatives = nullptr;
    size_t alternative_count = 0;

    // Owned children (self-contained factory-built schemas).
    std::unique_ptr<Schema> items_owned;
    std::unique_ptr<Schema> values_owned;
    std::vector<Schema> alternatives_owned;

    unsigned min_items = 0;
    unsigned max_items = static_cast<unsigned>(-1);

    struct Field
    {
        char const * name = nullptr;
        Schema const * schema = nullptr;
        bool required = false;
    };
    Field const * fields = nullptr;
    size_t field_count = 0;
    bool allow_extra_keys = true;

    bool has_min = false;
    bool has_max = false;
    double min_value = 0.0;
    double max_value = 0.0;

    std::vector<std::string> enum_strings;

    SchemaValidatorFn validator = nullptr;

    bool has_min_length = false;
    bool has_max_length = false;
    unsigned min_length = 0;
    unsigned max_length = 0;

    Schema() = default;
    Schema(Schema const & other);
    Schema & operator=(Schema const & other);
    Schema(Schema && other) = default;
    Schema & operator=(Schema && other) = default;
};

struct ValidateLimits
{
    // Maximum recursive validation depth (mirrors parse nesting limits).
    unsigned max_validation_depth = 256;
    // Maximum validate_impl steps across the whole tree (guards OneOf CPU blowups).
    size_t max_validation_steps = 100000;
};

inline ValidateLimits untrusted_validate_limits()
{
    ValidateLimits limits;
    limits.max_validation_depth = 64;
    limits.max_validation_steps = 10000;
    return limits;
}

class CompiledSchema
{
public:
    explicit CompiledSchema(Schema const & schema);

    Schema const & raw() const { return schema_; }
    bool validate(LuaVal const & value, std::string * err = nullptr) const;
    bool validate(
        LuaVal const & value,
        ValidateLimits const & limits,
        std::string * err = nullptr) const;

private:
    struct ValidateContext
    {
        ValidateLimits limits;
        unsigned depth = 0;
        size_t steps = 0;
    };

    struct ObjectIndex
    {
        Schema const * schema;
        std::unordered_map<std::string, Schema const *> fields;
    };

    Schema const & schema_;
    std::vector<ObjectIndex> object_indexes_;

    void compile_node(Schema const & node, std::unordered_set<Schema const *> & visited);
    ObjectIndex const * find_object_index(Schema const & node) const;
    bool validate_impl(
        LuaVal const & value,
        Schema const & node,
        std::string * err,
        std::string const & path,
        ValidateContext & ctx) const;

    static bool consume_validation_step(
        ValidateContext & ctx,
        std::string * err,
        std::string const & path);
};

// Compiles schema on each call; prefer CompiledSchema for hot paths.
bool validate(LuaVal const & value, Schema const & schema, std::string * err = nullptr);
bool validate(
    LuaVal const & value,
    Schema const & schema,
    ValidateLimits const & limits,
    std::string * err = nullptr);
bool validate(LuaVal const & value, CompiledSchema const & schema, std::string * err = nullptr);
bool validate(
    LuaVal const & value,
    CompiledSchema const & schema,
    ValidateLimits const & limits,
    std::string * err = nullptr);

LuaVal loads_validated(
    std::string const & input,
    Schema const & schema,
    std::string * err = nullptr);
LuaVal loads_validated(
    std::string const & input,
    Schema const & schema,
    LoadLimits const & limits,
    std::string * err = nullptr);
LuaVal loads_validated(
    std::string const & input,
    CompiledSchema const & schema,
    std::string * err = nullptr);
LuaVal loads_validated(
    std::string const & input,
    CompiledSchema const & schema,
    LoadLimits const & limits,
    std::string * err = nullptr);
LuaVal loads_validated(
    std::string const & input,
    CompiledSchema const & schema,
    LoadLimits const & load_limits,
    ValidateLimits const & validate_limits,
    std::string * err = nullptr);

LuaVal loads_validated_or_throw(std::string const & input, Schema const & schema);
LuaVal loads_validated_or_throw(
    std::string const & input,
    Schema const & schema,
    LoadLimits const & limits);
LuaVal loads_validated_or_throw(std::string const & input, CompiledSchema const & schema);
LuaVal loads_validated_or_throw(
    std::string const & input,
    CompiledSchema const & schema,
    LoadLimits const & limits);
LuaVal loads_validated_or_throw(
    std::string const & input,
    CompiledSchema const & schema,
    LoadLimits const & load_limits,
    ValidateLimits const & validate_limits);

namespace schema {

// Preset references are safe to share across threads after process startup.
// Factory functions return self-contained Schema values (move-friendly). Internal
// cyclic presets (schema::value()) remain static.

Schema const & any();
Schema const & null();
Schema const & boolean();
Schema const & number();
Schema const & string();
Schema const & object();
Schema const & array();

Schema const & string_array();
Schema const & number_array();
Schema const & boolean_array();
Schema const & number_or_string();
Schema const & value();

Schema number_range(double min_value, double max_value);
Schema string_length(unsigned min_length, unsigned max_length);
Schema array_of(
    Schema element,
    unsigned min_items = 0,
    unsigned max_items = static_cast<unsigned>(-1));
Schema map_of(
    Schema value_schema,
    bool allow_extra_keys = true);
Schema string_enum(std::vector<std::string> values);
Schema string_enum(std::initializer_list<std::string> values);
Schema one_of(std::vector<Schema> alternatives);
Schema one_of(std::initializer_list<Schema> alternatives);

} // namespace schema

#endif
