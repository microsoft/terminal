// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "NewTabMenu.h"
#include "JsonUtils.h"
#include "RemainingProfilesEntry.h"

#include "NewTabMenu.g.cpp"

using namespace winrt::Microsoft::Terminal::Settings::Model;
using namespace winrt::Windows::Foundation::Collections;

namespace winrt::Microsoft::Terminal::Settings::Model::implementation
{
    com_ptr<NewTabMenu> NewTabMenu::Copy() const
    {
        auto copy{ make_self<NewTabMenu>() };
        copy->_json = _json;

        if (_entries)
        {
            std::vector<Model::NewTabMenuEntry> copies;
            copies.reserve((*_entries).Size());
            for (const auto& e : *_entries)
            {
                copies.push_back(winrt::get_self<implementation::NewTabMenuEntry>(e)->Copy());
            }
            copy->_entries = winrt::single_threaded_vector<Model::NewTabMenuEntry>(std::move(copies));
        }

        copy->_parents.reserve(_parents.size());
        for (const auto& parent : _parents)
        {
            copy->_parents.emplace_back(parent->Copy());
        }
        return copy;
    }

    com_ptr<NewTabMenu> NewTabMenu::FromJson(const Json::Value& json)
    {
        auto result = make_self<NewTabMenu>();
        result->LayerJson(json);
        return result;
    }

    void NewTabMenu::LayerJson(const Json::Value& json)
    {
        if (json.isNull() || !json.isArray())
        {
            // No newTabMenu at this layer; leave _entries as nullopt (inherit).
            return;
        }

        _json = json;

        // Deserialize the JSON array into the IVector backing.
        std::vector<Model::NewTabMenuEntry> entries;
        entries.reserve(json.size());
        for (const auto& entryJson : json)
        {
            if (auto entry = implementation::NewTabMenuEntry::FromJson(entryJson))
            {
                entries.push_back(*entry);
            }
        }
        _entries = winrt::single_threaded_vector<Model::NewTabMenuEntry>(std::move(entries));

        _emitChangeLog();
    }

    Json::Value NewTabMenu::ToJson() const
    {
        if (!_entries)
        {
            return Json::Value{ Json::nullValue };
        }
        return _json;
    }

    void NewTabMenu::ResolveMediaResourcesWithBasePath(const hstring& basePath,
                                                       const Model::MediaResourceResolver& resolver)
    {
        if (!_entries)
        {
            return;
        }
        for (const auto& entry : *_entries)
        {
            if (const auto resolvable{ entry.try_as<IPathlessMediaResourceContainer>() })
            {
                resolvable->ResolveMediaResourcesWithBasePath(basePath, resolver);
            }
        }
    }

    void NewTabMenu::LogSettingChanges(std::set<std::string>& changes, const std::string_view& context) const
    {
        for (const auto& setting : _changeLog)
        {
            changes.emplace(fmt::format(FMT_COMPILE("{}.{}"), context, setting));
        }
    }

    IVectorView<Model::NewTabMenuEntry> NewTabMenu::Entries()
    {
        return _resolvedEntries().GetView();
    }

    uint32_t NewTabMenu::Size()
    {
        return _resolvedEntries().Size();
    }

    Model::NewTabMenuEntry NewTabMenu::GetAt(uint32_t index)
    {
        return _resolvedEntries().GetAt(index);
    }

    void NewTabMenu::ClearUserOverride()
    {
        _entries = std::nullopt;
        _json = Json::Value{ Json::arrayValue };
        _NotifyWriteSettings();
    }

    void NewTabMenu::ReplaceAll(const IVector<Model::NewTabMenuEntry>& entries)
    {
        _entries = entries ? entries : winrt::single_threaded_vector<Model::NewTabMenuEntry>();
        _resyncJson();
    }

    void NewTabMenu::InsertEntryAt(uint32_t index, const Model::NewTabMenuEntry& entry)
    {
        _ensureLocal();
        _entries->InsertAt(index, entry);
        _resyncJson();
    }

    void NewTabMenu::RemoveEntryAt(uint32_t index)
    {
        _ensureLocal();
        _entries->RemoveAt(index);
        _resyncJson();
    }

    void NewTabMenu::SetEntryAt(uint32_t index, const Model::NewTabMenuEntry& entry)
    {
        _ensureLocal();
        _entries->SetAt(index, entry);
        _resyncJson();
    }

    void NewTabMenu::InsertEntryInFolder(const Model::FolderEntry& folder, uint32_t index, const Model::NewTabMenuEntry& entry)
    {
        _ensureLocal();
        if (_findFolderRawEntries(*_entries, folder, [&](const auto& raw) {
                raw.InsertAt(index, entry);
            }))
        {
            _resyncJson();
        }
        else
        {
            // Folder reference does not match any FolderEntry in the local tree.
            assert(false);
        }
    }

    void NewTabMenu::RemoveEntryFromFolder(const Model::FolderEntry& folder, uint32_t index)
    {
        _ensureLocal();
        if (_findFolderRawEntries(*_entries, folder, [&](const auto& raw) {
                raw.RemoveAt(index);
            }))
        {
            _resyncJson();
        }
        else
        {
            // Folder reference does not match any FolderEntry in the local tree.
            assert(false);
        }
    }

    void NewTabMenu::SetEntryInFolder(const Model::FolderEntry& folder, uint32_t index, const Model::NewTabMenuEntry& entry)
    {
        _ensureLocal();
        if (_findFolderRawEntries(*_entries, folder, [&](const auto& raw) {
                raw.SetAt(index, entry);
            }))
        {
            _resyncJson();
        }
        else
        {
            // Folder reference does not match any FolderEntry in the local tree.
            assert(false);
        }
    }

    void NewTabMenu::ReplaceFolderEntries(const Model::FolderEntry& folder, const IVector<Model::NewTabMenuEntry>& entries)
    {
        _ensureLocal();
        if (_findFolderRawEntries(*_entries, folder, [&](const auto& /*raw*/) {
                folder.RawEntries(entries ? entries : winrt::single_threaded_vector<Model::NewTabMenuEntry>());
            }))
        {
            _resyncJson();
        }
        else
        {
            // Folder reference does not match any FolderEntry in the local tree.
            assert(false);
        }
    }

    void NewTabMenu::UpdateFolderName(const Model::FolderEntry& folder, const hstring& name)
    {
        _ensureLocal();
        if (_folderInTree(*_entries, folder))
        {
            folder.Name(name);
            _resyncJson();
        }
        else
        {
            // Folder reference does not match any FolderEntry in the local tree.
            assert(false);
        }
    }

    void NewTabMenu::UpdateFolderIcon(const Model::FolderEntry& folder, const Model::IMediaResource& icon)
    {
        _ensureLocal();
        if (_folderInTree(*_entries, folder))
        {
            folder.Icon(icon);
            _resyncJson();
        }
        else
        {
            // Folder reference does not match any FolderEntry in the local tree.
            assert(false);
        }
    }

    void NewTabMenu::UpdateFolderAllowEmpty(const Model::FolderEntry& folder, bool value)
    {
        _ensureLocal();
        if (_folderInTree(*_entries, folder))
        {
            folder.AllowEmpty(value);
            _resyncJson();
        }
        else
        {
            // Folder reference does not match any FolderEntry in the local tree.
            assert(false);
        }
    }

    void NewTabMenu::UpdateFolderInlining(const Model::FolderEntry& folder, Model::FolderEntryInlining value)
    {
        _ensureLocal();
        if (_folderInTree(*_entries, folder))
        {
            folder.Inlining(value);
            _resyncJson();
        }
        else
        {
            // Folder reference does not match any FolderEntry in the local tree.
            assert(false);
        }
    }

    // Walks the tree and updates ActionEntry.ActionId in place wherever it
    // matches `oldID`. Used by GlobalAppSettings::UpdateCommandID when an
    // ActionMap rename needs to propagate into the menu.
    void NewTabMenu::RemapActionIds(const hstring& oldID, const hstring& newID)
    {
        if (!_entries)
        {
            return;
        }
        bool anyUpdated = false;
        std::function<void(const Model::NewTabMenuEntry&)> visit;
        visit = [&](const Model::NewTabMenuEntry& entry) {
            if (entry.Type() == Model::NewTabMenuEntryType::Action)
            {
                if (const auto actionEntry{ entry.try_as<Model::ActionEntry>() })
                {
                    if (actionEntry.ActionId() == oldID)
                    {
                        actionEntry.ActionId(newID);
                        anyUpdated = true;
                    }
                }
            }
            else if (entry.Type() == Model::NewTabMenuEntryType::Folder)
            {
                if (const auto folderEntry{ entry.try_as<Model::FolderEntry>() })
                {
                    if (const auto raw = folderEntry.RawEntries())
                    {
                        for (const auto& nested : raw)
                        {
                            visit(nested);
                        }
                    }
                }
            }
        };
        for (const auto& entry : *_entries)
        {
            visit(entry);
        }
        if (anyUpdated)
        {
            _resyncJson();
        }
    }

    std::optional<IVector<Model::NewTabMenuEntry>> NewTabMenu::_findOverriddenEntries() const
    {
        if (_entries)
        {
            return _entries;
        }
        for (const auto& parent : _parents)
        {
            if (auto val{ parent->_findOverriddenEntries() })
            {
                return val;
            }
        }
        return std::nullopt;
    }

    IVector<Model::NewTabMenuEntry> NewTabMenu::_resolvedEntries() const
    {
        if (const auto val = _findOverriddenEntries())
        {
            return *val;
        }
        return winrt::single_threaded_vector<Model::NewTabMenuEntry>({ Model::RemainingProfilesEntry{} });
    }

    void NewTabMenu::_ensureLocal()
    {
        if (_entries)
        {
            return;
        }
        const auto inherited = _resolvedEntries();
        std::vector<Model::NewTabMenuEntry> copies;
        copies.reserve(inherited.Size());
        for (const auto& e : inherited)
        {
            copies.push_back(winrt::get_self<implementation::NewTabMenuEntry>(e)->Copy());
        }
        _entries = winrt::single_threaded_vector<Model::NewTabMenuEntry>(std::move(copies));
    }

    void NewTabMenu::_resyncJson()
    {
        if (!_entries)
        {
            _json = Json::Value{ Json::arrayValue };
            return;
        }
        Json::Value arr{ Json::arrayValue };
        for (const auto& entry : *_entries)
        {
            arr.append(winrt::get_self<implementation::NewTabMenuEntry>(entry)->ToJson());
        }
        _json = std::move(arr);
        _emitChangeLog();
        _NotifyWriteSettings();
    }

    void NewTabMenu::_emitChangeLog()
    {
        if (!_entries)
        {
            return;
        }

        // Exclude false positives where this layer's newTabMenu is just the
        // default "[{type: remainingProfiles}]".
        if (_entries->Size() == 1 && _entries->GetAt(0).Type() == NewTabMenuEntryType::RemainingProfiles)
        {
            return;
        }

        for (const auto& entry : *_entries)
        {
            std::string entryType;
            switch (entry.Type())
            {
            case NewTabMenuEntryType::Profile:
                entryType = "profile";
                break;
            case NewTabMenuEntryType::Separator:
                entryType = "separator";
                break;
            case NewTabMenuEntryType::Folder:
                entryType = "folder";
                break;
            case NewTabMenuEntryType::RemainingProfiles:
                entryType = "remainingProfiles";
                break;
            case NewTabMenuEntryType::MatchProfiles:
                entryType = "matchProfiles";
                break;
            case NewTabMenuEntryType::Action:
                entryType = "action";
                break;
            case NewTabMenuEntryType::Invalid:
            default:
                continue;
            }
            _changeLog.emplace(std::move(entryType));
        }
    }

    bool NewTabMenu::_findFolderRawEntries(
        const IVector<Model::NewTabMenuEntry>& root,
        const Model::FolderEntry& target,
        std::function<void(const IVector<Model::NewTabMenuEntry>&)> callback)
    {
        if (!root)
        {
            return false;
        }
        for (const auto& entry : root)
        {
            if (entry.Type() != Model::NewTabMenuEntryType::Folder)
            {
                continue;
            }
            const auto folder = entry.as<Model::FolderEntry>();
            if (folder == target)
            {
                auto raw = folder.RawEntries();
                if (!raw)
                {
                    raw = winrt::single_threaded_vector<Model::NewTabMenuEntry>();
                    folder.RawEntries(raw);
                }
                callback(raw);
                return true;
            }
            if (auto raw = folder.RawEntries())
            {
                if (_findFolderRawEntries(raw, target, callback))
                {
                    return true;
                }
            }
        }
        return false;
    }

    bool NewTabMenu::_folderInTree(
        const IVector<Model::NewTabMenuEntry>& root,
        const Model::FolderEntry& target)
    {
        if (!root)
        {
            return false;
        }
        for (const auto& entry : root)
        {
            if (entry.Type() != Model::NewTabMenuEntryType::Folder)
            {
                continue;
            }
            const auto folder = entry.as<Model::FolderEntry>();
            if (folder == target)
            {
                return true;
            }
            if (auto raw = folder.RawEntries())
            {
                if (_folderInTree(raw, target))
                {
                    return true;
                }
            }
        }
        return false;
    }
}
