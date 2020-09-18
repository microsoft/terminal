#include "pch.h"
#include "ColorScheme.h"
#include "Model.ColorSchemeModel.g.cpp"
#include "ColorSchemeModel.h"

namespace winrt::Microsoft::Terminal::Settings::Editor::Model::implementation
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
