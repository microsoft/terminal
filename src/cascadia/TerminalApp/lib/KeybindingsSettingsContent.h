#pragma once

#include "KeybindingsSettingsContent.g.h"

namespace winrt::TerminalApp::implementation
{
    struct KeybindingsSettingsContent : KeybindingsSettingsContentT<KeybindingsSettingsContent>
    {
        KeybindingsSettingsContent();

        int32_t MyProperty();
        void MyProperty(int32_t value);

    };
}

namespace winrt::TerminalApp::factory_implementation
{
    struct KeybindingsSettingsContent : KeybindingsSettingsContentT<KeybindingsSettingsContent, implementation::KeybindingsSettingsContent>
    {
    };
}
