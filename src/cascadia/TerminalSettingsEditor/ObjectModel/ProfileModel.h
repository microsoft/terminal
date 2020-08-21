#pragma once
#include "Microsoft.Terminal.Settings.Model.ProfileModel.g.h"
#include "ObjectModel/Profile.h"

namespace winrt::Microsoft::Terminal::Settings::Model::implementation
{
    struct ProfileModel : ProfileModelT<ProfileModel>
    {
        ProfileModel();

        Model::Profile Profile();

    private:
        Model::Profile m_Profile{ nullptr };
    };
}
