#include "pch.h"
#include "KeybindingsSettingsContent.h"
#if __has_include("KeybindingsSettingsContent.g.cpp")
#include "KeybindingsSettingsContent.g.cpp"
#endif

using namespace winrt;
using namespace Windows::UI::Xaml;
using namespace ::TerminalApp;

namespace winrt::TerminalApp::implementation
{
    KeybindingsSettingsContent::KeybindingsSettingsContent()
    {
        InitializeComponent();
    }

    KeybindingsSettingsContent::KeybindingsSettingsContent(std::shared_ptr<::TerminalApp::CascadiaSettings> settings)
    {
        InitializeComponent();

        _settings = settings;
    }

    int32_t KeybindingsSettingsContent::MyProperty()
    {
        throw hresult_not_implemented();
    }

    void KeybindingsSettingsContent::MyProperty(int32_t /* value */)
    {
        throw hresult_not_implemented();
    }
}
