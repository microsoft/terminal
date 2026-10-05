// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include <cstdint>
#include <optional>
#include <string>

#ifndef NATIVE_BOUNDARY
#include <winrt/Windows.Foundation.h>
#include <winrt/Microsoft.Terminal.Settings.Model.h>
#endif

namespace boundary
{
    struct LaunchValues
    {
        std::wstring commandline;
        std::wstring directory;
        std::wstring title;
        std::wstring profile;
        std::optional<int32_t> profileIndex;
        std::optional<bool> elevate;

        bool operator==(const LaunchValues&) const = default;
    };

#ifdef NATIVE_BOUNDARY
    using Request = LaunchValues;
#else
    using Request = winrt::Microsoft::Terminal::Settings::Model::NewTerminalArgs;
#endif

    Request Load(const LaunchValues& input, const std::wstring& modelDll);
    Request Edit(Request request, const std::wstring& title);
    Request Prepare(Request request, const std::wstring& directory);

    inline LaunchValues Read(const Request& request)
    {
#ifdef NATIVE_BOUNDARY
        return request;
#else
        LaunchValues values;
        values.commandline = request.Commandline();
        values.directory = request.StartingDirectory();
        values.title = request.TabTitle();
        values.profile = request.Profile();
        if (const auto index = request.ProfileIndex())
        {
            values.profileIndex = index.Value();
        }
        if (const auto elevate = request.Elevate())
        {
            values.elevate = elevate.Value();
        }
        return values;
#endif
    }

#ifdef BUILD_BOUNDARY_DLL
#define BOUNDARY_API __declspec(dllexport)
#else
#define BOUNDARY_API __declspec(dllimport)
#endif

    BOUNDARY_API Request Build(const LaunchValues& input,
                               const std::wstring& title,
                               const std::wstring& directory,
                               const std::wstring& modelDll);
}
