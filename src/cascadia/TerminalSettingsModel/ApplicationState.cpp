// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "ApplicationState.h"
#include "CascadiaSettings.h"
#include "ApplicationState.g.cpp"

#include "JsonUtils.h"
#include "FileUtils.h"

static constexpr std::string_view CloseAllTabsWarningDismissedKey{ "closeAllTabsWarningDismissed" };
static constexpr std::string_view LargePasteWarningDismissedKey{ "largePasteWarningDismissed" };
static constexpr std::string_view MultiLinePasteWarningDismissedKey{ "multiLinePasteWarningDismissed" };

using namespace ::Microsoft::Terminal::Settings::Model;

static std::filesystem::path _statePath()
{
    const auto statePath{ GetBaseSettingsPath() / L"state.json" };
    return statePath;
}

namespace winrt::Microsoft::Terminal::Settings::Model::implementation
{
    static std::tuple<std::reference_wrapper<std::mutex>, std::reference_wrapper<winrt::com_ptr<ApplicationState>>> _getStaticStorage()
    {
        static std::mutex mutex{};
        static winrt::com_ptr<ApplicationState> storage{ nullptr };
        return std::make_tuple(std::ref(mutex), std::ref(storage));
    }

    Microsoft::Terminal::Settings::Model::ApplicationState ApplicationState::GetForCurrentApp()
    {
        auto [mtx, state] = _getStaticStorage();
        std::lock_guard<std::mutex> lock{ mtx };
        if (auto& s{ state.get() })
        {
            return *s;
        }

        auto newState{ LoadAll() };
        state.get() = newState;
        return *newState;
    }

    winrt::com_ptr<ApplicationState> ApplicationState::LoadAll()
    {
        const auto path{ _statePath() };
        wil::unique_hfile hFile{ CreateFileW(path.c_str(),
                                             GENERIC_READ,
                                             FILE_SHARE_READ | FILE_SHARE_WRITE,
                                             nullptr,
                                             OPEN_EXISTING,
                                             FILE_ATTRIBUTE_NORMAL,
                                             nullptr) };

        auto newState{ winrt::make_self<ApplicationState>(path) };
        if (hFile)
        {
            const auto data{ ReadUTF8TextFileFull(hFile.get()) };

            std::string errs; // This string will receive any error text from failing to parse.
            std::unique_ptr<Json::CharReader> reader{ Json::CharReaderBuilder::CharReaderBuilder().newCharReader() };

            // Parse the json data into either our defaults or user settings. We'll keep
            // these original json values around for later, in case we need to parse
            // their raw contents again.
            Json::Value root;
            // `parse` will return false if it fails.
            if (reader->parse(data.data(), data.data() + data.size(), &root, &errs))
            {
                newState->LayerJson(root);
            }
        }
        return newState;
    }

    void ApplicationState::LayerJson(const Json::Value& document)
    {
        JsonUtils::GetValueForKey(document, CloseAllTabsWarningDismissedKey, _CloseAllTabsWarningDismissed);
        JsonUtils::GetValueForKey(document, LargePasteWarningDismissedKey, _LargePasteWarningDismissed);
        JsonUtils::GetValueForKey(document, MultiLinePasteWarningDismissedKey, _MultiLinePasteWarningDismissed);
    }

    Json::Value ApplicationState::ToJson() const
    {
        Json::Value document{ Json::objectValue };
        JsonUtils::SetValueForKey(document, CloseAllTabsWarningDismissedKey, _CloseAllTabsWarningDismissed);
        JsonUtils::SetValueForKey(document, LargePasteWarningDismissedKey, _LargePasteWarningDismissed);
        JsonUtils::SetValueForKey(document, MultiLinePasteWarningDismissedKey, _MultiLinePasteWarningDismissed);
        return document;
    }

    void ApplicationState::Reset()
    {
        auto [mtx, state] = _getStaticStorage();
        std::lock_guard<std::mutex> lock{ mtx };
        winrt::com_ptr<ApplicationState> oldState{ nullptr };
        std::swap(oldState, state.get());
        oldState->ResetInstance();
    }

    void ApplicationState::Commit()
    {
        if (_invalidated)
        {
            // We were destroyed, don't write.
            return;
        }

        Json::StreamWriterBuilder wbuilder;
        const auto content{ Json::writeString(wbuilder, ToJson()) };
        wil::unique_hfile hOut{ CreateFileW(_path.c_str(),
                                            GENERIC_WRITE,
                                            FILE_SHARE_READ | FILE_SHARE_WRITE,
                                            nullptr,
                                            CREATE_ALWAYS,
                                            FILE_ATTRIBUTE_NORMAL,
                                            nullptr) };
        THROW_LAST_ERROR_IF(!hOut);
        THROW_LAST_ERROR_IF(!WriteFile(hOut.get(), content.data(), gsl::narrow<DWORD>(content.size()), nullptr, nullptr));
    }

    void ApplicationState::ResetInstance()
    {
        std::filesystem::remove(_path);
        _invalidated = true; // don't write the file later -- just in case somebody has an outstanding reference
    }
}
