#include "pch.h"
#include "GlobalSettings.h"
#include "ObjectModel.GlobalSettingsModel.g.cpp"
#include "GlobalSettingsModel.h"

namespace winrt::ObjectModel::implementation
{
    GlobalSettingsModel::GlobalSettingsModel()
    {
        m_globalSettings = nullptr;
        m_globalSettings = winrt::make<ObjectModel::implementation::GlobalSettings>();
    }
    ObjectModel::GlobalSettings GlobalSettingsModel::GlobalSettings()
    {
        return m_globalSettings;
    }
}
