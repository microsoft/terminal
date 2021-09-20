// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "BaseApplicationState.h"
#include "CascadiaSettings.h"

#include "JsonUtils.h"
#include "FileUtils.h"

using namespace ::Microsoft::Terminal::Settings::Model;

BaseApplicationState::BaseApplicationState(std::filesystem::path path) noexcept :
    _path{ std::move(path) },
    _throttler{ std::chrono::seconds(1), [this]() { _write(); } }
{
    // DON'T _read() here! _read() will call FromJson, which is virtual, and
    // needs to be implemented in a derived class. Classes that derive from
    // BaseApplicationState should make sure to call Reload() after construction
    // to ensure the data is loaded.
}

// The destructor ensures that the last write is flushed to disk before returning.
BaseApplicationState::~BaseApplicationState()
{
    // This will ensure that we not just cancel the last outstanding timer,
    // but instead force it to run as soon as possible and wait for it to complete.
    _throttler.flush();
}

// Re-read the state.json from disk.
void BaseApplicationState::Reload() const noexcept
{
    _read();
}

// Returns the state.json path on the disk.
winrt::hstring BaseApplicationState::FilePath() const noexcept
{
    return winrt::hstring{ _path.wstring() };
}

// Deserializes the state.json at _path into this ApplicationState.
// * ANY errors during app state will result in the creation of a new empty state.
// * ANY errors during runtime will result in changes being partially ignored.
void BaseApplicationState::_read() const noexcept
try
{
    // Use the derived class's implementation of _readFileContents to get the
    // actual contents of the file.
    const auto data = _readFileContents().value_or(std::string{});
    if (data.empty())
    {
        return;
    }

    std::string errs;
    std::unique_ptr<Json::CharReader> reader{ Json::CharReaderBuilder::CharReaderBuilder().newCharReader() };

    Json::Value root;
    if (!reader->parse(data.data(), data.data() + data.size(), &root, &errs))
    {
        throw winrt::hresult_error(WEB_E_INVALID_JSON_STRING, winrt::to_hstring(errs));
    }

    FromJson(root);
}
CATCH_LOG()

// Serialized this ApplicationState (in `context`) into the state.json at _path.
// * Errors are only logged.
// * _state->_writeScheduled is set to false, signaling our
//   setters that _synchronize() needs to be called again.
void BaseApplicationState::_write() const noexcept
try
{
    Json::Value root{ this->ToJson() };

    Json::StreamWriterBuilder wbuilder;
    const auto content = Json::writeString(wbuilder, root);

    // Use the derived class's implementation of _writeFileContents to write the
    // file to disk.
    _writeFileContents(content);
}
CATCH_LOG()
