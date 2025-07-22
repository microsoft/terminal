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

struct MediaResourcePath
{
    winrt::hstring value{};
    bool ok{ false };
    bool resolved{ false };

    void reset() { *this = MediaResourcePath{}; }
    winrt::hstring resolved_or(const winrt::hstring& other) const { return resolved ? value : other; }
};

struct MediaResource : winrt::implements<MediaResource, winrt::Microsoft::Terminal::Settings::Model::IMediaResource, winrt::non_agile, winrt::no_weak_ref, winrt::no_module_lock>
{
    MediaResource(const winrt::hstring& p) :
        path{ p, false, false } {}

    winrt::hstring Path() { return path.value; };

    void Set(winrt::hstring newPath)
    {
        path.value = newPath;
        path.ok = true;
        path.resolved = true;
    }

    void Reject()
    {
        path.value = {};
        path.ok = false;
        path.resolved = true;
    }

    MediaResourcePath path;
};

using ThingResource = MediaResource; // stopgap to get it to compile

_TIL_INLINEPREFIX void ResolveMediaResourceIntoPath(const winrt::hstring& basePath, const winrt::hstring& unresolvedPath, const winrt::Microsoft::Terminal::Settings::Model::MediaResourceResolver& resolver, MediaResourcePath& resolvedPath)
{
    auto mediaResource{ winrt::make_self<MediaResource>(unresolvedPath) };
    resolver(basePath, *mediaResource); // populates MediaResourcePath
    resolvedPath = std::move(mediaResource->path);
}

_TIL_INLINEPREFIX void ResolveIconMediaResourceIntoPath(const winrt::hstring& basePath, const winrt::hstring& unresolvedPath, const winrt::Microsoft::Terminal::Settings::Model::MediaResourceResolver& resolver, MediaResourcePath& resolvedPath)
{
    std::wstring_view unresolvedPathAsView{ unresolvedPath };
    if (unresolvedPath.size() <= 2 || unresolvedPathAsView.find_first_of(L'\u200D') <= 8)
    {
        // **HEURISTIC**
        // If it's 2 code units long or contains a zero-width joiner in the first 8 code units, it is
        // PROBABLY an Emoji. Just pass it through.
        resolvedPath = { unresolvedPath, true, true };
        return;
    }

    ResolveMediaResourceIntoPath(basePath, unresolvedPath, resolver, resolvedPath);
}
