// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "NewTabMenuViewModel.g.h"
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

        static bool IsRemainingProfilesEntryMissing(const Windows::Foundation::Collections::IObservableVector<Editor::NewTabMenuEntryViewModel>& entries);
        bool IsFolderView() const noexcept;

        void UpdateSettings(Model::CascadiaSettings settings);

        void RequestReorderEntry(Editor::NewTabMenuEntryViewModel vm, bool goingUp);
        void RequestDeleteEntry(Editor::NewTabMenuEntryViewModel vm);

        void RequestAddSelectedProfileEntry();
        void RequestAddSeparatorEntry();
        void RequestAddFolderEntry();
        void RequestAddProfileMatcherEntry();
        void RequestAddRemainingProfilesEntry();

        Windows::Foundation::Collections::IObservableVector<Model::Profile> AvailableProfiles() { return _Settings.AllProfiles(); }
        VIEW_MODEL_OBSERVABLE_PROPERTY(Windows::Foundation::Collections::IObservableVector<Editor::NewTabMenuEntryViewModel>, Entries);
        VIEW_MODEL_OBSERVABLE_PROPERTY(Model::Profile, SelectedProfile, nullptr);

        VIEW_MODEL_OBSERVABLE_PROPERTY(hstring, ProfileMatcherName);
        VIEW_MODEL_OBSERVABLE_PROPERTY(hstring, ProfileMatcherSource);
        VIEW_MODEL_OBSERVABLE_PROPERTY(hstring, ProfileMatcherCommandline);

        VIEW_MODEL_OBSERVABLE_PROPERTY(hstring, FolderName);

        VIEW_MODEL_OBSERVABLE_PROPERTY(NTMSubPage, CurrentPage);
        VIEW_MODEL_OBSERVABLE_PROPERTY(Editor::FolderEntryViewModel, CurrentFolderEntry, nullptr);

    private:
        Model::CascadiaSettings _Settings{ nullptr };
        Windows::Foundation::Collections::IObservableVector<Editor::NewTabMenuEntryViewModel>::VectorChanged_revoker _entriesChangedRevoker;

        void _UpdateEntries();

        void _PrintAll();
#ifdef _DEBUG
        void _PrintModel(Windows::Foundation::Collections::IVector<Model::NewTabMenuEntry> list, std::wstring prefix = L"");
        void _PrintModel(const Model::NewTabMenuEntry& e, std::wstring prefix = L"");
        void _PrintVM(Windows::Foundation::Collections::IVector<Editor::NewTabMenuEntryViewModel> list, std::wstring prefix = L"");
        void _PrintVM(const Editor::NewTabMenuEntryViewModel& vm, std::wstring prefix = L"");
#endif
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

        GETSET_OBSERVABLE_PROJECTED_SETTING(_FolderEntry, Name);
        GETSET_OBSERVABLE_PROJECTED_SETTING(_FolderEntry, Icon);

        VIEW_MODEL_OBSERVABLE_PROPERTY(Windows::Foundation::Collections::IObservableVector<Editor::NewTabMenuEntryViewModel>, Entries);
        VIEW_MODEL_OBSERVABLE_PROPERTY(Model::FolderEntry, FolderEntry, nullptr);
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
    BASIC_FACTORY(ProfileEntryViewModel);
    BASIC_FACTORY(SeparatorEntryViewModel);
    BASIC_FACTORY(FolderEntryViewModel);
    BASIC_FACTORY(MatchProfilesEntryViewModel);
    BASIC_FACTORY(RemainingProfilesEntryViewModel);
}
