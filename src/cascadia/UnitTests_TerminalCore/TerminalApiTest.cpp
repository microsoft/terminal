// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include <WexTestClass.h>

#include "../cascadia/TerminalCore/Terminal.hpp"
#include "MockTermSettings.h"
#include "../renderer/inc/DummyRenderTarget.hpp"
#include "consoletaeftemplates.hpp"

using namespace winrt::Microsoft::Terminal::Settings;
using namespace Microsoft::Terminal::Core;

namespace TerminalCoreUnitTests
{
#define WCS(x) WCSHELPER(x)
#define WCSHELPER(x) L#x

    class TerminalApiTest
    {
        TEST_CLASS(TerminalApiTest);

        TEST_METHOD(SetColorTableEntry)
        {
            Terminal term;
            DummyRenderTarget emptyRT;
            term.Create({ 100, 100 }, 0, emptyRT);

            auto settings = winrt::make<MockTermSettings>(100, 100, 100);
            term.UpdateSettings(settings);

            VERIFY_IS_TRUE(term.SetColorTableEntry(0, 100));
            VERIFY_IS_TRUE(term.SetColorTableEntry(128, 100));
            VERIFY_IS_TRUE(term.SetColorTableEntry(255, 100));

            VERIFY_IS_FALSE(term.SetColorTableEntry(256, 100));
            VERIFY_IS_FALSE(term.SetColorTableEntry(512, 100));
        }
    };
}
