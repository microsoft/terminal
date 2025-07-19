/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.
--*/

#pragma once

struct
    __declspec(uuid("6068ee1b-1ea0-4804-993a-42ef0c58d867"))
        IMediaResourceContainer : public IUnknown
{
    virtual void ResolveMediaResources(const winrt::Microsoft::Terminal::Settings::Model::MediaResourceResolver& resolver) = 0;
};

struct
    __declspec(uuid("9f11361c-7c8f-45c9-8948-36b66d67eca8"))
        IPathlessMediaResourceContainer : public IUnknown
{
    virtual void ResolveMediaResourcesWithBasePath(const winrt::hstring& basePath, const winrt::Microsoft::Terminal::Settings::Model::MediaResourceResolver& resolver) = 0;
};

struct ThingResource : winrt::implements<ThingResource, winrt::Microsoft::Terminal::Settings::Model::IMediaResource, winrt::non_agile, winrt::no_weak_ref, winrt::no_module_lock>
{
    ThingResource(winrt::hstring p) :
        path{ p }, ok{ false } {};

    winrt::hstring Path() { return path; };
    void Set(winrt::hstring newPath)
    {
        path = newPath;
        ok = true;
    }
    void Reject()
    {
        path = {};
        ok = false;
    }

    winrt::hstring path;
    bool ok;
};
