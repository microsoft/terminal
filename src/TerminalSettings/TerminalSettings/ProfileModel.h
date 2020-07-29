#pragma once
#include "ObjectModel.ProfileModel.g.h"
#include "ObjectModel/Profile.h"

namespace winrt::ObjectModel::implementation
{
    struct ProfileModel : ProfileModelT<ProfileModel>
    {
        ProfileModel();

        ObjectModel::Profile Profile();

    private:
        ObjectModel::Profile m_Profile{ nullptr };
    };
}
