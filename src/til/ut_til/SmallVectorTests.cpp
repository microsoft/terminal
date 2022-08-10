// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include <til/small_vector.h>

using namespace std::literals;
using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;

class SmallVectorTests
{
    TEST_CLASS(SmallVectorTests)

    TEST_METHOD(Simple)
    {
        til::small_vector<int, 16> v;
        VERIFY_IS_TRUE(v.empty());
    }
};
