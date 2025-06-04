//Copyright (c) Microsoft Corporation
//Licensed under the MIT license. 

#pragma once

#include "SettingsCard.g.h"
#include "Utils.h"

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    // TODO CARLOS: SettingsCardT should work. Might be adding MIDL file improperly?
    //struct SettingsCard : SettingsCardT<SettingsCard>
    struct SettingsCard : SettingsCard_base<SettingsCard>
    {
    public:
        SettingsCard();

        DEPENDENCY_PROPERTY(Windows::Foundation::IInspectable, Header);
        DEPENDENCY_PROPERTY(Windows::Foundation::IInspectable, Description);
        DEPENDENCY_PROPERTY(Windows::UI::Xaml::Controls::IconElement, HeaderIcon);
        DEPENDENCY_PROPERTY(Windows::UI::Xaml::Controls::IconElement, ActionIcon);
        DEPENDENCY_PROPERTY(hstring, ActionIconToolTip);
        DEPENDENCY_PROPERTY(bool, IsClickEnabled);
        DEPENDENCY_PROPERTY(Editor::ContentAlignment, ContentAlignment);
        DEPENDENCY_PROPERTY(bool, IsActionIconVisible);

    private:
        static void _InitializeProperties();

        void _OnHeaderChanged();
        void _OnDescriptionChanged();
        void _OnHeaderIconChanged();
        void _OnIsClickEnabledChanged();
        void _OnIsActionIconVisibleChanged();


    };
}

namespace winrt::Microsoft::Terminal::Settings::Editor::factory_implementation
{
    BASIC_FACTORY(SettingsCard);
}
