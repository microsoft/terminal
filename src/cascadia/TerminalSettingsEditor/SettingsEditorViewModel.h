// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once
#include "SettingsEditorViewModel.g.h"
#include "HomeGridItem.h"

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    struct SettingsEditorViewModel : SettingsEditorViewModelT<SettingsEditorViewModel>
    {
        SettingsEditorViewModel();

        Editor::HomeGridItem HomeGridItem();

        Windows::Foundation::Collections::IObservableVector<Editor::HomeGridItem> HomeGridItems();

    private:
        Editor::HomeGridItem _HomeGridItem{ nullptr };
        Windows::Foundation::Collections::IObservableVector<Editor::HomeGridItem> _HomeGridItems;
    };
}
