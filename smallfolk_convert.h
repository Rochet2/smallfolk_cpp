#ifndef SMALLFOLK_CONVERT_H
#define SMALLFOLK_CONVERT_H

#include "smallfolk.h"

#include <array>
#include <deque>
#include <forward_list>
#include <list>
#include <map>
#include <unordered_map>
#include <vector>

namespace lua_val {

template<typename T>
LuaVal from_sequence(T const & items)
{
    LuaVal value(TTABLE);
    unsigned int index = 0;
    for (auto const & item : items)
    {
        LuaVal converted(item);
        if (converted.isnil())
            ++index;
        else
            value.set(++index, std::move(converted));
    }
    return value;
}

template<typename T>
LuaVal from_map(T const & items)
{
    LuaVal value(TTABLE);
    for (auto const & entry : items)
    {
        LuaVal key(entry.first);
        LuaVal val(entry.second);
        if (!key.isnil() && !val.isnil())
            value.set(std::move(key), std::move(val));
    }
    return value;
}

template<typename T>
LuaVal array(T const & items)
{
    return from_sequence(items);
}

template<typename T>
LuaVal map(T const & items)
{
    return from_map(items);
}

} // namespace lua_val

#endif
