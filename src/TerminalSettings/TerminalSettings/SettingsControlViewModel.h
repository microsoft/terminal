#pragma once
#include "SettingsControlViewModel.g.h"
#include "HomeGridItem.h"

namespace winrt::SettingsControl::implementation
{
    struct SettingsControlViewModel : SettingsControlViewModelT<SettingsControlViewModel>
    {
        SettingsControlViewModel();

        SettingsControl::HomeGridItem HomeGridItem();

        Windows::Foundation::Collections::IObservableVector<SettingsControl::HomeGridItem> HomeGridItems();

        private:
            SettingsControl::HomeGridItem m_homegriditem{ nullptr };
            Windows::Foundation::Collections::IObservableVector<SettingsControl::HomeGridItem> m_homegriditems;
    };
}
