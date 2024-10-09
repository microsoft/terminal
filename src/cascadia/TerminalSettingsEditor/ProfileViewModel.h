// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "DeleteProfileEventArgs.g.h"
#include "NavigateToProfileArgs.g.h"
#include "ProfileViewModel.g.h"
#include "Utils.h"
#include "ViewModelHelpers.h"

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    struct NavigateToProfileArgs : NavigateToProfileArgsT<NavigateToProfileArgs>
    {
    public:
        NavigateToProfileArgs(ProfileViewModel profile, Editor::IHostedInWindow windowRoot) :
            _Profile(profile),
            _WindowRoot(windowRoot) {}

        Editor::IHostedInWindow WindowRoot() const noexcept { return _WindowRoot; }
        Editor::ProfileViewModel Profile() const noexcept { return _Profile; }

    private:
        Editor::IHostedInWindow _WindowRoot;
        Editor::ProfileViewModel _Profile{ nullptr };
    };

    struct ProfileViewModel : ProfileViewModelT<ProfileViewModel>, ViewModelHelper<ProfileViewModel>
    {
    public:
        // font face
        static void UpdateFontList() noexcept;
        static Windows::Foundation::Collections::IObservableVector<Editor::Font> CompleteFontList() noexcept { return _FontList; };
        static Windows::Foundation::Collections::IObservableVector<Editor::Font> MonospaceFontList() noexcept { return _MonospaceFontList; };

        ProfileViewModel(const Model::Profile& profile, const Model::CascadiaSettings& settings);
        Model::TerminalSettings TermSettings() const;
        void DeleteProfile();

        void SetupAppearances(Windows::Foundation::Collections::IObservableVector<Editor::ColorSchemeViewModel> schemesList);

        // bell style bits
        bool IsBellStyleFlagSet(const uint32_t flag);
        void SetBellStyleAudible(winrt::Windows::Foundation::IReference<bool> on);
        void SetBellStyleWindow(winrt::Windows::Foundation::IReference<bool> on);
        void SetBellStyleTaskbar(winrt::Windows::Foundation::IReference<bool> on);

        void SetAcrylicOpacityPercentageValue(double value)
        {
            Opacity(static_cast<float>(value) / 100.0f);
        };

        void SetPadding(double value)
        {
            Padding(to_hstring(value));
        }

        winrt::hstring EvaluatedIcon() const
        {
            return _profile.EvaluatedIcon();
        }

        // starting directory
        bool UseParentProcessDirectory();
        void UseParentProcessDirectory(const bool useParent);
        bool UseCustomStartingDirectory();

        // icon
        bool HideIcon();
        void HideIcon(const bool hide);

        // general profile knowledge
        winrt::guid OriginalProfileGuid() const noexcept;
        bool CanDeleteProfile() const;
        Editor::AppearanceViewModel DefaultAppearance();
        Editor::AppearanceViewModel UnfocusedAppearance();
        bool HasUnfocusedAppearance();
        bool EditableUnfocusedAppearance() const noexcept;
        bool ShowUnfocusedAppearance();
        void CreateUnfocusedAppearance();
        void DeleteUnfocusedAppearance();

        bool ShowMarksAvailable() const noexcept;
        bool AutoMarkPromptsAvailable() const noexcept;
        bool RepositionCursorWithMouseAvailable() const noexcept;

        til::typed_event<Editor::ProfileViewModel, Editor::DeleteProfileEventArgs> DeleteProfileRequested;

        VIEW_MODEL_OBSERVABLE_PROPERTY(ProfileSubPage, CurrentPage);

        PERMANENT_OBSERVABLE_PROJECTED_SETTING(_profile, Guid);
        PERMANENT_OBSERVABLE_PROJECTED_SETTING(_profile, ConnectionType);
        OBSERVABLE_PROJECTED_SETTING(_profile, Name);
        OBSERVABLE_PROJECTED_SETTING(_profile, Source);
        OBSERVABLE_PROJECTED_SETTING(_profile, Hidden);
        OBSERVABLE_PROJECTED_SETTING(_profile, Icon);
        OBSERVABLE_PROJECTED_SETTING(_profile, CloseOnExit);
        OBSERVABLE_PROJECTED_SETTING(_profile, TabTitle);
        OBSERVABLE_PROJECTED_SETTING(_profile, TabColor);
        OBSERVABLE_PROJECTED_SETTING(_profile, SuppressApplicationTitle);
        OBSERVABLE_PROJECTED_SETTING(_profile, ScrollState);
        OBSERVABLE_PROJECTED_SETTING(_profile, Padding);
        OBSERVABLE_PROJECTED_SETTING(_profile, Commandline);
        OBSERVABLE_PROJECTED_SETTING(_profile, StartingDirectory);
        OBSERVABLE_PROJECTED_SETTING(_profile, AntialiasingMode);
        OBSERVABLE_PROJECTED_SETTING(_profile.DefaultAppearance(), Opacity);
        OBSERVABLE_PROJECTED_SETTING(_profile.DefaultAppearance(), UseAcrylic);
        OBSERVABLE_PROJECTED_SETTING(_profile, HistorySize);
        OBSERVABLE_PROJECTED_SETTING(_profile, SnapOnInput);
        OBSERVABLE_PROJECTED_SETTING(_profile, AltGrAliasing);
        OBSERVABLE_PROJECTED_SETTING(_profile, BellStyle);
        OBSERVABLE_PROJECTED_SETTING(_profile, Elevate);
        OBSERVABLE_PROJECTED_SETTING(_profile, ReloadEnvironmentVariables);
        OBSERVABLE_PROJECTED_SETTING(_profile, RightClickContextMenu);
        OBSERVABLE_PROJECTED_SETTING(_profile, ShowMarks);
        OBSERVABLE_PROJECTED_SETTING(_profile, AutoMarkPrompts);
        OBSERVABLE_PROJECTED_SETTING(_profile, RepositionCursorWithMouse);

        WINRT_PROPERTY(bool, IsBaseLayer, false);
        WINRT_PROPERTY(bool, FocusDeleteButton, false);
        GETSET_BINDABLE_ENUM_SETTING(AntiAliasingMode, Microsoft::Terminal::Control::TextAntialiasingMode, AntialiasingMode);
        GETSET_BINDABLE_ENUM_SETTING(CloseOnExitMode, Microsoft::Terminal::Settings::Model::CloseOnExitMode, CloseOnExit);
        GETSET_BINDABLE_ENUM_SETTING(ScrollState, Microsoft::Terminal::Control::ScrollbarState, ScrollState);

    private:
        Model::Profile _profile;
        winrt::guid _originalProfileGuid{};
        winrt::hstring _lastBgImagePath;
        winrt::hstring _lastStartingDirectoryPath;
        winrt::hstring _lastIcon;
        Editor::AppearanceViewModel _defaultAppearanceViewModel;

        static Windows::Foundation::Collections::IObservableVector<Editor::Font> _MonospaceFontList;
        static Windows::Foundation::Collections::IObservableVector<Editor::Font> _FontList;

        Model::CascadiaSettings _appSettings;
        Editor::AppearanceViewModel _unfocusedAppearanceViewModel;
    };

    struct DeleteProfileEventArgs :
        public DeleteProfileEventArgsT<DeleteProfileEventArgs>
    {
    public:
        DeleteProfileEventArgs(guid profileGuid) :
            _ProfileGuid(profileGuid) {}

        guid ProfileGuid() const noexcept { return _ProfileGuid; }

    private:
        guid _ProfileGuid{};
    };
};
