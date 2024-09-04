// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "EditAction.g.h"
#include "Utils.h"
#include "ViewModelHelpers.h"

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    struct EditAction : public HasScrollViewer<EditAction>, EditActionT<EditAction>
    {
    public:
        EditAction();

        winrt::hstring Ayy() { return L"lmao"; };
    };
}

namespace winrt::Microsoft::Terminal::Settings::Editor::factory_implementation
{
    BASIC_FACTORY(EditAction);
}
