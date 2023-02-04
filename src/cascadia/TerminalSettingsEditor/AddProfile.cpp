// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "AddProfile.h"
#include "AddProfile.g.cpp"
#include "AddProfilePageNavigationState.g.cpp"
#include "EnumEntry.h"

#include <LibraryResources.h>

using namespace WF;
using namespace WS;
using namespace WUC;
using namespace WUX;
using namespace WUX::Navigation;
using namespace MTSM;

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    AddProfile::AddProfile()
    {
        InitializeComponent();

        Automation::AutomationProperties::SetName(AddNewButton(), RS_(L"AddProfile_AddNewTextBlock/Text"));
        Automation::AutomationProperties::SetName(DuplicateButton(), RS_(L"AddProfile_DuplicateTextBlock/Text"));
    }

    void AddProfile::OnNavigatedTo(const NavigationEventArgs& e)
    {
        _State = e.Parameter().as<Editor::AddProfilePageNavigationState>();
    }

    void AddProfile::AddNewClick(const IInspectable& /*sender*/,
                                 const WUX::RoutedEventArgs& /*eventArgs*/)
    {
        _State.RequestAddNew();
    }

    void AddProfile::DuplicateClick(const IInspectable& /*sender*/,
                                    const WUX::RoutedEventArgs& /*eventArgs*/)
    {
        if (const auto selected = Profiles().SelectedItem())
        {
            _State.RequestDuplicate(selected.try_as<MTSM::Profile>().Guid());
        }
    }

    void AddProfile::ProfilesSelectionChanged(const IInspectable& /*sender*/,
                                              const WUX::RoutedEventArgs& /*eventArgs*/)
    {
        if (!_IsProfileSelected)
        {
            IsProfileSelected(true);
        }
    }
}
