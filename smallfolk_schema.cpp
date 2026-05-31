#include "smallfolk_schema.h"

#include <cmath>
#include <cstdio>
#include <mutex>
#include <sstream>
#include <unordered_set>

namespace
{
    Schema const * item_schema(Schema const & node)
    {
        if (node.items_owned)
            return node.items_owned.get();
        return node.items;
    }

    Schema const * value_schema(Schema const & node)
    {
        if (node.values_owned)
            return node.values_owned.get();
        return node.values;
    }

    size_t one_of_size(Schema const & node)
    {
        if (!node.alternatives_owned.empty())
            return node.alternatives_owned.size();
        return node.alternative_count;
    }

    Schema const * one_of_at(Schema const & node, size_t index)
    {
        if (!node.alternatives_owned.empty())
            return &node.alternatives_owned[index];
        return node.alternatives[index];
    }

    bool one_of_empty(Schema const & node)
    {
        return one_of_size(node) == 0;
    }

    void copy_owned_children(Schema const & other, Schema & out)
    {
        if (other.items_owned)
            out.items_owned.reset(new Schema(*other.items_owned));
        else
            out.items = other.items;

        if (other.values_owned)
            out.values_owned.reset(new Schema(*other.values_owned));
        else
            out.values = other.values;

        if (!other.alternatives_owned.empty())
        {
            out.alternatives_owned.reserve(other.alternatives_owned.size());
            for (size_t i = 0; i < other.alternatives_owned.size(); ++i)
                out.alternatives_owned.push_back(Schema(other.alternatives_owned[i]));
            out.alternatives = nullptr;
            out.alternative_count = 0;
        }
        else
        {
            out.alternatives = other.alternatives;
            out.alternative_count = other.alternative_count;
        }
    }
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
        if (schema.enum_strings.empty())
            return true;
        for (size_t i = 0; i < schema.enum_strings.size(); ++i)
        {
            if (value.str() == schema.enum_strings[i])
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

Schema::Schema(Schema const & other)
    : kind(other.kind)
    , min_items(other.min_items)
    , max_items(other.max_items)
    , fields(other.fields)
    , field_count(other.field_count)
    , allow_extra_keys(other.allow_extra_keys)
    , has_min(other.has_min)
    , has_max(other.has_max)
    , min_value(other.min_value)
    , max_value(other.max_value)
    , enum_strings(other.enum_strings)
    , validator(other.validator)
    , has_min_length(other.has_min_length)
    , has_max_length(other.has_max_length)
    , min_length(other.min_length)
    , max_length(other.max_length)
{
    copy_owned_children(other, *this);
}

Schema & Schema::operator=(Schema const & other)
{
    if (this == &other)
        return *this;
    kind = other.kind;
    items = nullptr;
    values = nullptr;
    alternatives = nullptr;
    items_owned.reset();
    values_owned.reset();
    alternatives_owned.clear();
    min_items = other.min_items;
    max_items = other.max_items;
    fields = other.fields;
    field_count = other.field_count;
    allow_extra_keys = other.allow_extra_keys;
    has_min = other.has_min;
    has_max = other.has_max;
    min_value = other.min_value;
    max_value = other.max_value;
    enum_strings = other.enum_strings;
    validator = other.validator;
    has_min_length = other.has_min_length;
    has_max_length = other.has_max_length;
    min_length = other.min_length;
    max_length = other.max_length;
    copy_owned_children(other, *this);
    return *this;
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

    if (Schema const * items = item_schema(node))
        compile_node(*items, visited);

    if (node.fields)
    {
        for (size_t i = 0; i < node.field_count; ++i)
        {
            if (node.fields[i].schema)
                compile_node(*node.fields[i].schema, visited);
        }
    }

    if (Schema const * values = value_schema(node))
        compile_node(*values, visited);

    for (size_t i = 0; i < one_of_size(node); ++i)
    {
        if (Schema const * alternative = one_of_at(node, i))
            compile_node(*alternative, visited);
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
    (void)depth_pop;

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
        else
        {
            size_t const length = value.str().size();
            if (node.has_min_length && length < node.min_length)
                return append_error(err, path + ": string too short");
            if (node.has_max_length && length > node.max_length)
                return append_error(err, path + ": string too long");
            if (!matches_enum_string(value, node))
                return append_error(err, path + ": string not in enum");
        }
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
            if (Schema const * items = item_schema(node))
            {
                for (unsigned int i = 1; i <= length; ++i)
                {
                    std::ostringstream segment;
                    segment << "[" << i << "]";
                    if (!validate_impl(value.get(static_cast<int>(i)), *items, err, join_path(path, segment.str()), ctx))
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

            if (Schema const * values = value_schema(node))
            {
                for (LuaVal::LuaTable::const_iterator it = value.tbl().begin(); it != value.tbl().end(); ++it)
                {
                    if (!it->first.isstring())
                        return append_error(err, path + ": object key must be string");
                    if (!validate_impl(it->second, *values, err, join_path(path, key_to_segment(it->first)), ctx))
                        return false;
                }
            }

            if (!node.allow_extra_keys && !value_schema(node))
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
        if (one_of_empty(node))
            return append_error(err, path + ": empty one_of schema");
        else
        {
            std::string branch_errors;
            for (size_t i = 0; i < one_of_size(node); ++i)
            {
                Schema const * alternative = one_of_at(node, i);
                if (!alternative)
                    continue;
                if (!kind_accepts_value_tag(alternative->kind, value))
                    continue;
                std::string branch_err;
                if (validate_impl(value, *alternative, &branch_err, path, ctx))
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

Schema kind_schema(SchemaKind kind)
{
    Schema schema;
    schema.kind = kind;
    return schema;
}

Schema const & any()
{
    static Schema const schema = kind_schema(SchemaKind::Any);
    return schema;
}

Schema const & null()
{
    static Schema const schema = kind_schema(SchemaKind::Null);
    return schema;
}

Schema const & boolean()
{
    static Schema const schema = kind_schema(SchemaKind::Bool);
    return schema;
}

Schema const & number()
{
    static Schema const schema = kind_schema(SchemaKind::Number);
    return schema;
}

Schema const & string()
{
    static Schema const schema = kind_schema(SchemaKind::String);
    return schema;
}

Schema const & object()
{
    static Schema const schema = [] {
        Schema s;
        s.kind = SchemaKind::Object;
        s.allow_extra_keys = true;
        return s;
    }();
    return schema;
}

Schema const & array()
{
    static Schema const schema = kind_schema(SchemaKind::Array);
    return schema;
}

Schema const & string_array()
{
    static Schema schema;
    static std::once_flag once;
    std::call_once(once, [] {
        schema.kind = SchemaKind::Array;
        schema.items = &string();
    });
    return schema;
}

Schema const & number_array()
{
    static Schema schema;
    static std::once_flag once;
    std::call_once(once, [] {
        schema.kind = SchemaKind::Array;
        schema.items = &number();
    });
    return schema;
}

Schema const & boolean_array()
{
    static Schema schema;
    static std::once_flag once;
    std::call_once(once, [] {
        schema.kind = SchemaKind::Array;
        schema.items = &boolean();
    });
    return schema;
}

Schema const & number_or_string()
{
    static Schema number_alt = kind_schema(SchemaKind::Number);
    static Schema string_alt = kind_schema(SchemaKind::String);
    static Schema const * alternatives[] = { &number_alt, &string_alt };
    static Schema schema;
    static std::once_flag once;
    std::call_once(once, [&] {
        schema.kind = SchemaKind::OneOf;
        schema.alternatives = alternatives;
        schema.alternative_count = 2;
    });
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
        array_node.kind = SchemaKind::Array;
        array_node.items = &value_node;
        map_node.kind = SchemaKind::Object;
        map_node.allow_extra_keys = true;
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
    Schema schema = kind_schema(SchemaKind::Number);
    schema.has_min = true;
    schema.has_max = true;
    schema.min_value = min_value;
    schema.max_value = max_value;
    return schema;
}

Schema string_length(unsigned min_length, unsigned max_length)
{
    Schema schema = kind_schema(SchemaKind::String);
    schema.has_min_length = true;
    schema.has_max_length = true;
    schema.min_length = min_length;
    schema.max_length = max_length;
    return schema;
}

Schema array_of(Schema element, unsigned min_items, unsigned max_items)
{
    Schema schema;
    schema.kind = SchemaKind::Array;
    schema.min_items = min_items;
    schema.max_items = max_items;
    schema.items_owned.reset(new Schema(std::move(element)));
    return schema;
}

Schema map_of(Schema value_schema, bool allow_extra_keys)
{
    Schema schema;
    schema.kind = SchemaKind::Object;
    schema.allow_extra_keys = allow_extra_keys;
    schema.values_owned.reset(new Schema(std::move(value_schema)));
    return schema;
}

Schema string_enum(std::vector<std::string> values)
{
    Schema schema;
    schema.kind = SchemaKind::String;
    schema.enum_strings = std::move(values);
    return schema;
}

Schema string_enum(std::initializer_list<std::string> values)
{
    return string_enum(std::vector<std::string>(values));
}

Schema one_of(std::vector<Schema> alternatives)
{
    Schema schema;
    schema.kind = SchemaKind::OneOf;
    schema.alternatives_owned = std::move(alternatives);
    return schema;
}

Schema one_of(std::initializer_list<Schema> alternatives)
{
    return one_of(std::vector<Schema>(alternatives));
}

} // namespace schema
