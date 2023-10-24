// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "WexTestClass.h"

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;

class CoalesceTests
{
    TEST_CLASS(CoalesceTests);

    TEST_METHOD(CoalesceFirstValue);
    TEST_METHOD(CoalesceMiddleValue);
    TEST_METHOD(CoalesceDefaultValue);

    TEST_METHOD(CoalesceOrNotFirstValue);
    TEST_METHOD(CoalesceOrNotMiddleValue);
    TEST_METHOD(CoalesceOrNotDefaultValue);
    TEST_METHOD(CoalesceOrNotDefaultIsNullopt);
};

void CoalesceTests::CoalesceFirstValue()
{
    auto result = til::coalesce_value(std::optional<int>(1),
                                      std::optional<int>(2),
                                      std::optional<int>(3),
                                      4);
    VERIFY_ARE_EQUAL(1, result);
}
void CoalesceTests::CoalesceMiddleValue()
{
    auto result = til::coalesce_value(std::optional<int>(std::nullopt),
                                      std::optional<int>(2),
                                      std::optional<int>(3),
                                      4);
    VERIFY_ARE_EQUAL(2, result);
}
void CoalesceTests::CoalesceDefaultValue()
{
    auto result = til::coalesce_value(std::optional<int>(std::nullopt),
                                      std::optional<int>(std::nullopt),
                                      std::optional<int>(std::nullopt),
                                      4);
    VERIFY_ARE_EQUAL(4, result);
}

void CoalesceTests::CoalesceOrNotFirstValue()
{
    auto result = til::coalesce(std::optional<int>(1),
                                std::optional<int>(2),
                                std::optional<int>(3),
                                std::optional<int>(4));
    VERIFY_IS_TRUE(result.has_value());
    VERIFY_ARE_EQUAL(1, result.value());
}
void CoalesceTests::CoalesceOrNotMiddleValue()
{
    auto result = til::coalesce(std::optional<int>(std::nullopt),
                                std::optional<int>(2),
                                std::optional<int>(3),
                                std::optional<int>(4));
    VERIFY_IS_TRUE(result.has_value());
    VERIFY_ARE_EQUAL(2, result.value());
}
void CoalesceTests::CoalesceOrNotDefaultValue()
{
    auto result = til::coalesce(std::optional<int>(std::nullopt),
                                std::optional<int>(std::nullopt),
                                std::optional<int>(std::nullopt),
                                std::optional<int>(4));
    VERIFY_IS_TRUE(result.has_value());
    VERIFY_ARE_EQUAL(4, result.value());
}
void CoalesceTests::CoalesceOrNotDefaultIsNullopt()
{
    auto result = til::coalesce(std::optional<int>(std::nullopt),
                                std::optional<int>(std::nullopt),
                                std::optional<int>(std::nullopt),
                                std::optional<int>(std::nullopt));
    VERIFY_IS_FALSE(result.has_value());
}
