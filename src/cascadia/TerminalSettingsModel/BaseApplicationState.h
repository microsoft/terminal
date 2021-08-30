/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- ApplicationState.h

Abstract:
- TODO!
--*/
#pragma once

// namespace winrt::Microsoft::Terminal::Settings::Model::implementation
// {
struct BaseApplicationState
{
    BaseApplicationState(std::filesystem::path path) noexcept;
    ~BaseApplicationState();

    // Methods
    void Reload() const noexcept;

    // General getters/setters
    winrt::hstring FilePath() const noexcept;

    virtual void FromJson(const Json::Value& root) const noexcept = 0;
    virtual Json::Value ToJson() const noexcept = 0;

protected:
    void _write() const noexcept;
    void _read() const noexcept;

    std::filesystem::path _path;
    til::throttled_func_trailing<> _throttler;
};
// }
