// Licensed under the MIT license.

#include "precomp.h"
#include "../TerminalSettingsEditor/CodePageUtils.h"

using namespace WEX::TestExecution;
using namespace winrt::Microsoft::Terminal::Settings::Editor;

namespace TerminalAppUnitTests
{
    class CodePageUtilsTests
    {
        TEST_CLASS(CodePageUtilsTests);

        TEST_METHOD(UnsetTextMatchesProfileLayer);
        TEST_METHOD(FilterMatchesOnlyDisplayedText);
        TEST_METHOD(ClearRequiresEmptyUserInput);
        TEST_METHOD(SingleResultCommitRequiresNoChosenSuggestion);
    };

    void CodePageUtilsTests::UnsetTextMatchesProfileLayer()
    {
        static constexpr std::wstring_view defaultText{ L"(Default)" };
        static constexpr std::wstring_view inheritedText{ L"(Inherited)" };

        VERIFY_IS_TRUE(CodePageUtils::UnsetTextForProfile(true, defaultText, inheritedText) == defaultText);
        VERIFY_IS_TRUE(CodePageUtils::UnsetTextForProfile(false, defaultText, inheritedText) == inheritedText);
    }

    void CodePageUtilsTests::FilterMatchesOnlyDisplayedText()
    {
        static constexpr std::wstring_view displayName{ L"65001 (Unicode UTF-8)" };
        VERIFY_IS_TRUE(CodePageUtils::MatchesFilter(displayName, L""));
        VERIFY_IS_TRUE(CodePageUtils::MatchesFilter(displayName, L"65001"));
        VERIFY_IS_TRUE(CodePageUtils::MatchesFilter(displayName, L"utf-8"));
        VERIFY_IS_TRUE(CodePageUtils::MatchesFilter(displayName, L"UNICODE"));
        VERIFY_IS_FALSE(CodePageUtils::MatchesFilter(displayName, L"utf8"));
        VERIFY_IS_FALSE(CodePageUtils::MatchesFilter(displayName, L"big5"));
    }

    void CodePageUtilsTests::ClearRequiresEmptyUserInput()
    {
        VERIFY_IS_TRUE(CodePageUtils::ShouldClearFromTextChange(true, true));
        VERIFY_IS_FALSE(CodePageUtils::ShouldClearFromTextChange(true, false));
        VERIFY_IS_FALSE(CodePageUtils::ShouldClearFromTextChange(false, true));
        VERIFY_IS_FALSE(CodePageUtils::ShouldClearFromTextChange(false, false));
    }

    void CodePageUtilsTests::SingleResultCommitRequiresNoChosenSuggestion()
    {
        VERIFY_IS_TRUE(CodePageUtils::ShouldCommitSingleResult(false, 1));
        VERIFY_IS_FALSE(CodePageUtils::ShouldCommitSingleResult(true, 1));
        VERIFY_IS_FALSE(CodePageUtils::ShouldCommitSingleResult(false, 0));
        VERIFY_IS_FALSE(CodePageUtils::ShouldCommitSingleResult(false, 2));
    }
}
