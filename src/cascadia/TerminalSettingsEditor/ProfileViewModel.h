// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "DeleteProfileEventArgs.g.h"
#include "BellSoundViewModel.g.h"
#include "ProfileViewModel.g.h"
#include "Utils.h"
#include "ViewModelHelpers.h"

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    struct BellSoundViewModel : BellSoundViewModelT<BellSoundViewModel>, ViewModelHelper<BellSoundViewModel>
    {
    public:
        BellSoundViewModel(const Model::IMediaResource& resource);

        hstring Path() const { return _resource.Path(); }
        bool FileExists() const { return _resource.Ok(); }
        hstring DisplayPath() const;
        hstring SubText() const;
        VIEW_MODEL_OBSERVABLE_PROPERTY(bool, ShowDirectory);

    private:
        Model::IMediaResource _resource;
    };

    struct ProfileViewModel : ProfileViewModelT<ProfileViewModel>, ViewModelHelper<ProfileViewModel>
    {
    public:
        // font face
        static void UpdateFontList() noexcept;
        static Windows::Foundation::Collections::IObservableVector<Editor::Font> CompleteFontList() noexcept { return _FontList; };
        static Windows::Foundation::Collections::IObservableVector<Editor::Font> MonospaceFontList() noexcept { return _MonospaceFontList; };

        ProfileViewModel(const Model::Profile& profile, const Model::CascadiaSettings& settings, const Windows::UI::Core::CoreDispatcher& dispatcher);
        Control::IControlSettings TermSettings() const;
        void DeleteProfile();

        void SetupAppearances(Windows::Foundation::Collections::IObservableVector<Editor::ColorSchemeViewModel> schemesList);
        void ResetSettings();

        // bell style bits
        hstring BellStylePreview() const;
        bool IsBellStyleFlagSet(const uint32_t flag);
        void SetBellStyleAudible(winrt::Windows::Foundation::IReference<bool> on);
        void SetBellStyleWindow(winrt::Windows::Foundation::IReference<bool> on);
        void SetBellStyleTaskbar(winrt::Windows::Foundation::IReference<bool> on);
        void SetBellStyleNotification(winrt::Windows::Foundation::IReference<bool> on);

        hstring BellSoundPreview();
        void RequestAddBellSound(hstring path);
        void RequestDeleteBellSound(const Editor::BellSoundViewModel& vm);

        void SetAcrylicOpacityPercentageValue(double value)
        {
            Opacity(static_cast<float>(value) / 100.0f);
        };

        void LeftPadding(double value) noexcept;
        double LeftPadding() const noexcept;
        void TopPadding(double value) noexcept;
        double TopPadding() const noexcept;
        void RightPadding(double value) noexcept;
        double RightPadding() const noexcept;
        void BottomPadding(double value) noexcept;
        double BottomPadding() const noexcept;

        winrt::hstring EvaluatedIcon() const
        {
            return _profile.Icon().Resolved();
        }
        Windows::UI::Xaml::Controls::IconElement IconPreview() const;
        winrt::hstring LocalizedIcon() const;
        winrt::hstring IconPath() const { return _profile.Icon().Path(); }
        void IconPath(const winrt::hstring& path)
        {
            Icon(Model::MediaResourceHelper::FromString(path));
            _NotifyChanges(L"Icon", L"IconPath");
        }
        bool UsingNoIcon() const noexcept;

        // starting directory
        hstring CurrentStartingDirectoryPreview() const;
        bool UseParentProcessDirectory() const;
        void UseParentProcessDirectory(const bool useParent);

        // general profile knowledge
        winrt::guid OriginalProfileGuid() const noexcept;
        bool CanDeleteProfile() const;
        Editor::AppearanceViewModel DefaultAppearance() const;
        Editor::AppearanceViewModel UnfocusedAppearance() const;
        bool HasUnfocusedAppearance();
        bool EditableUnfocusedAppearance() const noexcept;
        bool ShowUnfocusedAppearance();
        void CreateUnfocusedAppearance();
        void DeleteUnfocusedAppearance();

        bool ShowMarksAvailable() const noexcept;
        bool AutoMarkPromptsAvailable() const noexcept;
        bool RepositionCursorWithMouseAvailable() const noexcept;

        bool Orphaned() const;
        hstring TabTitlePreview() const;
        hstring AnswerbackMessagePreview() const;
        Windows::UI::Color TabColorPreview() const;
        Windows::UI::Color TabThemeColorPreview() const;

        til::typed_event<Editor::ProfileViewModel, Editor::DeleteProfileEventArgs> DeleteProfileRequested;

        VIEW_MODEL_OBSERVABLE_PROPERTY(ProfileSubPage, CurrentPage);
        VIEW_MODEL_OBSERVABLE_PROPERTY(Windows::Foundation::Collections::IObservableVector<Editor::BellSoundViewModel>, CurrentBellSounds);

        PERMANENT_OBSERVABLE_PROJECTED_SETTING(_profile, Guid);
        PERMANENT_OBSERVABLE_PROJECTED_SETTING(_profile, ConnectionType);

// The clearable settings this view model projects from the underlying profile model.
// This is the single source of truth for both the declarations below and the
// ClearX() calls in ResetSettings()
#define PROFILE_VIEW_MODEL_PROJECTED_SETTINGS(X) \
    X(_profile, Name)                            \
    X(_profile, Source)                          \
    X(_profile, Hidden)                          \
    X(_profile, Icon)                            \
    X(_profile, CloseOnExit)                     \
    X(_profile, TabTitle)                        \
    X(_profile, TabColor)                        \
    X(_profile, SuppressApplicationTitle)        \
    X(_profile, ScrollState)                     \
    X(_profile, Padding)                         \
    X(_profile, Commandline)                     \
    X(_profile, StartingDirectory)               \
    X(_profile, AntialiasingMode)                \
    X(_profile.DefaultAppearance(), Opacity)     \
    X(_profile.DefaultAppearance(), UseAcrylic)  \
    X(_profile, HistorySize)                     \
    X(_profile, SnapOnInput)                     \
    X(_profile, AltGrAliasing)                   \
    X(_profile, BellStyle)                       \
    X(_profile, BellSound)                       \
    X(_profile, Elevate)                         \
    X(_profile, ReloadEnvironmentVariables)      \
    X(_profile, RightClickContextMenu)           \
    X(_profile, ShowMarks)                       \
    X(_profile, AutoMarkPrompts)                 \
    X(_profile, RepositionCursorWithMouse)       \
    X(_profile, ForceVTInput)                    \
    X(_profile, AllowKittyKeyboardMode)          \
    X(_profile, AllowVtChecksumReport)           \
    X(_profile, AllowVtClipboardWrite)           \
    X(_profile, AllowOscNotifications)           \
    X(_profile, AnswerbackMessage)               \
    X(_profile, RainbowSuggestions)              \
    X(_profile, PathTranslationStyle)            \
    X(_profile, DragDropDelimiter)

#define PROFILE_VIEW_MODEL_DECLARE_SETTING(target, name) OBSERVABLE_PROJECTED_SETTING(target, name);
        PROFILE_VIEW_MODEL_PROJECTED_SETTINGS(PROFILE_VIEW_MODEL_DECLARE_SETTING)
#undef PROFILE_VIEW_MODEL_DECLARE_SETTING

        WINRT_PROPERTY(bool, IsBaseLayer, false);
        WINRT_PROPERTY(bool, FocusDeleteButton, false);
        GETSET_BINDABLE_ENUM_SETTING(AntiAliasingMode, Microsoft::Terminal::Control::TextAntialiasingMode, AntialiasingMode);
        GETSET_BINDABLE_ENUM_SETTING(CloseOnExitMode, Microsoft::Terminal::Settings::Model::CloseOnExitMode, CloseOnExit);
        GETSET_BINDABLE_ENUM_SETTING(ScrollState, Microsoft::Terminal::Control::ScrollbarState, ScrollState);
        GETSET_BINDABLE_ENUM_SETTING(PathTranslationStyle, Microsoft::Terminal::Control::PathTranslationStyle, PathTranslationStyle);

    private:
        Model::Profile _profile;
        winrt::guid _originalProfileGuid{};
        winrt::hstring _lastBgImagePath;
        winrt::hstring _lastStartingDirectoryPath;
        Editor::AppearanceViewModel _defaultAppearanceViewModel;
        Windows::UI::Core::CoreDispatcher _dispatcher;

        winrt::Windows::UI::Xaml::Thickness _parsedPadding;

        void _InitializeCurrentBellSounds();
        void _PrepareModelForBellSoundModification();
        void _MarkDuplicateBellSoundDirectories();
        void _RefreshDefaultAppearanceViewModel();
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
