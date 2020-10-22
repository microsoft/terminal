// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "../TerminalApp/TerminalSettings.h"
#include "../TerminalApp/CommandPalette.h"

using namespace Microsoft::Console;
using namespace WEX::Logging;
using namespace WEX::TestExecution;
using namespace WEX::Common;
using namespace winrt::Microsoft::Terminal::Settings::Model;
using namespace winrt::Microsoft::Terminal::TerminalControl;

namespace TerminalAppLocalTests
{
    class FilteredCommandTests
    {
        BEGIN_TEST_CLASS(FilteredCommandTests)
            TEST_CLASS_PROPERTY(L"RunAs", L"UAP")
            TEST_CLASS_PROPERTY(L"UAP:AppXManifest", L"TestHostAppXManifest.xml")
        END_TEST_CLASS()

        TEST_METHOD(VerifyHighlighting);
    };

    void FilteredCommandTests::VerifyHighlighting()
    {
        const std::string settingsJson{ R"(
        {
            "defaultProfile": "{6239a42c-0000-49a3-80bd-e8fdd045185c}",
            "profiles": [
                {
                    "name": "profile0",
                    "guid": "{6239a42c-0000-49a3-80bd-e8fdd045185c}",
                    "historySize": 1,
                    "commandline": "cmd.exe"
                }                
            ],
            "keybindings": [
                { "keys": ["ctrl+a"], "command": { "action": "splitPane", "split": "vertical" }, "name": "AAAAAABBBBBBCCC" }
            ]
        })" };

        CascadiaSettings settings{ til::u8u16(settingsJson) };
        const auto commands = settings.GlobalSettings().Commands();
        VERIFY_ARE_EQUAL(1u, commands.Size());

        const auto command = commands.Lookup(L"AAAAAABBBBBBCCC");
        VERIFY_IS_NOT_NULL(command);
        VERIFY_ARE_EQUAL(command.Name(), L"AAAAAABBBBBBCCC");
        {
            Log::Comment(L"Testing command name segmentation with no filter");
            const auto filteredCommand = winrt::make_self<winrt::TerminalApp::implementation::FilteredCommand>(command);
            auto segments = filteredCommand->_computeHighlightedName().Segments();
            VERIFY_ARE_EQUAL(segments.Size(), 1u);
            VERIFY_ARE_EQUAL(segments.GetAt(0).TextSegment(), L"AAAAAABBBBBBCCC");
            VERIFY_IS_FALSE(segments.GetAt(0).IsHighlighted());
        }
        {
            Log::Comment(L"Testing command name segmentation with empty filter");
            const auto filteredCommand = winrt::make_self<winrt::TerminalApp::implementation::FilteredCommand>(command);
            filteredCommand->_Filter = L"";
            auto segments = filteredCommand->_computeHighlightedName().Segments();
            VERIFY_ARE_EQUAL(segments.Size(), 1u);
            VERIFY_ARE_EQUAL(segments.GetAt(0).TextSegment(), L"AAAAAABBBBBBCCC");
            VERIFY_IS_FALSE(segments.GetAt(0).IsHighlighted());
        }
        {
            Log::Comment(L"Testing command name segmentation with filter equals to the string");
            const auto filteredCommand = winrt::make_self<winrt::TerminalApp::implementation::FilteredCommand>(command);
            filteredCommand->_Filter = L"AAAAAABBBBBBCCC";
            auto segments = filteredCommand->_computeHighlightedName().Segments();
            VERIFY_ARE_EQUAL(segments.Size(), 1u);
            VERIFY_ARE_EQUAL(segments.GetAt(0).TextSegment(), L"AAAAAABBBBBBCCC");
            VERIFY_IS_TRUE(segments.GetAt(0).IsHighlighted());
        }
        {
            Log::Comment(L"Testing command name segmentation with filter with first character matching");
            const auto filteredCommand = winrt::make_self<winrt::TerminalApp::implementation::FilteredCommand>(command);
            filteredCommand->_Filter = L"A";
            auto segments = filteredCommand->_computeHighlightedName().Segments();
            VERIFY_ARE_EQUAL(segments.Size(), 2u);
            VERIFY_ARE_EQUAL(segments.GetAt(0).TextSegment(), L"A");
            VERIFY_IS_TRUE(segments.GetAt(0).IsHighlighted());
            VERIFY_ARE_EQUAL(segments.GetAt(1).TextSegment(), L"AAAAABBBBBBCCC");
            VERIFY_IS_FALSE(segments.GetAt(1).IsHighlighted());
        }
        {
            Log::Comment(L"Testing command name segmentation with filter with other case");
            const auto filteredCommand = winrt::make_self<winrt::TerminalApp::implementation::FilteredCommand>(command);
            filteredCommand->_Filter = L"a";
            auto segments = filteredCommand->_computeHighlightedName().Segments();
            VERIFY_ARE_EQUAL(segments.Size(), 2u);
            VERIFY_ARE_EQUAL(segments.GetAt(0).TextSegment(), L"A");
            VERIFY_IS_TRUE(segments.GetAt(0).IsHighlighted());
            VERIFY_ARE_EQUAL(segments.GetAt(1).TextSegment(), L"AAAAABBBBBBCCC");
            VERIFY_IS_FALSE(segments.GetAt(1).IsHighlighted());
        }
        {
            Log::Comment(L"Testing command name segmentation with filter matching several characters");
            const auto filteredCommand = winrt::make_self<winrt::TerminalApp::implementation::FilteredCommand>(command);
            filteredCommand->_Filter = L"ab";
            auto segments = filteredCommand->_computeHighlightedName().Segments();
            VERIFY_ARE_EQUAL(segments.Size(), 4u);
            VERIFY_ARE_EQUAL(segments.GetAt(0).TextSegment(), L"A");
            VERIFY_IS_TRUE(segments.GetAt(0).IsHighlighted());
            VERIFY_ARE_EQUAL(segments.GetAt(1).TextSegment(), L"AAAAA");
            VERIFY_IS_FALSE(segments.GetAt(1).IsHighlighted());
            VERIFY_ARE_EQUAL(segments.GetAt(2).TextSegment(), L"B");
            VERIFY_IS_TRUE(segments.GetAt(2).IsHighlighted());
            VERIFY_ARE_EQUAL(segments.GetAt(3).TextSegment(), L"BBBBBCCC");
            VERIFY_IS_FALSE(segments.GetAt(3).IsHighlighted());
        }
        {
            Log::Comment(L"Testing command name segmentation with non matching filter");
            const auto filteredCommand = winrt::make_self<winrt::TerminalApp::implementation::FilteredCommand>(command);
            filteredCommand->_Filter = L"abcd";
            VERIFY_THROWS(filteredCommand->_computeHighlightedName(), winrt::hresult_invalid_argument);
        }
    }
}
