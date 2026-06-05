/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- JsonManager.h

Abstract:
- Owns settings.json persistence and file-change watching, split out of
  CascadiaSettings as part of the Auto-Save effort.
- Ownership: an instance is owned by the app (AppLogic), NOT a process singleton.
  The decision of "when NOT to save" lives with AppLogic — it binds the live tree
  via SetLiveSettings() ONLY after a successful load, and never binds the
  LoadDefaults() fallback, so we never overwrite a user's broken settings.json
  with defaults. There is one AppLogic per process, so an instance is already
  process-scoped; instance ownership also gives clean lifetime + testability that
  a singleton holding the live model tree would not.
- The bound tree is held weakly and guarded by a generation token so a
  scheduled write can never run against a superseded tree.
- Threading: the agile weak_ref is touched only on the owner (UI) thread, because
  the model *tree* it points at is not thread-safe. Serialization is snapshotted
  on the owner thread (RequestSave) and only the resulting immutable byte snapshot
  crosses to the background debounce-timer / file-I/O thread.
- The static WriteSettings()/ResetToDefaultSettings() are force-writes callable
  without an instance (used by load-time fixups, the editor clone save, and other
  explicit savers). The instance auto-save path is the only debounced one and is
  the only path that performs the baseline lost-update conflict check.

Author(s):
- Carlos Zamora - June 2026

--*/
#pragma once

#include "JsonManager.g.h"

#include <inc/cppwinrt_utils.h>
#include <til/throttled_func.h>
#include <til/generational.h>
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

        // Force-write the given tree to disk (serialize + atomic write + .bak +
        // persist default-terminal choice). Returns the new content/time hash, or
        // an empty string on failure. Static so it works without an instance:
        // used by load-time fixups, the settings-editor clone save, and other
        // explicit savers. Does NOT perform the auto-save baseline conflict check.
        static winrt::hstring WriteSettings(const Model::CascadiaSettings& settings);

        // Reset ApplicationState and write the user-defaults template to disk.
        static void ResetToDefaultSettings();

        TYPED_EVENT(SettingsChangedExternally, Model::JsonManager, Windows::Foundation::IInspectable);

    private:
        // The live settings tree, bound only after a successful load. Held
        // weakly so JsonManager never keeps a superseded/failed tree alive.
        // Only touched on the owner (UI) thread (see Threading note above). A
        // bound (non-null) tree is exactly what enables auto-save.
        winrt::weak_ref<Model::CascadiaSettings> _liveSettings{ nullptr };

        // Bumped on every (Set|Clear)LiveSettings so a write scheduled against
        // a previous tree can detect that it is stale and bail out.
        til::generation_t _generation;

        // The folder watcher over the settings directory. Created in Start(),
        // torn down in Stop(). Its callback runs on a background thread.
        wil::unique_folder_change_reader_nothrow _watcher;

        // State shared between the owner thread, the debounce timer thread, and
        // the watcher thread. Guarded by _stateMutex.
        std::mutex _stateMutex;
        std::string _pendingSnapshot;
        // The generation the pending snapshot was captured under. If the live
        // tree is swapped (reload) before the debounced write fires, the
        // generation no longer matches and the stale snapshot is dropped.
        til::generation_t _pendingSnapshotGeneration;
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
        static winrt::hstring _writeContent(std::string_view content);
    };
}

namespace winrt::Microsoft::Terminal::Settings::Model::factory_implementation
{
    BASIC_FACTORY(JsonManager);
}
