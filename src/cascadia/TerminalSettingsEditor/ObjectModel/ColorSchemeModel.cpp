#include "pch.h"
#include "ColorScheme.h"
#include "Microsoft.Terminal.Settings.Model.ColorSchemeModel.g.cpp"
#include "ColorSchemeModel.h"

namespace winrt::Microsoft::Terminal::Settings::Model::implementation
{
    ColorSchemeModel::ColorSchemeModel()
    {
        m_colorScheme = winrt::make<implementation::ColorScheme>();
    }
    Model::ColorScheme ColorSchemeModel::ColorScheme()
    {
        return m_colorScheme;
    }
}
