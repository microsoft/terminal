#pragma once
#include "Microsoft.Terminal.Settings.Model.ColorSchemeModel.g.h"
#include "ColorScheme.h"

namespace winrt::Microsoft::Terminal::Settings::Model::implementation
{
    struct ColorSchemeModel : ColorSchemeModelT<ColorSchemeModel>
    {
        ColorSchemeModel();

        Model::ColorScheme ColorScheme();

    private:
        Model::ColorScheme m_colorScheme{ nullptr };
    };
}
