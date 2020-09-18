#include "pch.h"
#include "GlobalSettings.h"
#include "Model.GlobalSettingsModel.g.cpp"
#include "GlobalSettingsModel.h"

namespace winrt::Microsoft::Terminal::Settings::Editor::Model::implementation
{
    GlobalSettingsModel::GlobalSettingsModel()
    {
        m_globalSettings = nullptr;
        m_globalSettings = winrt::make<implementation::GlobalSettings>();
    }
    Model::GlobalSettings GlobalSettingsModel::GlobalSettings()
    {
        return m_globalSettings;
    }
}
