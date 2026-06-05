/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- JsonManager.h

Abstract:
- Owns settings.json persistence and file-change watching, split out of
  CascadiaSettings as part of the Auto-Save effort.
- An instance is owned by the app (AppLogic), not a singleton. AppLogic binds
  the live settings tree via SetLiveSettings() ONLY after a successful load,
  and disables auto-save on load failure / defaults fallback so we never
  overwrite a user's broken settings.json with defaults.
- The bound tree is held weakly and guarded by a generation token so a
  scheduled write can never run against a superseded tree.

Author(s):
- Carlos Zamora - June 2026

--*/
#pragma once

#include "JsonManager.g.h"

#include <inc/cppwinrt_utils.h>
#include <til/throttled_func.h>
#include <wil/filesystem.h>
#include <mutex>

namespace winrt::Microsoft::Terminal::Settings::Model::implementation
{
    struct JsonManager : JsonManagerT<JsonManager>
    {
        JsonManager();
        ~JsonManager();

        void Start();
        void Stop();

        void SetLiveSettings(const Model::CascadiaSettings& settings);
        void ClearLiveSettings();

        void RequestSave();
        bool Save();

        TYPED_EVENT(SettingsChangedExternally, Model::JsonManager, Windows::Foundation::IInspectable);

    private:
        // The live settings tree, bound only after a successful load. Held
        // weakly so JsonManager never keeps a superseded/failed tree alive.
        // Only touched on the owner (UI) thread.
        winrt::weak_ref<Model::CascadiaSettings> _liveSettings{ nullptr };

        // Bumped on every (Set|Clear)LiveSettings so a write scheduled against
        // a previous tree can detect that it is stale and bail out.
        uint64_t _generation{ 0 };

        // The folder watcher over the settings directory. Created in Start(),
        // torn down in Stop(). Its callback runs on a background thread.
        wil::unique_folder_change_reader_nothrow _watcher;

        // State shared between the owner thread, the debounce timer thread, and
        // the watcher thread. Guarded by _stateMutex.
        std::mutex _stateMutex;
        bool _autoSaveEnabled{ false };
        std::string _pendingSnapshot;
        // The generation the pending snapshot was captured under. If the live
        // tree is swapped (reload) before the debounced write fires, the
        // generation no longer matches and the stale snapshot is dropped.
        uint64_t _pendingSnapshotGeneration{ 0 };
        // The hash we expect settings.json to currently have on disk. Used to
        // distinguish our own writes from external edits, and as the baseline
        // for the lost-update conflict check.
        winrt::hstring _expectedDiskHash;

        // Timer-based debounce (no UI DispatcherQueue dependency). The callback
        // only writes the already-captured snapshot. Declared last so it is
        // destroyed first (its pending callback only touches other members,
        // which therefore still exist at that point).
        til::throttled_func<> _writeThrottler;

        void _write() noexcept;
        void _onFileChanged() noexcept;
        bool _persistSnapshot(const std::string& snapshot) noexcept;
        static std::string _serialize(const Model::CascadiaSettings& settings);
    };
}

namespace winrt::Microsoft::Terminal::Settings::Model::factory_implementation
{
    BASIC_FACTORY(JsonManager);
}
