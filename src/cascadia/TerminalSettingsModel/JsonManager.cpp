// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "JsonManager.h"
#include "CascadiaSettings.h"
#include "FileUtils.h"
#include "ApplicationState.h"
#include "DefaultTerminal.h"
#include "resource.h"

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
        // Stop watching first so the flush's write below can't trigger a
        // self-write callback during teardown.
        Stop();

        // Flush any pending auto-save so a debounced write isn't lost on shutdown
        // (e.g. a setting changed moments before the app exits). The flush runs
        // _write(), which only touches the captured byte snapshot -- not the tree.
        _writeThrottler.flush();
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
        // Detach the previous tree's write handler so a stale tree still retained
        // elsewhere can no longer schedule saves against us.
        if (auto old = _liveSettings.get())
        {
            winrt::get_self<implementation::CascadiaSettings>(old)->SetWriteHandler(nullptr);
        }

        {
            std::scoped_lock lock{ _stateMutex };
            _generation.bump();
            _pendingSnapshot.clear();
            _expectedDiskHash = settings ? settings.Hash() : winrt::hstring{};
        }

        if (settings)
        {
            _liveSettings = winrt::make_weak(settings);

            auto self = winrt::get_self<implementation::CascadiaSettings>(settings);
            auto weakThis = get_weak();
            self->SetWriteHandler([weakThis]() {
                if (auto strong = weakThis.get())
                {
                    strong->RequestSave();
                }
            });
        }
        else
        {
            _liveSettings = nullptr;
        }
    }

    // Unbinds the live tree and disables auto-save (e.g. on load failure or
    // when falling back to the defaults settings object). Identical to binding a
    // null tree.
    void JsonManager::ClearLiveSettings()
    {
        SetLiveSettings(nullptr);
    }

    // Captures a snapshot of the bound tree (on the calling/owner thread) and
    // schedules a debounced write. Snapshotting here, rather than on the timer
    // thread, keeps all model-tree traversal on the owner thread.
    void JsonManager::RequestSave()
    {
        // A bound (non-null) tree is exactly what enables auto-save.
        const auto live = _liveSettings.get();
        if (!live)
        {
            return;
        }

        til::generation_t generation;
        {
            std::scoped_lock lock{ _stateMutex };
            generation = _generation;
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
            if (diskContent.empty())
            {
                // The file was deleted/emptied externally. Treat as a conflict so
                // we don't silently recreate it, unless we never had a baseline
                // (nothing to lose).
                conflict = !_expectedDiskHash.empty();
            }
            else
            {
                const auto diskHash = CalculateSettingsHash(diskContent, diskTime);
                // Disk diverged from our baseline, likely due to an external edit.
                // Disk is the source of truth, so drop our pending change.
                conflict = !_expectedDiskHash.empty() && diskHash != _expectedDiskHash;
            }

            if (!conflict)
            {
                _expectedDiskHash = _writeContent(snapshot);
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

    // Serializes a settings tree to JSON. Walks the model tree (ToJson), so it
    // must run on the owner thread.
    std::string JsonManager::_serialize(const Model::CascadiaSettings& settings)
    {
        auto self = winrt::get_self<implementation::CascadiaSettings>(settings);

        Json::StreamWriterBuilder wbuilder;
        wbuilder.settings_["enableYAMLCompatibility"] = true; // suppress spaces around colons
        wbuilder.settings_["indentation"] = "    ";
        wbuilder.settings_["precision"] = 6; // prevent values like 1.1000000000000001

        return Json::writeString(wbuilder, self->ToJson());
    }

    // Low-level write: atomically writes content to settings.json (with a .bak
    // backup) and returns the resulting content/time hash. Pure I/O -- it never
    // touches the model tree, so it is safe to call from the background timer
    // thread (the auto-save path) as well as from the owner thread.
    winrt::hstring JsonManager::_writeContent(std::string_view content)
    {
        const std::filesystem::path path{ std::wstring_view{ CascadiaSettings::SettingsPath() } };
        FILETIME newTime{};
        WriteSettingsFile(path, content, /*makeBackup*/ true, &newTime);
        return CalculateSettingsHash(content, newTime);
    }

    // Force-writes a tree to disk: serialize + atomic write + .bak + persist the
    // default-terminal choice. Returns the new hash (empty on failure). Static --
    // no instance and no auto-save baseline check. Used by load-time fixups, the
    // settings-editor clone save, and other explicit savers. Must run on the
    // thread that owns the tree (it reads tree state for the defterm persist).
    winrt::hstring JsonManager::WriteSettings(const Model::CascadiaSettings& settings)
    try
    {
        if (!settings)
        {
            return {};
        }

        const auto content = _serialize(settings);
        const auto hash = _writeContent(content);

        // Persist the default terminal choice (GH#10003: only when actually
        // initialized). Read the cached member directly -- the public getter
        // would trigger an expensive defterm refresh.
        auto self = winrt::get_self<implementation::CascadiaSettings>(settings);
        if (self->_currentDefaultTerminal)
        {
            DefaultTerminal::Current(self->_currentDefaultTerminal);
        }

        return hash;
    }
    catch (...)
    {
        LOG_CAUGHT_EXCEPTION();
        return {};
    }

    // Resets ApplicationState and writes the user-defaults template to disk.
    void JsonManager::ResetToDefaultSettings()
    {
        ApplicationState::SharedInstance().Reset();
        const std::filesystem::path path{ std::wstring_view{ CascadiaSettings::SettingsPath() } };
        WriteSettingsFile(path, LoadStringResource(IDR_USER_DEFAULTS), /*makeBackup*/ true);
    }
}
