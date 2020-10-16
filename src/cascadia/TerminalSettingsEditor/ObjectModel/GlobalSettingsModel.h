#pragma once
#include "Model.GlobalSettingsModel.g.h"
#include "GlobalSettings.h"

namespace winrt::Microsoft::Terminal::Settings::Editor::Model::implementation
{
    struct GlobalSettingsModel : GlobalSettingsModelT<GlobalSettingsModel>
    {
        GlobalSettingsModel();

        Model::GlobalSettings GlobalSettings();

    private:
        Model::GlobalSettings m_globalSettings{ nullptr };
    };
}
