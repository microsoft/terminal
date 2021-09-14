/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- BaseApplicationState.h

Abstract:
- This is the common core for both ApplicationState and ElevatedState. This
  handles more of the mechanics of serializing these structures to/from json, as
  well as the mechanics of loading the file.
--*/
#pragma once

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
    virtual std::optional<std::string> _readFileContents() const;
    virtual void _writeFileContents(const std::string_view content) const;

    void _write() const noexcept;
    void _read() const noexcept;

    std::filesystem::path _path;
    til::throttled_func_trailing<> _throttler;
};
