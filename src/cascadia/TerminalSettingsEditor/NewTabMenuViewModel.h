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

        Windows::Foundation::Collections::IObservableVector<Model::Profile> AvailableProfiles() { return _Settings.AllProfiles(); }

        void RequestAddSelectedProfileEntry();
        void RequestAddSeparatorEntry();
        void RequestAddRemainingProfilesEntry();

        VIEW_MODEL_OBSERVABLE_PROPERTY(Windows::Foundation::Collections::IObservableVector<Editor::NewTabMenuEntryViewModel>, Entries);
        VIEW_MODEL_OBSERVABLE_PROPERTY(Model::Profile, SelectedProfile, nullptr);
        VIEW_MODEL_OBSERVABLE_PROPERTY(bool, IsMissingRemainingProfilesEntry, false);

    private:
        Model::CascadiaSettings _Settings;
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
        SeparatorEntryViewModel();
    };

    struct FolderEntryViewModel : FolderEntryViewModelT<FolderEntryViewModel, NewTabMenuEntryViewModel>
    {
    public:
        FolderEntryViewModel(Model::FolderEntry folderEntry);

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
        RemainingProfilesEntryViewModel();
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
