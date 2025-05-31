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
            const auto filteredCommand = winrt::make_self<winrt::TerminalApp::implementation::FilteredCommand>(paletteItem);

            {
                Log::Comment(L"Testing command name segmentation with no filter");
                auto segments = filteredCommand->HighlightedName().Segments();
                VERIFY_ARE_EQUAL(segments.Size(), 1u);
                VERIFY_ARE_EQUAL(segments.GetAt(0).TextSegment(), L"AAAAAABBBBBBCCC");
                VERIFY_IS_FALSE(segments.GetAt(0).IsHighlighted());
            }
            {
                Log::Comment(L"Testing command name segmentation with empty filter");
                filteredCommand->UpdateFilter(std::make_shared<fzf::matcher::Pattern>(fzf::matcher::ParsePattern(L"")));
                auto segments = filteredCommand->HighlightedName().Segments();
                VERIFY_ARE_EQUAL(segments.Size(), 1u);
                VERIFY_ARE_EQUAL(segments.GetAt(0).TextSegment(), L"AAAAAABBBBBBCCC");
                VERIFY_IS_FALSE(segments.GetAt(0).IsHighlighted());
            }
            {
                Log::Comment(L"Testing command name segmentation with filter equal to the string");
                filteredCommand->UpdateFilter(std::make_shared<fzf::matcher::Pattern>(fzf::matcher::ParsePattern(L"AAAAAABBBBBBCCC")));
                auto segments = filteredCommand->HighlightedName().Segments();
                VERIFY_ARE_EQUAL(segments.Size(), 1u);
                VERIFY_ARE_EQUAL(segments.GetAt(0).TextSegment(), L"AAAAAABBBBBBCCC");
                VERIFY_IS_TRUE(segments.GetAt(0).IsHighlighted());
            }
            {
                Log::Comment(L"Testing command name segmentation with filter with first character matching");
                filteredCommand->UpdateFilter(std::make_shared<fzf::matcher::Pattern>(fzf::matcher::ParsePattern(L"A")));
                auto segments = filteredCommand->HighlightedName().Segments();
                VERIFY_ARE_EQUAL(segments.Size(), 2u);
                VERIFY_ARE_EQUAL(segments.GetAt(0).TextSegment(), L"A");
                VERIFY_IS_TRUE(segments.GetAt(0).IsHighlighted());
                VERIFY_ARE_EQUAL(segments.GetAt(1).TextSegment(), L"AAAAABBBBBBCCC");
                VERIFY_IS_FALSE(segments.GetAt(1).IsHighlighted());
            }
            {
                Log::Comment(L"Testing command name segmentation with filter with other case");
                filteredCommand->UpdateFilter(std::make_shared<fzf::matcher::Pattern>(fzf::matcher::ParsePattern(L"a")));
                auto segments = filteredCommand->HighlightedName().Segments();
                VERIFY_ARE_EQUAL(segments.Size(), 2u);
                VERIFY_ARE_EQUAL(segments.GetAt(0).TextSegment(), L"A");
                VERIFY_IS_TRUE(segments.GetAt(0).IsHighlighted());
                VERIFY_ARE_EQUAL(segments.GetAt(1).TextSegment(), L"AAAAABBBBBBCCC");
                VERIFY_IS_FALSE(segments.GetAt(1).IsHighlighted());
            }
            {
                Log::Comment(L"Testing command name segmentation with filter matching several characters");
                filteredCommand->UpdateFilter(std::make_shared<fzf::matcher::Pattern>(fzf::matcher::ParsePattern(L"ab")));
                auto segments = filteredCommand->HighlightedName().Segments();
                VERIFY_ARE_EQUAL(segments.Size(), 3u);
                VERIFY_ARE_EQUAL(segments.GetAt(0).TextSegment(), L"AAAAA");
                VERIFY_IS_FALSE(segments.GetAt(0).IsHighlighted());
                VERIFY_ARE_EQUAL(segments.GetAt(1).TextSegment(), L"AB");
                VERIFY_IS_TRUE(segments.GetAt(1).IsHighlighted());
                VERIFY_ARE_EQUAL(segments.GetAt(2).TextSegment(), L"BBBBBCCC");
                VERIFY_IS_FALSE(segments.GetAt(2).IsHighlighted());
            }
            {
                Log::Comment(L"Testing command name segmentation with non matching filter");
                filteredCommand->UpdateFilter(std::make_shared<fzf::matcher::Pattern>(fzf::matcher::ParsePattern(L"abcd")));
                auto segments = filteredCommand->HighlightedName().Segments();
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
            const auto filteredCommand = winrt::make_self<winrt::TerminalApp::implementation::FilteredCommand>(paletteItem);

            const auto weigh = [&](const wchar_t* str) {
                std::shared_ptr<fzf::matcher::Pattern> pattern;
                if (str)
                {
                    pattern = std::make_shared<fzf::matcher::Pattern>(fzf::matcher::ParsePattern(str));
                }
                filteredCommand->UpdateFilter(std::move(pattern));
                return filteredCommand->Weight();
            };

            const auto null = weigh(nullptr);
            const auto empty = weigh(L"");
            const auto full = weigh(L"AAAAAABBBBBBCCC");
            const auto firstChar = weigh(L"A");
            const auto otherCase = weigh(L"a");
            const auto severalChars = weigh(L"ab");

            VERIFY_ARE_EQUAL(null, 0);
            VERIFY_ARE_EQUAL(empty, 0);
            VERIFY_IS_GREATER_THAN(full, 100);

            VERIFY_IS_GREATER_THAN(firstChar, 0);
            VERIFY_IS_LESS_THAN(firstChar, full);

            VERIFY_IS_GREATER_THAN(otherCase, 0);
            VERIFY_IS_LESS_THAN(otherCase, full);

            VERIFY_IS_GREATER_THAN(severalChars, otherCase);
            VERIFY_IS_LESS_THAN(severalChars, full);
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
                filteredCommand->UpdateFilter(std::make_shared<fzf::matcher::Pattern>(fzf::matcher::ParsePattern(L"")));

                const auto filteredCommand2 = winrt::make_self<winrt::TerminalApp::implementation::FilteredCommand>(paletteItem2);
                filteredCommand2->UpdateFilter(std::make_shared<fzf::matcher::Pattern>(fzf::matcher::ParsePattern(L"")));

                VERIFY_ARE_EQUAL(filteredCommand->Weight(), filteredCommand2->Weight());
                VERIFY_IS_TRUE(winrt::TerminalApp::implementation::FilteredCommand::Compare(*filteredCommand, *filteredCommand2));
            }
            {
                Log::Comment(L"Testing comparison of commands with different weights");
                const auto filteredCommand = winrt::make_self<winrt::TerminalApp::implementation::FilteredCommand>(paletteItem);
                filteredCommand->UpdateFilter(std::make_shared<fzf::matcher::Pattern>(fzf::matcher::ParsePattern(L"B")));

                const auto filteredCommand2 = winrt::make_self<winrt::TerminalApp::implementation::FilteredCommand>(paletteItem2);
                filteredCommand2->UpdateFilter(std::make_shared<fzf::matcher::Pattern>(fzf::matcher::ParsePattern(L"B")));

                VERIFY_IS_LESS_THAN(filteredCommand->Weight(), filteredCommand2->Weight()); // Second command gets more points due to the beginning of the word
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
