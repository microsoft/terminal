/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- TestUtils.h

Abstract:
- This file has helper functions for writing tests for the TerminalApp project.

Author(s):
    Mike Griese (migrie) December-2019
--*/

class TerminalAppLocalTests::TestUtils
{
public:
    // Function Description:
    // - This is a helper to retrieve the ActionAndArgs from the keybindings
    //   for a given chord.
    // Arguments:
    // - bindings: The AppKeyBindings to lookup the ActionAndArgs from.
    // - kc: The key chord to look up the bound ActionAndArgs for.
    // Return Value:
    // - The ActionAndArgs bound to the given key, or nullptr if nothing is bound to it.
    static const winrt::TerminalApp::ActionAndArgs GetActionAndArgs(const winrt::TerminalApp::implementation::AppKeyBindings& bindings,
                                                                    const winrt::Microsoft::Terminal::Settings::KeyChord& kc)
    {
        std::wstring buffer{ L"" };
        if (WI_IsFlagSet(kc.Modifiers(), winrt::Microsoft::Terminal::Settings::KeyModifiers::Ctrl))
        {
            buffer += L"Ctrl+";
        }
        if (WI_IsFlagSet(kc.Modifiers(), winrt::Microsoft::Terminal::Settings::KeyModifiers::Shift))
        {
            buffer += L"Shift+";
        }
        if (WI_IsFlagSet(kc.Modifiers(), winrt::Microsoft::Terminal::Settings::KeyModifiers::Alt))
        {
            buffer += L"Alt+";
        }
        buffer += static_cast<wchar_t>(MapVirtualKeyW(kc.Vkey(), MAPVK_VK_TO_CHAR));
        WEX::Logging::Log::Comment(WEX::Common::NoThrowString().Format(L"Looking for key:%s", buffer.c_str()));

        const auto keyIter = bindings._keyShortcuts.find(kc);
        VERIFY_IS_TRUE(keyIter != bindings._keyShortcuts.end(), L"Expected to find an action bound to the given KeyChord");
        if (keyIter != bindings._keyShortcuts.end())
        {
            return keyIter->second;
        }
        return nullptr;
    };
};
