// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "til/operators.h"

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;

class VisualizeControlCodesTests
{
    TEST_CLASS(VisualizeControlCodesTests);

    TEST_METHOD(EscapeSequence)
    {
        const std::wstring_view expected{ L"\u241b[A\u2423\u241b[B" };

        const std::wstring_view input{ L"\u001b[A \u001b[B" };
        VERIFY_ARE_EQUAL(expected, til::visualize_control_codes(input));
    }
};
