#pragma once
#include "Model.ColorSchemeModel.g.h"
#include "ColorScheme.h"

namespace winrt::Microsoft::Terminal::Settings::Editor::Model::implementation
{
    struct ColorSchemeModel : ColorSchemeModelT<ColorSchemeModel>
    {
        ColorSchemeModel();

        Model::ColorScheme ColorScheme();

    private:
        Model::ColorScheme m_colorScheme{ nullptr };
    };
}
