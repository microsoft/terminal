#include "pch.h"
#include "SettingsControlViewModel.h"
#include "SettingsControlViewModel.g.cpp"

namespace winrt::SettingsControl::implementation
{
    SettingsControlViewModel::SettingsControlViewModel()
    {
        m_homegriditems = winrt::single_threaded_observable_vector<SettingsControl::HomeGridItem>();
    }

    SettingsControl::HomeGridItem SettingsControlViewModel::HomeGridItem()
    {
        return m_homegriditem;
    }

    Windows::Foundation::Collections::IObservableVector<SettingsControl::HomeGridItem> SettingsControlViewModel::HomeGridItems()
    {
        return m_homegriditems;
    }
}
