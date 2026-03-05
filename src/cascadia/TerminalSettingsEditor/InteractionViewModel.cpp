// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "InteractionViewModel.h"
#include "InteractionViewModel.g.cpp"
#include "EnumEntry.h"

using namespace winrt::Windows::UI::Xaml::Navigation;
using namespace winrt::Windows::Foundation;
using namespace winrt::Microsoft::Terminal::Settings::Model;

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    InteractionViewModel::InteractionViewModel(Model::GlobalAppSettings globalSettings) :
        _GlobalSettings{ globalSettings }
    {
        INITIALIZE_BINDABLE_ENUM_SETTING(TabSwitcherMode, TabSwitcherMode, TabSwitcherMode, L"Globals_TabSwitcherMode", L"Content");
        INITIALIZE_BINDABLE_ENUM_SETTING(CopyFormat, CopyFormat, winrt::Microsoft::Terminal::Control::CopyFormat, L"Globals_CopyFormat", L"Content");
    }

    hstring InteractionViewModel::ConfirmCloseOnPreview() const
    {
        const auto flags = ConfirmCloseOn();
        if (WI_IsFlagSet(flags, Model::ConfirmCloseOn::Always))
        {
            return RS_(L"Globals_ConfirmCloseOnAlways/Content");
        }
        else if (flags == Model::ConfirmCloseOn::Never)
        {
            return RS_(L"Globals_ConfirmCloseOnNever");
        }

        std::vector<hstring> resultList;
        resultList.reserve(2);
        if (WI_IsFlagSet(flags, Model::ConfirmCloseOn::MultipleTabs))
        {
            resultList.emplace_back(RS_(L"Globals_ConfirmCloseOnMultipleTabs/Content"));
        }
        if (WI_IsFlagSet(flags, Model::ConfirmCloseOn::MultiplePanes))
        {
            resultList.emplace_back(RS_(L"Globals_ConfirmCloseOnMultiplePanes/Content"));
        }

        hstring result{};
        for (auto&& entry : resultList)
        {
            if (result.empty())
            {
                result = entry;
            }
            else
            {
                result = result + L", " + entry;
            }
        }
        return result;
    }

    bool InteractionViewModel::IsConfirmCloseOnFlagSet(const uint32_t flag)
    {
        return (WI_EnumValue(ConfirmCloseOn()) & flag) == flag;
    }

    void InteractionViewModel::SetConfirmCloseOnAlways(winrt::Windows::Foundation::IReference<bool> on)
    {
        auto current = ConfirmCloseOn();
        WI_UpdateFlag(current, Model::ConfirmCloseOn::Always, winrt::unbox_value<bool>(on));
        ConfirmCloseOn(current);
    }

    void InteractionViewModel::SetConfirmCloseOnMultipleTabs(winrt::Windows::Foundation::IReference<bool> on)
    {
        auto current = ConfirmCloseOn();
        WI_UpdateFlag(current, Model::ConfirmCloseOn::MultipleTabs, winrt::unbox_value<bool>(on));
        ConfirmCloseOn(current);
    }

    void InteractionViewModel::SetConfirmCloseOnMultiplePanes(winrt::Windows::Foundation::IReference<bool> on)
    {
        auto current = ConfirmCloseOn();
        WI_UpdateFlag(current, Model::ConfirmCloseOn::MultiplePanes, winrt::unbox_value<bool>(on));
        ConfirmCloseOn(current);
    }
}
