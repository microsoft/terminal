/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- NewTabMenu.h

Abstract:
- Owns the entries of the new tab menu dropdown, the underlying JSON storage, and
  the mutation APIs.

Author(s):
- Carlos Zamora - June 2026

--*/
#pragma once

#include "NewTabMenu.g.h"
#include "IInheritable.h"
#include "FolderEntry.h"
#include "NewTabMenuEntry.h"
#include "MediaResourceSupport.h"

namespace winrt::Microsoft::Terminal::Settings::Model::implementation
{
    struct NewTabMenu : NewTabMenuT<NewTabMenu>, IInheritable<NewTabMenu>
    {
    public:
        NewTabMenu() = default;

        void _FinalizeInheritance() override {}
        com_ptr<NewTabMenu> Copy() const;

        static com_ptr<NewTabMenu> FromJson(const Json::Value& json);
        void LayerJson(const Json::Value& json);
        Json::Value ToJson() const;

        void ResolveMediaResourcesWithBasePath(const hstring& basePath,
                                               const Model::MediaResourceResolver& resolver);

        void LogSettingChanges(std::set<std::string>& changes, const std::string_view& context) const;

        Windows::Foundation::Collections::IVectorView<Model::NewTabMenuEntry> Entries();
        uint32_t Size();
        Model::NewTabMenuEntry GetAt(uint32_t index);

        void InsertEntryAt(uint32_t index, const Model::NewTabMenuEntry& entry);
        void RemoveEntryAt(uint32_t index);
        void SetEntryAt(uint32_t index, const Model::NewTabMenuEntry& entry);
        void ReplaceAll(const Windows::Foundation::Collections::IVector<Model::NewTabMenuEntry>& entries);

        void InsertEntryInFolder(const Model::FolderEntry& folder, uint32_t index, const Model::NewTabMenuEntry& entry);
        void RemoveEntryFromFolder(const Model::FolderEntry& folder, uint32_t index);
        void SetEntryInFolder(const Model::FolderEntry& folder, uint32_t index, const Model::NewTabMenuEntry& entry);
        void ReplaceFolderEntries(const Model::FolderEntry& folder, const Windows::Foundation::Collections::IVector<Model::NewTabMenuEntry>& entries);

        void UpdateFolderName(const Model::FolderEntry& folder, const hstring& name);
        void UpdateFolderIcon(const Model::FolderEntry& folder, const Model::IMediaResource& icon);
        void UpdateFolderAllowEmpty(const Model::FolderEntry& folder, bool value);
        void UpdateFolderInlining(const Model::FolderEntry& folder, Model::FolderEntryInlining value);

        bool HasUserOverride() const noexcept { return _entries.has_value(); }
        void ClearUserOverride();

        void RemapActionIds(const hstring& oldID, const hstring& newID);

    private:
        // Resolved entries for this layer. nullopt = inherit from parent.
        std::optional<Windows::Foundation::Collections::IVector<Model::NewTabMenuEntry>> _entries;
        Json::Value _json{ Json::ValueType::arrayValue };
        std::set<std::string> _changeLog;

        std::optional<Windows::Foundation::Collections::IVector<Model::NewTabMenuEntry>> _findOverriddenEntries() const;
        Windows::Foundation::Collections::IVector<Model::NewTabMenuEntry> _resolvedEntries() const;

        void _ensureLocal();
        void _resyncJson();
        void _emitChangeLog();

        static bool _findFolderRawEntries(
            const Windows::Foundation::Collections::IVector<Model::NewTabMenuEntry>& root,
            const Model::FolderEntry& target,
            std::function<void(const Windows::Foundation::Collections::IVector<Model::NewTabMenuEntry>&)> callback);

        static bool _folderInTree(
            const Windows::Foundation::Collections::IVector<Model::NewTabMenuEntry>& root,
            const Model::FolderEntry& target);
    };
}
