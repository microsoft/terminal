#include "pch.h"
#include "ObjectModel/Profile.h"
#include "ObjectModel.ProfileModel.g.cpp"
#include "ProfileModel.h"

namespace winrt::ObjectModel::implementation
{
    ProfileModel::ProfileModel()
    {
        m_Profile = winrt::make<ObjectModel::implementation::Profile>();
    }

    ObjectModel::Profile ProfileModel::Profile()
    {
        return m_Profile;
    }
}
