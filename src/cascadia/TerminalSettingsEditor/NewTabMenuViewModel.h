// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "NewTabMenuViewModel.g.h"
#include "FolderTreeViewEntry.g.h"
#include "NewTabMenuEntryViewModel.g.h"
#include "ProfileEntryViewModel.g.h"
#include "SeparatorEntryViewModel.g.h"
#include "FolderEntryViewModel.g.h"
#include "MatchProfilesEntryViewModel.g.h"
#include "RemainingProfilesEntryViewModel.g.h"

#include "ProfileViewModel.h"
#include "ViewModelHelpers.h"
#include "Utils.h"

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    struct NewTabMenuViewModel : NewTabMenuViewModelT<NewTabMenuViewModel>, ViewModelHelper<NewTabMenuViewModel>
    {
    public:
        NewTabMenuViewModel(Model::CascadiaSettings settings);
        void UpdateSettings(const Model::CascadiaSettings& settings);

        bool IsRemainingProfilesEntryMissing() const;
        bool IsFolderView() const noexcept;

        void RequestReorderEntry(const Editor::NewTabMenuEntryViewModel& vm, bool goingUp);
        void RequestDeleteEntry(const Editor::NewTabMenuEntryViewModel& vm);
        void RequestMoveEntriesToFolder(const Windows::Foundation::Collections::IVector<Editor::NewTabMenuEntryViewModel>& entries, const Editor::FolderEntryViewModel& destinationFolder);

        void RequestAddSelectedProfileEntry();
        void RequestAddSeparatorEntry();
        void RequestAddFolderEntry();
        void RequestAddProfileMatcherEntry();
        void RequestAddRemainingProfilesEntry();

        hstring CurrentFolderName() const;
        void CurrentFolderName(const hstring& value);
        bool CurrentFolderInlining() const;
        void CurrentFolderInlining(bool value);
        bool CurrentFolderAllowEmpty() const;
        void CurrentFolderAllowEmpty(bool value);

        Windows::Foundation::Collections::IObservableVector<Model::Profile> AvailableProfiles() { return _Settings.AllProfiles(); }
        Windows::Foundation::Collections::IObservableVector<Editor::FolderTreeViewEntry> FolderTree();
        Windows::Foundation::Collections::IObservableVector<Editor::NewTabMenuEntryViewModel> CurrentView() const;
        VIEW_MODEL_OBSERVABLE_PROPERTY(Editor::FolderEntryViewModel, CurrentFolder, nullptr);
        VIEW_MODEL_OBSERVABLE_PROPERTY(Editor::FolderTreeViewEntry, CurrentFolderTreeViewSelectedItem, nullptr);

        // Bound to the UI to create new entries
        VIEW_MODEL_OBSERVABLE_PROPERTY(Model::Profile, SelectedProfile, nullptr);
        VIEW_MODEL_OBSERVABLE_PROPERTY(hstring, ProfileMatcherName);
        VIEW_MODEL_OBSERVABLE_PROPERTY(hstring, ProfileMatcherSource);
        VIEW_MODEL_OBSERVABLE_PROPERTY(hstring, ProfileMatcherCommandline);
        VIEW_MODEL_OBSERVABLE_PROPERTY(hstring, AddFolderName);

    private:
        Model::CascadiaSettings _Settings{ nullptr };
        Windows::Foundation::Collections::IObservableVector<Editor::NewTabMenuEntryViewModel> _rootEntries;
        Windows::Foundation::Collections::IObservableVector<Editor::NewTabMenuEntryViewModel>::VectorChanged_revoker _rootEntriesChangedRevoker;

        Windows::Foundation::Collections::IObservableVector<Editor::FolderTreeViewEntry> _folderTreeCache;
        Windows::Foundation::Collections::IObservableVector<Editor::NewTabMenuEntryViewModel>::VectorChanged_revoker _CurrentViewChangedRevoker;

        static bool _IsRemainingProfilesEntryMissing(const Windows::Foundation::Collections::IVector<Editor::NewTabMenuEntryViewModel>& entries);

        void _PrintAll();
#ifdef _DEBUG
        void _PrintModel(Windows::Foundation::Collections::IVector<Model::NewTabMenuEntry> list, std::wstring prefix = L"");
        void _PrintModel(const Model::NewTabMenuEntry& e, std::wstring prefix = L"");
        void _PrintVM(Windows::Foundation::Collections::IVector<Editor::NewTabMenuEntryViewModel> list, std::wstring prefix = L"");
        void _PrintVM(const Editor::NewTabMenuEntryViewModel& vm, std::wstring prefix = L"");
#endif
    };

    struct FolderTreeViewEntry : FolderTreeViewEntryT<FolderTreeViewEntry>
    {
    public:
        FolderTreeViewEntry(Editor::FolderEntryViewModel folderEntry);

        hstring Name();
        hstring Icon();
        Editor::FolderEntryViewModel FolderEntryVM() { return _folderEntry; }

        WINRT_PROPERTY(Windows::Foundation::Collections::IObservableVector<Microsoft::Terminal::Settings::Editor::FolderTreeViewEntry>, Children);

    private:
        Editor::FolderEntryViewModel _folderEntry;
    };

    struct NewTabMenuEntryViewModel : NewTabMenuEntryViewModelT<NewTabMenuEntryViewModel>, ViewModelHelper<NewTabMenuEntryViewModel>
    {
    public:
        static Model::NewTabMenuEntry GetModel(const Editor::NewTabMenuEntryViewModel& viewModel);
        VIEW_MODEL_OBSERVABLE_PROPERTY(Model::NewTabMenuEntryType, Type, Model::NewTabMenuEntryType::Invalid);

    protected:
        explicit NewTabMenuEntryViewModel(const Model::NewTabMenuEntryType type) noexcept;
    };

    struct ProfileEntryViewModel : ProfileEntryViewModelT<ProfileEntryViewModel, NewTabMenuEntryViewModel>
    {
    public:
        ProfileEntryViewModel(Model::ProfileEntry profileEntry);

        VIEW_MODEL_OBSERVABLE_PROPERTY(Model::ProfileEntry, ProfileEntry, nullptr);
    };

    struct SeparatorEntryViewModel : SeparatorEntryViewModelT<SeparatorEntryViewModel, NewTabMenuEntryViewModel>
    {
    public:
        SeparatorEntryViewModel(Model::SeparatorEntry separatorEntry);

        VIEW_MODEL_OBSERVABLE_PROPERTY(Model::SeparatorEntry, SeparatorEntry, nullptr);
    };

    struct FolderEntryViewModel : FolderEntryViewModelT<FolderEntryViewModel, NewTabMenuEntryViewModel>
    {
    public:
        FolderEntryViewModel(Model::FolderEntry folderEntry);

        bool Inlining() const;
        void Inlining(bool value);
        GETSET_OBSERVABLE_PROJECTED_SETTING(_FolderEntry, Name);
        GETSET_OBSERVABLE_PROJECTED_SETTING(_FolderEntry, Icon);
        GETSET_OBSERVABLE_PROJECTED_SETTING(_FolderEntry, AllowEmpty);

        VIEW_MODEL_OBSERVABLE_PROPERTY(Windows::Foundation::Collections::IObservableVector<Editor::NewTabMenuEntryViewModel>, Entries);
        VIEW_MODEL_OBSERVABLE_PROPERTY(Model::FolderEntry, FolderEntry, nullptr);

    private:
        Windows::Foundation::Collections::IObservableVector<Editor::NewTabMenuEntryViewModel>::VectorChanged_revoker _entriesChangedRevoker;
    };

    struct MatchProfilesEntryViewModel : MatchProfilesEntryViewModelT<MatchProfilesEntryViewModel, NewTabMenuEntryViewModel>
    {
    public:
        MatchProfilesEntryViewModel(Model::MatchProfilesEntry matchProfilesEntry);

        hstring DisplayText() const;
        VIEW_MODEL_OBSERVABLE_PROPERTY(Model::MatchProfilesEntry, MatchProfilesEntry, nullptr);
    };

    struct RemainingProfilesEntryViewModel : RemainingProfilesEntryViewModelT<RemainingProfilesEntryViewModel, NewTabMenuEntryViewModel>
    {
    public:
        RemainingProfilesEntryViewModel(Model::RemainingProfilesEntry remainingProfielsEntry);

        VIEW_MODEL_OBSERVABLE_PROPERTY(Model::RemainingProfilesEntry, RemainingProfilesEntry, nullptr);
    };
};

namespace winrt::Microsoft::Terminal::Settings::Editor::factory_implementation
{
    BASIC_FACTORY(NewTabMenuViewModel);
    BASIC_FACTORY(FolderTreeViewEntry);
    BASIC_FACTORY(ProfileEntryViewModel);
    BASIC_FACTORY(SeparatorEntryViewModel);
    BASIC_FACTORY(FolderEntryViewModel);
    BASIC_FACTORY(MatchProfilesEntryViewModel);
    BASIC_FACTORY(RemainingProfilesEntryViewModel);
}
