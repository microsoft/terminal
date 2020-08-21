#include "pch.h"
#include "ObjectModel/Profile.h"
#include "Microsoft.Terminal.Settings.Model.ProfileModel.g.cpp"
#include "ProfileModel.h"

namespace winrt::Microsoft::Terminal::Settings::Model::implementation
{
    ProfileModel::ProfileModel()
    {
        m_Profile = winrt::make<implementation::Profile>();
    }

    Model::Profile ProfileModel::Profile()
    {
        return m_Profile;
    }
}
