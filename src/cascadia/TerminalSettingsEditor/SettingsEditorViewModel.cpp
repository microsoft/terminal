// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "SettingsEditorViewModel.h"
#include "SettingsEditorViewModel.g.cpp"

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    SettingsEditorViewModel::SettingsEditorViewModel()
    {
        _HomeGridItems = winrt::single_threaded_observable_vector<Editor::HomeGridItem>();
    }

    Editor::HomeGridItem SettingsEditorViewModel::HomeGridItem()
    {
        return _HomeGridItem;
    }

    Windows::Foundation::Collections::IObservableVector<Editor::HomeGridItem> SettingsEditorViewModel::HomeGridItems()
    {
        return _HomeGridItems;
    }
}
