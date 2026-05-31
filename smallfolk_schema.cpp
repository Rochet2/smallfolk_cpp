#include "smallfolk_schema.h"

#include <cmath>
#include <cstdio>
#include <deque>
#include <mutex>
#include <sstream>
#include <unordered_set>

namespace
{
    bool append_error(std::string * err, std::string const & message)
    {
        if (err)
        {
            if (!err->empty())
                *err += "; ";
            *err += message;
        }
        return false;
    }

    std::string key_to_segment(LuaVal const & key)
    {
        if (key.isnumber() && std::floor(key.num()) == key.num() && key.num() >= 1.0)
        {
            char buffer[32];
            std::snprintf(buffer, sizeof(buffer), "[%u]", static_cast<unsigned>(key.num()));
            return buffer;
        }
        if (key.isstring())
            return std::string(".") + key.str();
        return "[?]";
    }

    std::string join_path(std::string const & parent, std::string const & segment)
    {
        if (parent.empty() || parent == "$")
            return std::string("$") + segment;
        return parent + segment;
    }

    bool matches_enum_string(LuaVal const & value, Schema const & schema)
    {
        if (!schema.enum_values || schema.enum_count == 0)
            return true;
        for (size_t i = 0; i < schema.enum_count; ++i)
        {
            if (value.str() == schema.enum_values[i])
                return true;
        }
        return false;
    }

    bool run_custom_validator(
        LuaVal const & value,
        Schema const & schema,
        std::string * err,
        std::string const & path)
    {
        if (!schema.validator)
            return true;
        return schema.validator(value, err, path.c_str());
    }

    bool kind_accepts_value_tag(SchemaKind kind, LuaVal const & value)
    {
        switch (kind)
        {
        case SchemaKind::Any:
            return true;
        case SchemaKind::Null:
            return value.isnil();
        case SchemaKind::Bool:
            return value.isbool();
        case SchemaKind::Number:
            return value.isnumber();
        case SchemaKind::String:
            return value.isstring();
        case SchemaKind::Array:
        case SchemaKind::Object:
        case SchemaKind::OneOf:
            return true;
        }
        return true;
    }
}

CompiledSchema::CompiledSchema(Schema const & schema) : schema_(schema)
{
    std::unordered_set<Schema const *> visited;
    compile_node(schema_, visited);
}

void CompiledSchema::compile_node(Schema const & node, std::unordered_set<Schema const *> & visited)
{
    if (!visited.insert(&node).second)
        return;

    if (node.kind == SchemaKind::Object && node.fields && node.field_count > 0)
    {
        ObjectIndex index;
        index.schema = &node;
        for (size_t i = 0; i < node.field_count; ++i)
        {
            if (node.fields[i].name && node.fields[i].schema)
                index.fields[node.fields[i].name] = node.fields[i].schema;
        }
        object_indexes_.push_back(index);
    }

    if (node.items)
        compile_node(*node.items, visited);

    if (node.fields)
    {
        for (size_t i = 0; i < node.field_count; ++i)
        {
            if (node.fields[i].schema)
                compile_node(*node.fields[i].schema, visited);
        }
    }

    if (node.values)
        compile_node(*node.values, visited);

    if (node.alternatives)
    {
        for (size_t i = 0; i < node.alternative_count; ++i)
        {
            if (node.alternatives[i])
                compile_node(*node.alternatives[i], visited);
        }
    }
}

CompiledSchema::ObjectIndex const * CompiledSchema::find_object_index(Schema const & node) const
{
    for (size_t i = 0; i < object_indexes_.size(); ++i)
    {
        if (object_indexes_[i].schema == &node)
            return &object_indexes_[i];
    }
    return nullptr;
}

bool CompiledSchema::consume_validation_step(
    ValidateContext & ctx,
    std::string * err,
    std::string const & path)
{
    ++ctx.steps;
    if (ctx.steps > ctx.limits.max_validation_steps)
        return append_error(err, path + ": validation step limit exceeded");
    return true;
}

bool CompiledSchema::validate(LuaVal const & value, std::string * err) const
{
    return validate(value, ValidateLimits{}, err);
}

bool CompiledSchema::validate(
    LuaVal const & value,
    ValidateLimits const & limits,
    std::string * err) const
{
    ValidateContext ctx;
    ctx.limits = limits;
    return validate_impl(value, schema_, err, "$", ctx);
}

bool CompiledSchema::validate_impl(
    LuaVal const & value,
    Schema const & node,
    std::string * err,
    std::string const & path,
    ValidateContext & ctx) const
{
    if (!consume_validation_step(ctx, err, path))
        return false;

    if (ctx.depth >= ctx.limits.max_validation_depth)
        return append_error(err, path + ": validation depth limit exceeded");

    ++ctx.depth;
    struct DepthPop
    {
        ValidateContext & ctx;
        explicit DepthPop(ValidateContext & c) : ctx(c) {}
        ~DepthPop() { --ctx.depth; }
    } depth_pop(ctx);

    switch (node.kind)
    {
    case SchemaKind::Any:
        break;
    case SchemaKind::Null:
        if (!value.isnil())
            return append_error(err, path + ": expected nil");
        break;
    case SchemaKind::Bool:
        if (!value.isbool())
            return append_error(err, path + ": expected boolean");
        break;
    case SchemaKind::Number:
        if (!value.isnumber())
            return append_error(err, path + ": expected number");
        else
        {
            double number = value.num();
            if (node.has_min && number < node.min_value)
                return append_error(err, path + ": number below minimum");
            if (node.has_max && number > node.max_value)
                return append_error(err, path + ": number above maximum");
        }
        break;
    case SchemaKind::String:
        if (!value.isstring())
            return append_error(err, path + ": expected string");
        else if (!matches_enum_string(value, node))
            return append_error(err, path + ": string not in enum");
        break;
    case SchemaKind::Array:
        if (!value.istable())
            return append_error(err, path + ": expected array table");
        else
        {
            unsigned int length = value.len();
            if (length < node.min_items)
                return append_error(err, path + ": array too short");
            if (length > node.max_items)
                return append_error(err, path + ": array too long");
            if (node.items)
            {
                for (unsigned int i = 1; i <= length; ++i)
                {
                    std::ostringstream segment;
                    segment << "[" << i << "]";
                    if (!validate_impl(value.get(static_cast<int>(i)), *node.items, err, join_path(path, segment.str()), ctx))
                        return false;
                }
            }
        }
        break;
    case SchemaKind::Object:
        if (!value.istable())
            return append_error(err, path + ": expected object table");
        else
        {
            ObjectIndex const * index = find_object_index(node);

            if (node.fields)
            {
                for (size_t i = 0; i < node.field_count; ++i)
                {
                    Schema::Field const & field = node.fields[i];
                    if (!field.name || !field.schema)
                        continue;
                    LuaVal const * found = value.try_get(std::string(field.name));
                    if (field.required && !found)
                        return append_error(err, join_path(path, std::string(".") + field.name) + ": required field missing");
                    if (found && !validate_impl(*found, *field.schema, err, join_path(path, std::string(".") + field.name), ctx))
                        return false;
                }
            }

            if (node.values)
            {
                for (LuaVal::LuaTable::const_iterator it = value.tbl().begin(); it != value.tbl().end(); ++it)
                {
                    if (!it->first.isstring())
                        return append_error(err, path + ": object key must be string");
                    if (!validate_impl(it->second, *node.values, err, join_path(path, key_to_segment(it->first)), ctx))
                        return false;
                }
            }

            if (!node.allow_extra_keys && !node.values)
            {
                for (LuaVal::LuaTable::const_iterator it = value.tbl().begin(); it != value.tbl().end(); ++it)
                {
                    if (!it->first.isstring())
                        return append_error(err, path + ": extra non-string key");
                    if (index)
                    {
                        if (index->fields.find(it->first.str()) == index->fields.end())
                            return append_error(err, join_path(path, key_to_segment(it->first)) + ": unexpected field");
                    }
                    else
                    {
                        bool known = false;
                        for (size_t i = 0; i < node.field_count; ++i)
                        {
                            if (node.fields[i].name && it->first.str() == node.fields[i].name)
                            {
                                known = true;
                                break;
                            }
                        }
                        if (!known)
                            return append_error(err, join_path(path, key_to_segment(it->first)) + ": unexpected field");
                    }
                }
            }
        }
        break;
    case SchemaKind::OneOf:
        if (!node.alternatives || node.alternative_count == 0)
            return append_error(err, path + ": empty one_of schema");
        else
        {
            std::string branch_errors;
            for (size_t i = 0; i < node.alternative_count; ++i)
            {
                if (!node.alternatives[i])
                    continue;
                if (!kind_accepts_value_tag(node.alternatives[i]->kind, value))
                    continue;
                std::string branch_err;
                if (validate_impl(value, *node.alternatives[i], &branch_err, path, ctx))
                    return run_custom_validator(value, node, err, path);
                if (!branch_err.empty())
                {
                    if (!branch_errors.empty())
                        branch_errors += " | ";
                    branch_errors += branch_err;
                }
            }
            if (!branch_errors.empty())
                return append_error(err, path + ": one_of mismatch (" + branch_errors + ")");
            return append_error(err, path + ": one_of mismatch");
        }
    }

    return run_custom_validator(value, node, err, path);
}

bool validate(LuaVal const & value, Schema const & schema, std::string * err)
{
    CompiledSchema compiled(schema);
    return compiled.validate(value, err);
}

bool validate(
    LuaVal const & value,
    Schema const & schema,
    ValidateLimits const & limits,
    std::string * err)
{
    CompiledSchema compiled(schema);
    return compiled.validate(value, limits, err);
}

bool validate(LuaVal const & value, CompiledSchema const & schema, std::string * err)
{
    return schema.validate(value, err);
}

bool validate(
    LuaVal const & value,
    CompiledSchema const & schema,
    ValidateLimits const & limits,
    std::string * err)
{
    return schema.validate(value, limits, err);
}

LuaVal loads_validated(std::string const & input, Schema const & schema, std::string * err)
{
    return loads_validated(input, schema, LuaVal::get_load_limits(), err);
}

LuaVal loads_validated(
    std::string const & input,
    Schema const & schema,
    LoadLimits const & limits,
    std::string * err)
{
    std::string load_err;
    LuaVal value = LuaVal::loads(input, limits, &load_err);
    if (!load_err.empty())
    {
        if (err)
            *err = load_err;
        return LuaVal::nil;
    }

    std::string validate_err;
    if (!validate(value, schema, &validate_err))
    {
        if (err)
            *err = validate_err;
        return LuaVal::nil;
    }

    return value;
}

LuaVal loads_validated(std::string const & input, CompiledSchema const & schema, std::string * err)
{
    return loads_validated(input, schema, LuaVal::get_load_limits(), err);
}

LuaVal loads_validated(
    std::string const & input,
    CompiledSchema const & schema,
    LoadLimits const & limits,
    std::string * err)
{
    return loads_validated(input, schema, limits, ValidateLimits{}, err);
}

LuaVal loads_validated(
    std::string const & input,
    CompiledSchema const & schema,
    LoadLimits const & load_limits,
    ValidateLimits const & validate_limits,
    std::string * err)
{
    std::string load_err;
    LuaVal value = LuaVal::loads(input, load_limits, &load_err);
    if (!load_err.empty())
    {
        if (err)
            *err = load_err;
        return LuaVal::nil;
    }

    std::string validate_err;
    if (!schema.validate(value, validate_limits, &validate_err))
    {
        if (err)
            *err = validate_err;
        return LuaVal::nil;
    }

    return value;
}

LuaVal loads_validated_or_throw(std::string const & input, Schema const & schema)
{
    return loads_validated_or_throw(input, schema, LuaVal::get_load_limits());
}

LuaVal loads_validated_or_throw(
    std::string const & input,
    Schema const & schema,
    LoadLimits const & limits)
{
    std::string err;
    LuaVal value = loads_validated(input, schema, limits, &err);
    if (!err.empty())
        throw smallfolk_exception("%s", err.c_str());
    return value;
}

LuaVal loads_validated_or_throw(std::string const & input, CompiledSchema const & schema)
{
    return loads_validated_or_throw(input, schema, LuaVal::get_load_limits());
}

LuaVal loads_validated_or_throw(
    std::string const & input,
    CompiledSchema const & schema,
    LoadLimits const & limits)
{
    return loads_validated_or_throw(input, schema, limits, ValidateLimits{});
}

LuaVal loads_validated_or_throw(
    std::string const & input,
    CompiledSchema const & schema,
    LoadLimits const & load_limits,
    ValidateLimits const & validate_limits)
{
    std::string err;
    LuaVal value = loads_validated(input, schema, load_limits, validate_limits, &err);
    if (!err.empty())
        throw smallfolk_exception("%s", err.c_str());
    return value;
}

namespace schema
{
namespace
{
    std::mutex g_schema_factory_mutex;

    struct EnumSchemaStorage
    {
        std::vector<std::string> strings;
        std::vector<char const *> pointers;
        Schema schema;

        EnumSchemaStorage(char const * const * values, size_t count)
        {
            strings.reserve(count);
            for (size_t i = 0; i < count; ++i)
                strings.push_back(values[i]);
            pointers.reserve(count);
            for (size_t i = 0; i < count; ++i)
                pointers.push_back(strings[i].c_str());
            schema.kind = SchemaKind::String;
            schema.enum_values = pointers.data();
            schema.enum_count = count;
        }
    };

    struct OneOfSchemaStorage
    {
        std::vector<Schema const *> alternatives;
        Schema schema;

        OneOfSchemaStorage(Schema const * const * values, size_t count)
        {
            alternatives.assign(values, values + count);
            schema.kind = SchemaKind::OneOf;
            schema.alternatives = alternatives.data();
            schema.alternative_count = count;
        }
    };
}

Schema const & any()
{
    static Schema const schema = { SchemaKind::Any };
    return schema;
}

Schema const & null()
{
    static Schema const schema = { SchemaKind::Null };
    return schema;
}

Schema const & boolean()
{
    static Schema const schema = { SchemaKind::Bool };
    return schema;
}

Schema const & number()
{
    static Schema const schema = { SchemaKind::Number };
    return schema;
}

Schema const & string()
{
    static Schema const schema = { SchemaKind::String };
    return schema;
}

Schema const & object()
{
    static Schema const schema = { SchemaKind::Object, nullptr, 0, static_cast<unsigned>(-1), nullptr, 0, true };
    return schema;
}

Schema const & array()
{
    static Schema const schema = { SchemaKind::Array, nullptr, 0, static_cast<unsigned>(-1) };
    return schema;
}

Schema const & string_array()
{
    static Schema schema;
    static std::once_flag once;
    std::call_once(once, [] { schema = { SchemaKind::Array, &string(), 0, static_cast<unsigned>(-1) }; });
    return schema;
}

Schema const & number_array()
{
    static Schema schema;
    static std::once_flag once;
    std::call_once(once, [] { schema = { SchemaKind::Array, &number(), 0, static_cast<unsigned>(-1) }; });
    return schema;
}

Schema const & boolean_array()
{
    static Schema schema;
    static std::once_flag once;
    std::call_once(once, [] { schema = { SchemaKind::Array, &boolean(), 0, static_cast<unsigned>(-1) }; });
    return schema;
}

Schema const & number_or_string()
{
    static Schema number_alt = { SchemaKind::Number };
    static Schema string_alt = { SchemaKind::String };
    static Schema const * alternatives[] = { &number_alt, &string_alt };
    static Schema const schema = {
        SchemaKind::OneOf,
        nullptr,
        0,
        static_cast<unsigned>(-1),
        nullptr,
        0,
        true,
        false,
        false,
        0.0,
        0.0,
        nullptr,
        0,
        alternatives,
        2,
        nullptr
    };
    return schema;
}

Schema const & value()
{
    static Schema value_node;
    static Schema array_node;
    static Schema map_node;
    static Schema const * alternatives[6];
    static std::once_flag once;
    std::call_once(once, [] {
        value_node.kind = SchemaKind::OneOf;
        array_node = { SchemaKind::Array, &value_node, 0, static_cast<unsigned>(-1) };
        map_node = { SchemaKind::Object, nullptr, 0, static_cast<unsigned>(-1), nullptr, 0, true };
        map_node.values = &value_node;
        alternatives[0] = &null();
        alternatives[1] = &boolean();
        alternatives[2] = &number();
        alternatives[3] = &string();
        alternatives[4] = &array_node;
        alternatives[5] = &map_node;
        value_node.alternatives = alternatives;
        value_node.alternative_count = 6;
    });
    return value_node;
}

Schema number_range(double min_value, double max_value)
{
    Schema schema = { SchemaKind::Number };
    schema.has_min = true;
    schema.has_max = true;
    schema.min_value = min_value;
    schema.max_value = max_value;
    return schema;
}

Schema array_of(Schema const & element, unsigned min_items, unsigned max_items)
{
    std::lock_guard<std::mutex> lock(g_schema_factory_mutex);
    static std::deque<Schema> elements;
    static std::deque<Schema> arrays;
    elements.push_back(element);
    Schema schema = { SchemaKind::Array, &elements.back(), min_items, max_items };
    arrays.push_back(schema);
    return arrays.back();
}

Schema map_of(Schema const & value_schema, bool allow_extra_keys)
{
    std::lock_guard<std::mutex> lock(g_schema_factory_mutex);
    static std::deque<Schema> values;
    static std::deque<Schema> maps;
    values.push_back(value_schema);
    Schema schema = { SchemaKind::Object, nullptr, 0, static_cast<unsigned>(-1), nullptr, 0, allow_extra_keys };
    schema.values = &values.back();
    maps.push_back(schema);
    return maps.back();
}

Schema string_enum(char const * const * values, size_t count)
{
    std::lock_guard<std::mutex> lock(g_schema_factory_mutex);
    static std::deque<EnumSchemaStorage> storage;
    storage.emplace_back(values, count);
    return storage.back().schema;
}

Schema one_of(Schema const * const * alternatives, size_t count)
{
    std::lock_guard<std::mutex> lock(g_schema_factory_mutex);
    static std::deque<OneOfSchemaStorage> storage;
    storage.emplace_back(alternatives, count);
    return storage.back().schema;
}

} // namespace schema
