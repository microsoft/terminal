#pragma once
#include "ObjectModel.GlobalSettingsModel.g.h"
#include "GlobalSettings.h"

namespace winrt::ObjectModel::implementation
{
    struct GlobalSettingsModel : GlobalSettingsModelT<GlobalSettingsModel>
    {
        GlobalSettingsModel();

        ObjectModel::GlobalSettings GlobalSettings();

    private:
        ObjectModel::GlobalSettings m_globalSettings{ nullptr };
    };
}
