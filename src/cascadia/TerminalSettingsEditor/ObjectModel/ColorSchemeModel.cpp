#include "pch.h"
#include "ColorScheme.h"
#include "ObjectModel.ColorSchemeModel.g.cpp"
#include "ColorSchemeModel.h"


namespace winrt::ObjectModel::implementation
{
    ColorSchemeModel::ColorSchemeModel()
    {
        m_colorScheme = winrt::make<ObjectModel::implementation::ColorScheme>();
    }
    ObjectModel::ColorScheme ColorSchemeModel::ColorScheme()
    {
        return m_colorScheme;
    }
}
