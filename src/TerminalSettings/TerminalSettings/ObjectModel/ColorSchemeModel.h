#pragma once
#include "ObjectModel.ColorSchemeModel.g.h"
#include "ColorScheme.h"

namespace winrt::ObjectModel::implementation
{
    struct ColorSchemeModel : ColorSchemeModelT<ColorSchemeModel>
    {
        ColorSchemeModel();

        ObjectModel::ColorScheme ColorScheme();

    private:
        ObjectModel::ColorScheme m_colorScheme{ nullptr };
    };
}
