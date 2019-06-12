// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "WexTestClass.h"
#include "..\..\inc\consoletaeftemplates.hpp"

#include "..\inc\utils.hpp"

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;

using namespace Microsoft::Console::Utils;

class UtilsTests
{
    TEST_CLASS(UtilsTests);

    TEST_METHOD(TestClampToShortMax)
    {
        const short min = 1;

        // Test outside the lower end of the range
        const short minExpected = min;
        auto minActual = ClampToShortMax(0, min);
        VERIFY_ARE_EQUAL(minExpected, minActual);

        // Test negative numbers
        const short negativeExpected = min;
        auto negativeActual = ClampToShortMax(-1, min);
        VERIFY_ARE_EQUAL(negativeExpected, negativeActual);

        // Test outside the upper end of the range
        const short maxExpected = SHRT_MAX;
        auto maxActual = ClampToShortMax(50000, min);
        VERIFY_ARE_EQUAL(maxExpected, maxActual);

        // Test within the range
        const short withinRangeExpected = 100;
        auto withinRangeActual = ClampToShortMax(withinRangeExpected, min);
        VERIFY_ARE_EQUAL(withinRangeExpected, withinRangeActual);
    }
};
