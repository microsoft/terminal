// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "../TerminalApp/CommandPalette.h"
#include "../TerminalApp/BasePaletteItem.h"
#include "CppWinrtTailored.h"

using namespace Microsoft::Console;
using namespace WEX::Logging;
using namespace WEX::TestExecution;
using namespace WEX::Common;
using namespace winrt::Microsoft::Terminal::Settings::Model;
using namespace winrt::Microsoft::Terminal::Control;

namespace TerminalAppLocalTests
{
    struct StringPaletteItem : winrt::implements<StringPaletteItem, winrt::TerminalApp::IPaletteItem, winrt::Windows::UI::Xaml::Data::INotifyPropertyChanged>, winrt::TerminalApp::implementation::BasePaletteItem<StringPaletteItem, winrt::TerminalApp::PaletteItemType::CommandLine>
    {
        StringPaletteItem(std::wstring_view value) :
            _value{ value } {}

        winrt::hstring Name() { return _value; }
        winrt::hstring Subtitle() { return {}; }
        winrt::hstring KeyChordText() { return {}; }
        winrt::hstring Icon() { return {}; }

    private:
        winrt::hstring _value;
    };

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

    static void _verifySegment(auto&& segments, uint32_t index, uint64_t start, uint64_t end)
    {
        const auto& segment{ segments.GetAt(index) };
        VERIFY_ARE_EQUAL(segment.Start, start, NoThrowString().Format(L"segment %zu", index));
        VERIFY_ARE_EQUAL(segment.End, end, NoThrowString().Format(L"segment %zu", index));
    }

    void FilteredCommandTests::VerifyHighlighting()
    {
        auto result = RunOnUIThread([]() {
            const WEX::TestExecution::DisableVerifyExceptions disableExceptionsScope;

            const auto paletteItem{ winrt::make<StringPaletteItem>(L"AAAAAABBBBBBCCC") };
            const auto filteredCommand = winrt::make_self<winrt::TerminalApp::implementation::FilteredCommand>(paletteItem);

            {
                Log::Comment(L"Testing command name segmentation with no filter");
                const auto segments = filteredCommand->NameHighlights();

                VERIFY_IS_NULL(segments); // No matches = no segments
            }
            {
                Log::Comment(L"Testing command name segmentation with empty filter");
                filteredCommand->UpdateFilter(std::make_shared<fzf::matcher::Pattern>(fzf::matcher::ParsePattern(L"")));
                const auto segments = filteredCommand->NameHighlights();

                VERIFY_IS_NULL(segments); // No matches = no segments
            }
            {
                Log::Comment(L"Testing command name segmentation with filter equal to the string");
                filteredCommand->UpdateFilter(std::make_shared<fzf::matcher::Pattern>(fzf::matcher::ParsePattern(L"AAAAAABBBBBBCCC")));
                const auto segments = filteredCommand->NameHighlights();

                VERIFY_ARE_EQUAL(segments.Size(), 1u);
                _verifySegment(segments, 0, 0, 14); // one segment for the entire string
            }
            {
                Log::Comment(L"Testing command name segmentation with filter with first character matching");
                filteredCommand->UpdateFilter(std::make_shared<fzf::matcher::Pattern>(fzf::matcher::ParsePattern(L"A")));
                const auto segments = filteredCommand->NameHighlights();

                VERIFY_ARE_EQUAL(segments.Size(), 1u); // only one bold segment
                _verifySegment(segments, 0, 0, 0); // it only covers the first character
            }
            {
                Log::Comment(L"Testing command name segmentation with filter with other case");
                filteredCommand->UpdateFilter(std::make_shared<fzf::matcher::Pattern>(fzf::matcher::ParsePattern(L"a")));
                const auto segments = filteredCommand->NameHighlights();

                VERIFY_ARE_EQUAL(segments.Size(), 1u); // only one bold segment
                _verifySegment(segments, 0, 0, 0); // it only covers the first character
            }
            {
                Log::Comment(L"Testing command name segmentation with filter matching several characters");
                filteredCommand->UpdateFilter(std::make_shared<fzf::matcher::Pattern>(fzf::matcher::ParsePattern(L"ab")));
                const auto segments = filteredCommand->NameHighlights();

                VERIFY_ARE_EQUAL(segments.Size(), 1u); // one bold segment
                _verifySegment(segments, 0, 5, 6); // middle 'ab'
            }
            {
                Log::Comment(L"Testing command name segmentation with filter matching several regions");
                filteredCommand->UpdateFilter(std::make_shared<fzf::matcher::Pattern>(fzf::matcher::ParsePattern(L"abcc")));
                const auto segments = filteredCommand->NameHighlights();

                VERIFY_ARE_EQUAL(segments.Size(), 2u); // two bold segments
                _verifySegment(segments, 0, 5, 6); // middle 'ab'
                _verifySegment(segments, 1, 12, 13); // start of 'cc'
            }
            {
                Log::Comment(L"Testing command name segmentation with non matching filter");
                filteredCommand->UpdateFilter(std::make_shared<fzf::matcher::Pattern>(fzf::matcher::ParsePattern(L"abcd")));
                const auto segments = filteredCommand->NameHighlights();

                VERIFY_IS_NULL(segments); // No matches = no segments
            }
        });

        VERIFY_SUCCEEDED(result);
    }

    void FilteredCommandTests::VerifyWeight()
    {
        auto result = RunOnUIThread([]() {
            const auto paletteItem{ winrt::make<StringPaletteItem>(L"AAAAAABBBBBBCCC") };
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
            const auto paletteItem{ winrt::make<StringPaletteItem>(L"AAAAAABBBBBBCCC") };
            const auto paletteItem2{ winrt::make<StringPaletteItem>(L"BBBBBCCC") };
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
            const auto paletteItem{ winrt::make<StringPaletteItem>(L"a") };
            const auto paletteItem2{ winrt::make<StringPaletteItem>(L"B") };
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
