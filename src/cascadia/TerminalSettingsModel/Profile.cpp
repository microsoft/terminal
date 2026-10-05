// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "Profile.h"
#include "Profile.g.cpp"
#include "AdapterCache.h"

namespace
{
    using ProfileAdapter = winrt::Microsoft::Terminal::Settings::Model::implementation::Profile;
    using NativeProfile = Microsoft::Terminal::Settings::Model::Native::Profile;

    auto& profileAdapters()
    {
        static Microsoft::Terminal::Settings::Model::Adapters::AdapterCache<NativeProfile, ProfileAdapter> cache;
        return cache;
    }
}

namespace winrt::Microsoft::Terminal::Settings::Model::implementation
{
    Profile::Profile() noexcept :
        _native{ Native::Profile::Create() }
    {
        profileAdapters().Register(_native.get(), this);
    }

    Profile::Profile(const winrt::guid guid) noexcept :
        _native{ Native::Profile::Create(guid) }
    {
        profileAdapters().Register(_native.get(), this);
    }

    Profile::Profile(winrt::com_ptr<Native::Profile> native) :
        _native{ std::move(native) }
    {
    }

    Profile::~Profile()
    {
        profileAdapters().Remove(_native.get(), this);
    }

    winrt::com_ptr<Profile> Profile::FromNative(const winrt::com_ptr<Native::Profile>& native)
    {
        return profileAdapters().Get(native);
    }

    winrt::com_ptr<Profile> Profile::FromJson(const Json::Value& json)
    {
        return FromNative(Native::Profile::FromJson(json));
    }

    void Profile::LayerJson(const Json::Value& json)
    {
        _native->LayerJson(json);
    }

    Json::Value Profile::ToJson() const
    {
        return _native->ToJson();
    }

    void Profile::CopyInheritanceGraphs(CopyMap& visited, const std::vector<winrt::com_ptr<Profile>>& source, std::vector<winrt::com_ptr<Profile>>& target)
    {
        for (const auto& profile : source)
        {
            target.emplace_back(profile->CopyInheritanceGraph(visited));
        }
    }

    winrt::com_ptr<Profile> Profile::CopyInheritanceGraph(CopyMap& visited) const
    {
        return FromNative(_native->CopyInheritanceGraph(visited));
    }

    std::vector<winrt::com_ptr<Profile>> Profile::Parents() const
    {
        std::vector<winrt::com_ptr<Profile>> parents;
        parents.reserve(_native->Parents().size());
        for (const auto& parent : _native->Parents())
        {
            parents.emplace_back(FromNative(parent));
        }
        return parents;
    }

    void Profile::ResolveMediaResources(const Model::MediaResourceResolver& resolver)
    {
        _native->ResolveMediaResources([&](const Native::OriginTag origin, const winrt::hstring& path, const winrt::com_ptr<Native::MediaResource>& resource) {
            resolver(Adapters::ToProjected(origin), path, Adapters::ToProjected(resource));
        });
    }
}

namespace Microsoft::Terminal::Settings::Model::Adapters
{
    winrt::com_ptr<Native::Profile> ToNative(const winrt::Microsoft::Terminal::Settings::Model::Profile& profile)
    {
        return profile ? winrt::get_self<ProfileAdapter>(profile)->NativeModel() : nullptr;
    }

    winrt::Microsoft::Terminal::Settings::Model::Profile ToProjected(const winrt::com_ptr<Native::Profile>& profile)
    {
        const auto adapter = ProfileAdapter::FromNative(profile);
        if (adapter)
        {
            return *adapter;
        }
        return nullptr;
    }
}
