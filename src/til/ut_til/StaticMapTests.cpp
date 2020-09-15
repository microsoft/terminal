// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include <til/static_map.h>

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;

using namespace std::string_view_literals;

class StaticMapTests
{
    TEST_CLASS(StaticMapTests);

    TEST_METHOD(Basic)
    {
        til::static_map intIntMap{
            std::pair{ 1, 100 },
            std::pair{ 3, 300 },
            std::pair{ 5, 500 },
        };

        VERIFY_ARE_EQUAL(100, intIntMap.at(1));
        VERIFY_ARE_EQUAL(300, intIntMap.at(3));
        VERIFY_ARE_EQUAL(500, intIntMap.at(5));

        int unused{};
        VERIFY_THROWS(unused = intIntMap.at(0), std::runtime_error);
        VERIFY_THROWS(unused = intIntMap.at(7), std::runtime_error);
    }

    TEST_METHOD(Unsorted)
    {
        til::static_map intIntMap{
            std::pair{ 5, 500 },
            std::pair{ 3, 300 },
            std::pair{ 1, 100 },
        };

        VERIFY_ARE_EQUAL(100, intIntMap.at(1));
        VERIFY_ARE_EQUAL(300, intIntMap.at(3));
        VERIFY_ARE_EQUAL(500, intIntMap.at(5));

        int unused{};
        VERIFY_THROWS(unused = intIntMap.at(0), std::runtime_error);
        VERIFY_THROWS(unused = intIntMap.at(7), std::runtime_error);
    }

    TEST_METHOD(StringViewKeys)
    {
        // We have to use the string view literal type here, as leaving the strings unviewed
        // will result in a static_map<const char *, ...>
        // Deduction guides are only applied when *no* template arguments are specified,
        // which means we would need to specify them all, including the comparator and number of entries.
        til::static_map stringIntMap{
            std::pair{ "xylophones"sv, 100 },
            std::pair{ "apples"sv, 200 },
            std::pair{ "grapes"sv, 300 },
            std::pair{ "pears"sv, 400 },
        };

        VERIFY_ARE_EQUAL(100, stringIntMap.at("xylophones"));
        VERIFY_ARE_EQUAL(300, stringIntMap.at("grapes"));
        VERIFY_ARE_EQUAL(400, stringIntMap.at("pears"));
        VERIFY_ARE_EQUAL(200, stringIntMap.at("apples"));

        int unused{};
        VERIFY_THROWS(unused = stringIntMap.at("0_hello"), std::runtime_error);
        VERIFY_THROWS(unused = stringIntMap.at("z_world"), std::runtime_error);
    }

    TEST_METHOD(Find)
    {
        til::static_map intIntMap{
            std::pair{ 5, 500 },
        };

        VERIFY_ARE_NOT_EQUAL(intIntMap.end(), intIntMap.find(5));
        VERIFY_ARE_EQUAL(intIntMap.end(), intIntMap.find(7));
    }

#pragma warning(push)
#pragma warning(disable : 26446) // Suppress bounds.4 check for subscript operator.
    TEST_METHOD(Subscript)
    {
        til::static_map intIntMap{
            std::pair{ 5, 500 },
        };

        VERIFY_ARE_EQUAL(500, intIntMap[5]);
        int unused{};
        VERIFY_THROWS(unused = intIntMap[7], std::runtime_error);
    }
#pragma warning(pop)

    TEST_METHOD(Presort)
    {
        static constexpr til::presorted_static_map intIntMap{
            std::pair{ 1, 100 },
            std::pair{ 3, 300 },
            std::pair{ 5, 500 },
        };

        VERIFY_ARE_EQUAL(100, intIntMap.at(1));
        VERIFY_ARE_EQUAL(300, intIntMap.at(3));
        VERIFY_ARE_EQUAL(500, intIntMap.at(5));

        int unused{};
        VERIFY_THROWS(unused = intIntMap.at(0), std::runtime_error);
        VERIFY_THROWS(unused = intIntMap.at(4), std::runtime_error);
        VERIFY_THROWS(unused = intIntMap.at(7), std::runtime_error);

#pragma warning(push)
#pragma warning(disable : 26446) // Suppress bounds.4 check for subscript operator.
        VERIFY_ARE_EQUAL(500, intIntMap[5]);
        VERIFY_THROWS(unused = intIntMap[4], std::runtime_error);
        VERIFY_THROWS(unused = intIntMap[7], std::runtime_error);
#pragma warning(pop)
    }
};
