#pragma once

#include "KeybindingsSettingsContent.g.h"
#include "CascadiaSettings.h"

namespace winrt::TerminalApp::implementation
{
    struct KeybindingsSettingsContent : KeybindingsSettingsContentT<KeybindingsSettingsContent>
    {
    public:
        KeybindingsSettingsContent();
        KeybindingsSettingsContent(std::shared_ptr<::TerminalApp::CascadiaSettings> settings);

        int32_t MyProperty();
        void MyProperty(int32_t value);

         winrt::hstring getKeybinding();

    private:
        std::shared_ptr<::TerminalApp::CascadiaSettings> _settings{ nullptr };
    };
}

namespace winrt::TerminalApp::factory_implementation
{
    struct KeybindingsSettingsContent : KeybindingsSettingsContentT<KeybindingsSettingsContent, implementation::KeybindingsSettingsContent>
    {
    };
}
