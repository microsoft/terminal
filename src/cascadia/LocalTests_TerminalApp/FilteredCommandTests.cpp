// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "../TerminalApp/CommandLinePaletteItem.h"
#include "../TerminalApp/CommandPalette.h"
#include "CppWinrtTailored.h"

using namespace Microsoft::Console;
using namespace WEX::Logging;
using namespace WEX::TestExecution;
using namespace WEX::Common;
using namespace winrt::Microsoft::Terminal::Settings::Model;
using namespace winrt::Microsoft::Terminal::Control;

namespace TerminalAppLocalTests
{
    class FilteredCommandTests
    {
        BEGIN_TEST_CLASS(FilteredCommandTests)
            TEST_CLASS_PROPERTY(L"RunAs", L"UAP")
            TEST_CLASS_PROPERTY(L"UAP:AppXManifest", L"TestHostAppXManifest.xml")
        END_TEST_CLASS()

        TEST_METHOD(VerifyHighlighting);
        TEST_METHOD(VerifyWeight);
        TEST_METHOD(VerifyCompare);
        TEST_METHOD(VerifyCompareIgnoreCase);
    };

    void FilteredCommandTests::VerifyHighlighting()
    {
        auto result = RunOnUIThread([]() {
            const auto paletteItem{ winrt::make<winrt::TerminalApp::implementation::CommandLinePaletteItem>(L"AAAAAABBBBBBCCC") };
            {
                Log::Comment(L"Testing command name segmentation with no filter");
                const auto filteredCommand = winrt::make_self<winrt::TerminalApp::implementation::FilteredCommand>(paletteItem);
                auto segments = filteredCommand->_computeHighlightedName().Segments();
                VERIFY_ARE_EQUAL(segments.Size(), 1u);
                VERIFY_ARE_EQUAL(segments.GetAt(0).TextSegment(), L"AAAAAABBBBBBCCC");
                VERIFY_IS_FALSE(segments.GetAt(0).IsHighlighted());
            }
            {
                Log::Comment(L"Testing command name segmentation with empty filter");
                const auto filteredCommand = winrt::make_self<winrt::TerminalApp::implementation::FilteredCommand>(paletteItem);
                filteredCommand->_Filter = L"";
                auto segments = filteredCommand->_computeHighlightedName().Segments();
                VERIFY_ARE_EQUAL(segments.Size(), 1u);
                VERIFY_ARE_EQUAL(segments.GetAt(0).TextSegment(), L"AAAAAABBBBBBCCC");
                VERIFY_IS_FALSE(segments.GetAt(0).IsHighlighted());
            }
            {
                Log::Comment(L"Testing command name segmentation with filter equals to the string");
                const auto filteredCommand = winrt::make_self<winrt::TerminalApp::implementation::FilteredCommand>(paletteItem);
                filteredCommand->_Filter = L"AAAAAABBBBBBCCC";
                auto segments = filteredCommand->_computeHighlightedName().Segments();
                VERIFY_ARE_EQUAL(segments.Size(), 1u);
                VERIFY_ARE_EQUAL(segments.GetAt(0).TextSegment(), L"AAAAAABBBBBBCCC");
                VERIFY_IS_TRUE(segments.GetAt(0).IsHighlighted());
            }
            {
                Log::Comment(L"Testing command name segmentation with filter with first character matching");
                const auto filteredCommand = winrt::make_self<winrt::TerminalApp::implementation::FilteredCommand>(paletteItem);
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
                const auto filteredCommand = winrt::make_self<winrt::TerminalApp::implementation::FilteredCommand>(paletteItem);
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
                const auto filteredCommand = winrt::make_self<winrt::TerminalApp::implementation::FilteredCommand>(paletteItem);
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
                const auto filteredCommand = winrt::make_self<winrt::TerminalApp::implementation::FilteredCommand>(paletteItem);
                filteredCommand->_Filter = L"abcd";
                auto segments = filteredCommand->_computeHighlightedName().Segments();
                VERIFY_ARE_EQUAL(segments.Size(), 1u);
                VERIFY_ARE_EQUAL(segments.GetAt(0).TextSegment(), L"AAAAAABBBBBBCCC");
                VERIFY_IS_FALSE(segments.GetAt(0).IsHighlighted());
            }
        });

        VERIFY_SUCCEEDED(result);
    }

    void FilteredCommandTests::VerifyWeight()
    {
        auto result = RunOnUIThread([]() {
            const auto paletteItem{ winrt::make<winrt::TerminalApp::implementation::CommandLinePaletteItem>(L"AAAAAABBBBBBCCC") };
            {
                Log::Comment(L"Testing weight of command with no filter");
                const auto filteredCommand = winrt::make_self<winrt::TerminalApp::implementation::FilteredCommand>(paletteItem);
                filteredCommand->_HighlightedName = filteredCommand->_computeHighlightedName();
                auto weight = filteredCommand->_computeWeight();
                VERIFY_ARE_EQUAL(weight, 0);
            }
            {
                Log::Comment(L"Testing weight of command with empty filter");
                const auto filteredCommand = winrt::make_self<winrt::TerminalApp::implementation::FilteredCommand>(paletteItem);
                filteredCommand->_Filter = L"";
                filteredCommand->_HighlightedName = filteredCommand->_computeHighlightedName();
                auto weight = filteredCommand->_computeWeight();
                VERIFY_ARE_EQUAL(weight, 0);
            }
            {
                Log::Comment(L"Testing weight of command with filter equals to the string");
                const auto filteredCommand = winrt::make_self<winrt::TerminalApp::implementation::FilteredCommand>(paletteItem);
                filteredCommand->_Filter = L"AAAAAABBBBBBCCC";
                filteredCommand->_HighlightedName = filteredCommand->_computeHighlightedName();
                auto weight = filteredCommand->_computeWeight();
                VERIFY_ARE_EQUAL(weight, 30); // 1 point for the first char and 2 points for the 14 consequent ones + 1 point for the beginning of the word
            }
            {
                Log::Comment(L"Testing weight of command with filter with first character matching");
                const auto filteredCommand = winrt::make_self<winrt::TerminalApp::implementation::FilteredCommand>(paletteItem);
                filteredCommand->_Filter = L"A";
                filteredCommand->_HighlightedName = filteredCommand->_computeHighlightedName();
                auto weight = filteredCommand->_computeWeight();
                VERIFY_ARE_EQUAL(weight, 2); // 1 point for the first char match + 1 point for the beginning of the word
            }
            {
                Log::Comment(L"Testing weight of command with filter with other case");
                const auto filteredCommand = winrt::make_self<winrt::TerminalApp::implementation::FilteredCommand>(paletteItem);
                filteredCommand->_Filter = L"a";
                filteredCommand->_HighlightedName = filteredCommand->_computeHighlightedName();
                auto weight = filteredCommand->_computeWeight();
                VERIFY_ARE_EQUAL(weight, 2); // 1 point for the first char match + 1 point for the beginning of the word
            }
            {
                Log::Comment(L"Testing weight of command with filter matching several characters");
                const auto filteredCommand = winrt::make_self<winrt::TerminalApp::implementation::FilteredCommand>(paletteItem);
                filteredCommand->_Filter = L"ab";
                filteredCommand->_HighlightedName = filteredCommand->_computeHighlightedName();
                auto weight = filteredCommand->_computeWeight();
                VERIFY_ARE_EQUAL(weight, 3); // 1 point for the first char match + 1 point for the beginning of the word + 1 point for the match of "b"
            }
        });

        VERIFY_SUCCEEDED(result);
    }

    void FilteredCommandTests::VerifyCompare()
    {
        auto result = RunOnUIThread([]() {
            const auto paletteItem{ winrt::make<winrt::TerminalApp::implementation::CommandLinePaletteItem>(L"AAAAAABBBBBBCCC") };
            const auto paletteItem2{ winrt::make<winrt::TerminalApp::implementation::CommandLinePaletteItem>(L"BBBBBCCC") };
            {
                Log::Comment(L"Testing comparison of commands with no filter");
                const auto filteredCommand = winrt::make_self<winrt::TerminalApp::implementation::FilteredCommand>(paletteItem);
                const auto filteredCommand2 = winrt::make_self<winrt::TerminalApp::implementation::FilteredCommand>(paletteItem2);

                VERIFY_ARE_EQUAL(filteredCommand->Weight(), filteredCommand2->Weight());
                VERIFY_IS_TRUE(winrt::TerminalApp::implementation::FilteredCommand::Compare(*filteredCommand, *filteredCommand2));
            }
            {
                Log::Comment(L"Testing comparison of commands with empty filter");
                const auto filteredCommand = winrt::make_self<winrt::TerminalApp::implementation::FilteredCommand>(paletteItem);
                filteredCommand->_Filter = L"";
                filteredCommand->_HighlightedName = filteredCommand->_computeHighlightedName();
                filteredCommand->_Weight = filteredCommand->_computeWeight();

                const auto filteredCommand2 = winrt::make_self<winrt::TerminalApp::implementation::FilteredCommand>(paletteItem2);
                filteredCommand2->_Filter = L"";
                filteredCommand2->_HighlightedName = filteredCommand2->_computeHighlightedName();
                filteredCommand2->_Weight = filteredCommand2->_computeWeight();

                VERIFY_ARE_EQUAL(filteredCommand->Weight(), filteredCommand2->Weight());
                VERIFY_IS_TRUE(winrt::TerminalApp::implementation::FilteredCommand::Compare(*filteredCommand, *filteredCommand2));
            }
            {
                Log::Comment(L"Testing comparison of commands with different weights");
                const auto filteredCommand = winrt::make_self<winrt::TerminalApp::implementation::FilteredCommand>(paletteItem);
                filteredCommand->_Filter = L"B";
                filteredCommand->_HighlightedName = filteredCommand->_computeHighlightedName();
                filteredCommand->_Weight = filteredCommand->_computeWeight();

                const auto filteredCommand2 = winrt::make_self<winrt::TerminalApp::implementation::FilteredCommand>(paletteItem2);
                filteredCommand2->_Filter = L"B";
                filteredCommand2->_HighlightedName = filteredCommand2->_computeHighlightedName();
                filteredCommand2->_Weight = filteredCommand2->_computeWeight();

                VERIFY_IS_TRUE(filteredCommand->Weight() < filteredCommand2->Weight()); // Second command gets more points due to the beginning of the word
                VERIFY_IS_FALSE(winrt::TerminalApp::implementation::FilteredCommand::Compare(*filteredCommand, *filteredCommand2));
            }
        });

        VERIFY_SUCCEEDED(result);
    }

    void FilteredCommandTests::VerifyCompareIgnoreCase()
    {
        auto result = RunOnUIThread([]() {
            const auto paletteItem{ winrt::make<winrt::TerminalApp::implementation::CommandLinePaletteItem>(L"a") };
            const auto paletteItem2{ winrt::make<winrt::TerminalApp::implementation::CommandLinePaletteItem>(L"B") };
            {
                const auto filteredCommand = winrt::make_self<winrt::TerminalApp::implementation::FilteredCommand>(paletteItem);
                const auto filteredCommand2 = winrt::make_self<winrt::TerminalApp::implementation::FilteredCommand>(paletteItem2);

                VERIFY_ARE_EQUAL(filteredCommand->Weight(), filteredCommand2->Weight());
                VERIFY_IS_TRUE(winrt::TerminalApp::implementation::FilteredCommand::Compare(*filteredCommand, *filteredCommand2));
            }
        });

        VERIFY_SUCCEEDED(result);
    }
}
