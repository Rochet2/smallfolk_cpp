#include "smallfolk.h"

#include <chrono>
#include <iostream>
#include <string>

namespace
{
    using clock = std::chrono::steady_clock;

    template<typename Fn>
    double measure_ms(Fn fn, int iterations)
    {
        auto const start = clock::now();
        for (int i = 0; i < iterations; ++i)
            fn();
        auto const end = clock::now();
        return std::chrono::duration<double, std::milli>(end - start).count() / iterations;
    }
}

int main()
{
    LuaVal sample = {
        LuaVal(true),
        LuaVal("somestring"),
        LuaVal(123.456),
        LuaVal(true),
        lua_val::map({
            { "t", -678 },
            { "test", 123.45600128173828 },
            { "f", 268435455 },
            { "subtable", lua_val::array({ 1, 2, 3 }) }
        })
    };

    std::string serialized = sample.dumps();
    int const iterations = 10000;

    double serialize_ms = measure_ms([&]() {
        std::string out = sample.dumps();
        if (out.empty())
            throw std::runtime_error("serialize failed");
    }, iterations);

    double deserialize_ms = measure_ms([&]() {
        LuaVal value = LuaVal::loads(serialized);
        if (value.isnil())
            throw std::runtime_error("deserialize failed");
    }, iterations);

    double roundtrip_ms = measure_ms([&]() {
        LuaVal value = LuaVal::loads(sample.dumps());
        std::string out = value.dumps();
        if (out.empty())
            throw std::runtime_error("roundtrip failed");
    }, iterations);

    std::cout << "Serialized payload (" << serialized.size() << " bytes):" << std::endl;
    std::cout << serialized << std::endl << std::endl;
    std::cout << "Iterations: " << iterations << std::endl;
    std::cout << "serialize avg:   " << serialize_ms << " ms" << std::endl;
    std::cout << "deserialize avg: " << deserialize_ms << " ms" << std::endl;
    std::cout << "round-trip avg:  " << roundtrip_ms << " ms" << std::endl;

    return 0;
}
