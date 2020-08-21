// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "SettingsEditorViewModel.h"
#include "SettingsEditorViewModel.g.cpp"

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    SettingsEditorViewModel::SettingsEditorViewModel()
    {
        m_homegriditems = winrt::single_threaded_observable_vector<Editor::HomeGridItem>();
    }

    Editor::HomeGridItem SettingsEditorViewModel::HomeGridItem()
    {
        return m_homegriditem;
    }

    Windows::Foundation::Collections::IObservableVector<Editor::HomeGridItem> SettingsEditorViewModel::HomeGridItems()
    {
        return m_homegriditems;
    }
}
