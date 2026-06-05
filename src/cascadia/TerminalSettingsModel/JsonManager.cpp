// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "JsonManager.h"
#include "CascadiaSettings.h"
#include "FileUtils.h"

#include <til/io.h>

#include "JsonManager.g.cpp"

using namespace winrt::Microsoft::Terminal::Settings::Model;

// How long to coalesce a burst of auto-save requests before writing to disk.
static constexpr auto AutoSaveDebounceDelay{ std::chrono::milliseconds{ 500 } };

namespace winrt::Microsoft::Terminal::Settings::Model::implementation
{
    JsonManager::JsonManager() :
        _writeThrottler{
            til::throttled_func_options{
                .delay = AutoSaveDebounceDelay,
                .debounce = true,
                .trailing = true,
            },
            [this]() { _write(); }
        }
    {
    }

    JsonManager::~JsonManager()
    {
        // Tear down the watcher before the rest of *this is destroyed.
        // _writeThrottler (last-declared member) is destroyed first, cancelling
        // any pending callback.
        Stop();
    }

    // Begins watching the settings directory for external changes.
    void JsonManager::Start()
    {
        const std::filesystem::path settingsPath{ std::wstring_view{ CascadiaSettings::SettingsPath() } };
        const auto directory = settingsPath.parent_path();
        _watcher.reset();
        _watcher.create(
            directory.c_str(),
            false,
            // Watch for modifications AND renames -- editors often write a temp
            // file then rename it over settings.json.
            wil::FolderChangeEvents::FileName | wil::FolderChangeEvents::LastWriteTime,
            [this, settingsBasename = settingsPath.filename()](wil::FolderChangeEvent, PCWSTR fileModified) {
                const auto modifiedBasename = std::filesystem::path{ fileModified }.filename();
                if (modifiedBasename == settingsBasename)
                {
                    _onFileChanged();
                }
            });
    }

    // Stops watching the settings directory.
    void JsonManager::Stop()
    {
        _watcher.reset();
    }

    // Binds the successfully-loaded settings tree and installs the auto-save
    // handler on it. Bumping the generation invalidates any callback still
    // referencing a previous tree. The caller MUST pass only a successfully
    // loaded tree (never a failed load or the defaults fallback).
    void JsonManager::SetLiveSettings(const Model::CascadiaSettings& settings)
    {
        {
            std::scoped_lock lock{ _stateMutex };
            ++_generation;
            _pendingSnapshot.clear();
            _autoSaveEnabled = static_cast<bool>(settings);
            _expectedDiskHash = settings ? settings.Hash() : winrt::hstring{};
        }

        _liveSettings = settings ? winrt::make_weak(settings) : winrt::weak_ref<Model::CascadiaSettings>{ nullptr };

        if (settings)
        {
            auto self = winrt::get_self<implementation::CascadiaSettings>(settings);
            auto weakThis = get_weak();
            self->SetWriteHandler([weakThis]() {
                if (auto strong = weakThis.get())
                {
                    strong->RequestSave();
                }
            });
        }
    }

    // Unbinds the live tree and disables auto-save (e.g. on load failure or
    // when falling back to the defaults settings object).
    void JsonManager::ClearLiveSettings()
    {
        {
            std::scoped_lock lock{ _stateMutex };
            ++_generation;
            _pendingSnapshot.clear();
            _autoSaveEnabled = false;
            _expectedDiskHash = {};
        }
        _liveSettings = winrt::weak_ref<Model::CascadiaSettings>{ nullptr };
    }

    // Captures a snapshot of the bound tree (on the calling/owner thread) and
    // schedules a debounced write. Snapshotting here, rather than on the timer
    // thread, keeps all model-tree traversal on the owner thread.
    void JsonManager::RequestSave()
    {
        uint64_t generation{};
        {
            std::scoped_lock lock{ _stateMutex };
            if (!_autoSaveEnabled)
            {
                return;
            }
            generation = _generation;
        }

        const auto live = _liveSettings.get();
        if (!live)
        {
            return;
        }

        std::string snapshot;
        try
        {
            snapshot = _serialize(live);
        }
        catch (...)
        {
            LOG_CAUGHT_EXCEPTION();
            return;
        }

        {
            std::scoped_lock lock{ _stateMutex };
            // If the live tree was swapped while we serialized, this snapshot is
            // already stale --> drop it.
            if (generation != _generation)
            {
                return;
            }
            _pendingSnapshot = std::move(snapshot);
            _pendingSnapshotGeneration = generation;
        }
        _writeThrottler();
    }

    // Synchronously writes the bound tree to disk. Used by callers that need the
    // write to complete before returning (rather than the debounced path).
    bool JsonManager::Save()
    {
        {
            std::scoped_lock lock{ _stateMutex };
            if (!_autoSaveEnabled)
            {
                return false;
            }
        }

        const auto live = _liveSettings.get();
        if (!live)
        {
            return false;
        }

        std::string snapshot;
        try
        {
            snapshot = _serialize(live);
        }
        catch (...)
        {
            LOG_CAUGHT_EXCEPTION();
            return false;
        }
        return _persistSnapshot(snapshot);
    }

    // Debounce callback (timer thread): writes the captured snapshot.
    void JsonManager::_write() noexcept
    {
        std::string snapshot;
        {
            std::scoped_lock lock{ _stateMutex };
            // Drop a snapshot captured under a now-superseded tree.
            if (_pendingSnapshotGeneration != _generation)
            {
                _pendingSnapshot.clear();
                return;
            }
            snapshot = std::move(_pendingSnapshot);
            _pendingSnapshot.clear();
        }
        if (snapshot.empty())
        {
            return;
        }
        _persistSnapshot(snapshot);
    }

    // Writes a snapshot to disk, guarding against lost updates. If the on-disk
    // file diverged from our baseline (an external edit), the disk wins: the
    // save is cancelled and SettingsChangedExternally is raised so the app
    // reloads. Otherwise the snapshot is written atomically and the expected
    // hash is updated so the resulting watcher notification is treated as a
    // self-write.
    bool JsonManager::_persistSnapshot(const std::string& snapshot) noexcept
    try
    {
        const std::filesystem::path path{ std::wstring_view{ CascadiaSettings::SettingsPath() } };

        auto conflict = false;
        {
            // The write AND the expected-hash update happen under the same lock
            // the watcher uses. That way the watcher notification this write
            // generates can't observe the new file against a stale expected hash
            // (which would look like an external edit and cause a spurious reload).
            std::scoped_lock lock{ _stateMutex };

            FILETIME diskTime{};
            const auto diskContent = til::io::read_file_as_utf8_string_if_exists(path, false, &diskTime);
            if (!diskContent.empty())
            {
                const auto diskHash = CalculateSettingsHash(diskContent, diskTime);
                if (!_expectedDiskHash.empty() && diskHash != _expectedDiskHash)
                {
                    // Disk diverged from our baseline, likely due to an external edit. Disk is
                    // the source of truth, so drop our pending change.
                    conflict = true;
                }
            }

            if (!conflict)
            {
                FILETIME newTime{};
                WriteSettingsFile(path, snapshot, /*makeBackup*/ true, &newTime);
                _expectedDiskHash = CalculateSettingsHash(snapshot, newTime);
            }
        }

        if (conflict)
        {
            _SettingsChangedExternallyHandlers(*this, nullptr);
            return false;
        }
        return true;
    }
    catch (...)
    {
        LOG_CAUGHT_EXCEPTION();
        return false;
    }

    // Watcher callback (background thread): decides whether a settings.json
    // change was ours (self-write) or external, and raises the event for the
    // latter. The app is responsible for marshaling any reload onto the UI
    // thread.
    void JsonManager::_onFileChanged() noexcept
    try
    {
        const std::filesystem::path path{ std::wstring_view{ CascadiaSettings::SettingsPath() } };

        FILETIME diskTime{};
        const auto diskContent = til::io::read_file_as_utf8_string_if_exists(path, false, &diskTime);

        auto external = false;
        {
            std::scoped_lock lock{ _stateMutex };
            if (diskContent.empty())
            {
                // File deleted/emptied -- treat as external unless we never knew a hash.
                external = !_expectedDiskHash.empty();
            }
            else
            {
                const auto diskHash = CalculateSettingsHash(diskContent, diskTime);
                external = diskHash != _expectedDiskHash;
            }
        }

        if (external)
        {
            _SettingsChangedExternallyHandlers(*this, nullptr);
        }
    }
    catch (...)
    {
        LOG_CAUGHT_EXCEPTION();
    }

    // Serializes a settings tree to JSON using the same writer settings as
    // CascadiaSettings::WriteSettingsToDisk. Must run on the owner thread.
    std::string JsonManager::_serialize(const Model::CascadiaSettings& settings)
    {
        auto self = winrt::get_self<implementation::CascadiaSettings>(settings);

        Json::StreamWriterBuilder wbuilder;
        wbuilder.settings_["enableYAMLCompatibility"] = true; // suppress spaces around colons
        wbuilder.settings_["indentation"] = "    ";
        wbuilder.settings_["precision"] = 6; // prevent values like 1.1000000000000001

        return Json::writeString(wbuilder, self->ToJson());
    }
}
