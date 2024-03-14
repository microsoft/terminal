// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "NewTabMenuViewModel.h"
#include <LibraryResources.h>

#include "NewTabMenuViewModel.g.cpp"
#include "NewTabMenuEntryViewModel.g.cpp"
#include "ProfileEntryViewModel.g.cpp"
#include "SeparatorEntryViewModel.g.cpp"
#include "FolderEntryViewModel.g.cpp"
#include "MatchProfilesEntryViewModel.g.cpp"
#include "RemainingProfilesEntryViewModel.g.cpp"

using namespace winrt::Windows::UI::Xaml::Navigation;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Foundation::Collections;
using namespace winrt::Microsoft::Terminal::Settings::Model;
using namespace winrt::Windows::UI::Xaml::Data;

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    static IObservableVector<Editor::NewTabMenuEntryViewModel> _ConvertToViewModelEntries(IVector<Model::NewTabMenuEntry> settingsModelEntries)
    {
        auto result = single_threaded_observable_vector<Editor::NewTabMenuEntryViewModel>();
        for (const auto& entry : settingsModelEntries)
        {
            switch (entry.Type())
            {
            case NewTabMenuEntryType::Profile:
            {
                // If the Profile isn't set, this is an invalid entry. Skip it.
                if (const auto profileEntry = entry.as<Model::ProfileEntry>(); profileEntry.Profile())
                {
                    const auto profileEntryVM = make<ProfileEntryViewModel>(profileEntry);
                    result.Append(profileEntryVM);
                }
                break;
            }
            case NewTabMenuEntryType::Separator:
            {
                const auto separatorEntryVM = make<SeparatorEntryViewModel>();
                result.Append(separatorEntryVM);
                break;
            }
            case NewTabMenuEntryType::Folder:
            {
                const auto folderEntry = entry.as<Model::FolderEntry>();
                const auto folderEntryVM = make<FolderEntryViewModel>(folderEntry);
                result.Append(folderEntryVM);
                break;
            }
            case NewTabMenuEntryType::MatchProfiles:
            {
                const auto matchProfilesEntry = entry.as<Model::MatchProfilesEntry>();
                const auto matchProfilesEntryVM = make<MatchProfilesEntryViewModel>(matchProfilesEntry);
                result.Append(matchProfilesEntryVM);
                break;
            }
            case NewTabMenuEntryType::RemainingProfiles:
            {
                const auto remainingProfilesEntryVM = make<RemainingProfilesEntryViewModel>();
                result.Append(remainingProfilesEntryVM);
                break;
            }
            case NewTabMenuEntryType::Invalid:
            default:
                break;
            }
        }
        return result;
    }

    static bool _IsRemainingProfilesEntryMissing(const IObservableVector<Editor::NewTabMenuEntryViewModel>& entries)
    {
        for (const auto& entry : entries)
        {
            switch (entry.Type())
            {
            case NewTabMenuEntryType::RemainingProfiles:
            {
                return false;
            }
            case NewTabMenuEntryType::Folder:
            {
                if (!_IsRemainingProfilesEntryMissing(entry.as<Editor::FolderEntryViewModel>().Entries()))
                {
                    return false;
                }
            }
            default:
                break;
            }
        }
        return true;
    };

    NewTabMenuViewModel::NewTabMenuViewModel(Model::CascadiaSettings settings) :
        _Settings{ settings }
    {
        const auto newTabMenuEntries = _Settings.GlobalSettings().NewTabMenu();
        _Entries = _ConvertToViewModelEntries(newTabMenuEntries);
        _IsMissingRemainingProfilesEntry = _IsRemainingProfilesEntryMissing(_Entries);
        _SelectedProfile = AvailableProfiles().GetAt(0);

        // Add a property changed handler to our own property changed event.
        // This propagates changes from the settings model to anybody listening to our
        // unique view model members.
        //PropertyChanged([this](auto&&, const PropertyChangedEventArgs& args) {
        //    const auto viewModelProperty{ args.PropertyName() };
        //    if (viewModelProperty == L"CenterOnNewTabMenu")
        //    {
        //        _NotifyChanges(L"NewTabMenuParametersCurrentValue");
        //    }
        //});

        _Entries.VectorChanged([this](auto&&, const IVectorChangedEventArgs& args) {
            switch (args.CollectionChange())
            {
            case CollectionChange::Reset:
            {
                // fully replace settings model with _Entries
                for (const auto& entry : _Entries)
                {
                    auto modelEntries = single_threaded_vector<Model::NewTabMenuEntry>();
                    modelEntries.Append(NewTabMenuEntryViewModel::GetModel(entry));

                    _Settings.GlobalSettings().NewTabMenu(modelEntries);
                }
                return;
            }
            case CollectionChange::ItemInserted:
            {
                const auto insertedEntry = _Entries.GetAt(args.Index());
                if (insertedEntry.Type() == NewTabMenuEntryType::RemainingProfiles)
                {
                    _IsMissingRemainingProfilesEntry = false;
                }

                auto newTabMenu = _Settings.GlobalSettings().NewTabMenu();
                newTabMenu.InsertAt(args.Index(), NewTabMenuEntryViewModel::GetModel(insertedEntry));
                return;
            }
            case CollectionChange::ItemRemoved:
            {
                auto newTabMenu = _Settings.GlobalSettings().NewTabMenu();
                newTabMenu.RemoveAt(args.Index());
                return;
            }
            case CollectionChange::ItemChanged:
            {
                auto newTabMenu = _Settings.GlobalSettings().NewTabMenu();
                const auto modifiedEntry = _Entries.GetAt(args.Index());
                newTabMenu.SetAt(args.Index(), NewTabMenuEntryViewModel::GetModel(modifiedEntry));
                return;
            }
            }
        });
    }

    void NewTabMenuViewModel::RequestAddSelectedProfileEntry()
    {
        if (_SelectedProfile)
        {
            const Model::ProfileEntry profileEntry{ winrt::to_hstring(_SelectedProfile.Guid()) };
            const auto profileEntryVM = make<ProfileEntryViewModel>(profileEntry);
            _Entries.Append(profileEntryVM);
        }
    }

    void NewTabMenuViewModel::RequestAddSeparatorEntry()
    {
        _Entries.Append(make<SeparatorEntryViewModel>());
    }

    void NewTabMenuViewModel::RequestAddRemainingProfilesEntry()
    {
        _Entries.Append(make<RemainingProfilesEntryViewModel>());
        _IsMissingRemainingProfilesEntry = false;
    }

    NewTabMenuEntryViewModel::NewTabMenuEntryViewModel(const NewTabMenuEntryType type) noexcept :
        _Type{ type }
    {
    }

    Model::NewTabMenuEntry NewTabMenuEntryViewModel::GetModel(const Editor::NewTabMenuEntryViewModel& viewModel)
    {
        switch (viewModel.Type())
        {
        case NewTabMenuEntryType::Profile:
        {
            return viewModel.as<Editor::ProfileEntryViewModel>().ProfileEntry();
        }
        case NewTabMenuEntryType::Separator:
        {
            return Model::SeparatorEntry{};
        }
        case NewTabMenuEntryType::Folder:
        {
            return viewModel.as<Editor::FolderEntryViewModel>().FolderEntry();
        }
        case NewTabMenuEntryType::MatchProfiles:
        {
            return viewModel.as<Editor::MatchProfilesEntryViewModel>().MatchProfilesEntry();
        }
        case NewTabMenuEntryType::RemainingProfiles:
        {
            return Model::RemainingProfilesEntry{};
        }
        case NewTabMenuEntryType::Invalid:
        default:
            return nullptr;
        }
    }

    ProfileEntryViewModel::ProfileEntryViewModel(Model::ProfileEntry profileEntry) :
        ProfileEntryViewModelT<ProfileEntryViewModel, NewTabMenuEntryViewModel>(Model::NewTabMenuEntryType::Profile),
        _ProfileEntry{ profileEntry }
    {
    }

    SeparatorEntryViewModel::SeparatorEntryViewModel() :
        SeparatorEntryViewModelT<SeparatorEntryViewModel, NewTabMenuEntryViewModel>(Model::NewTabMenuEntryType::Separator)
    {
    }

    FolderEntryViewModel::FolderEntryViewModel(Model::FolderEntry folderEntry) :
        FolderEntryViewModelT<FolderEntryViewModel, NewTabMenuEntryViewModel>(Model::NewTabMenuEntryType::Folder),
        _FolderEntry{ folderEntry }
    {
        _Entries = _ConvertToViewModelEntries(_FolderEntry.Entries());
    }

    MatchProfilesEntryViewModel::MatchProfilesEntryViewModel(Model::MatchProfilesEntry matchProfilesEntry) :
        MatchProfilesEntryViewModelT<MatchProfilesEntryViewModel, NewTabMenuEntryViewModel>(Model::NewTabMenuEntryType::MatchProfiles),
        _MatchProfilesEntry{ matchProfilesEntry }
    {
    }

    hstring MatchProfilesEntryViewModel::DisplayText() const
    {
        std::wstringstream ss;
        if (const auto profileName = _MatchProfilesEntry.Name(); !profileName.empty())
        {
            ss << fmt::format(L"profile: {}, ", profileName);
        }
        if (const auto commandline = _MatchProfilesEntry.Commandline(); !commandline.empty())
        {
            ss << fmt::format(L"profile: {}, ", commandline);
        }
        if (const auto source = _MatchProfilesEntry.Source(); !source.empty())
        {
            ss << fmt::format(L"profile: {}, ", source);
        }

        // Chop off the last ", "
        auto s = ss.str();
        return winrt::hstring{ s.substr(0, s.size() - 2) };
    }

    RemainingProfilesEntryViewModel::RemainingProfilesEntryViewModel() :
        RemainingProfilesEntryViewModelT<RemainingProfilesEntryViewModel, NewTabMenuEntryViewModel>(Model::NewTabMenuEntryType::RemainingProfiles)
    {
    }
}
