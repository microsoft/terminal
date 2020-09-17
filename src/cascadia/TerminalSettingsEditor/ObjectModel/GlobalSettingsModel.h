#pragma once
#include "Microsoft.Terminal.Settings.Model.GlobalSettingsModel.g.h"
#include "GlobalSettings.h"

namespace winrt::Microsoft::Terminal::Settings::Model::implementation
{
    struct GlobalSettingsModel : GlobalSettingsModelT<GlobalSettingsModel>
    {
        GlobalSettingsModel();

        Model::GlobalSettings GlobalSettings();

    private:
        Model::GlobalSettings m_globalSettings{ nullptr };
    };
}
