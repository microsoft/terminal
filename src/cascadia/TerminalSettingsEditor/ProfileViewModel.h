// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "DeleteProfileEventArgs.g.h"
#include "NavigateToProfileArgs.g.h"
#include "BellSoundViewModel.g.h"
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
        static Windows::Foundation::Collections::IVector<IInspectable> BuiltInIcons() noexcept { return _BuiltInIcons; };

        ProfileViewModel(const Model::Profile& profile, const Model::CascadiaSettings& settings, const Windows::UI::Core::CoreDispatcher& dispatcher);
        Control::IControlSettings TermSettings() const;
        void DeleteProfile();

        void SetupAppearances(Windows::Foundation::Collections::IObservableVector<Editor::ColorSchemeViewModel> schemesList);

        // bell style bits
        hstring BellStylePreview() const;
        bool IsBellStyleFlagSet(const uint32_t flag);
        void SetBellStyleAudible(winrt::Windows::Foundation::IReference<bool> on);
        void SetBellStyleWindow(winrt::Windows::Foundation::IReference<bool> on);
        void SetBellStyleTaskbar(winrt::Windows::Foundation::IReference<bool> on);

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
        Windows::Foundation::IInspectable CurrentIconType() const noexcept
        {
            return _currentIconType;
        }
        Windows::UI::Xaml::Controls::IconElement IconPreview() const;
        winrt::hstring LocalizedIcon() const;
        void CurrentIconType(const Windows::Foundation::IInspectable& value);
        bool UsingNoIcon() const;
        bool UsingBuiltInIcon() const;
        bool UsingEmojiIcon() const;
        bool UsingImageIcon() const;
        winrt::hstring IconPath() const { return _profile.Icon().Path(); }
        void IconPath(const winrt::hstring& path)
        {
            Icon(Model::MediaResourceHelper::FromString(path));
            _NotifyChanges(L"Icon", L"IconPath");
        }

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
        VIEW_MODEL_OBSERVABLE_PROPERTY(Windows::Foundation::IInspectable, CurrentBuiltInIcon);
        VIEW_MODEL_OBSERVABLE_PROPERTY(hstring, CurrentEmojiIcon);

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
        OBSERVABLE_PROJECTED_SETTING(_profile, BellSound);
        OBSERVABLE_PROJECTED_SETTING(_profile, Elevate);
        OBSERVABLE_PROJECTED_SETTING(_profile, ReloadEnvironmentVariables);
        OBSERVABLE_PROJECTED_SETTING(_profile, RightClickContextMenu);
        OBSERVABLE_PROJECTED_SETTING(_profile, ShowMarks);
        OBSERVABLE_PROJECTED_SETTING(_profile, AutoMarkPrompts);
        OBSERVABLE_PROJECTED_SETTING(_profile, RepositionCursorWithMouse);
        OBSERVABLE_PROJECTED_SETTING(_profile, ForceVTInput);
        OBSERVABLE_PROJECTED_SETTING(_profile, AllowVtChecksumReport);
        OBSERVABLE_PROJECTED_SETTING(_profile, AllowVtClipboardWrite);
        OBSERVABLE_PROJECTED_SETTING(_profile, AnswerbackMessage);
        OBSERVABLE_PROJECTED_SETTING(_profile, RainbowSuggestions);
        OBSERVABLE_PROJECTED_SETTING(_profile, PathTranslationStyle);

        WINRT_PROPERTY(bool, IsBaseLayer, false);
        WINRT_PROPERTY(bool, FocusDeleteButton, false);
        WINRT_PROPERTY(Windows::Foundation::Collections::IVector<Windows::Foundation::IInspectable>, IconTypes);
        GETSET_BINDABLE_ENUM_SETTING(AntiAliasingMode, Microsoft::Terminal::Control::TextAntialiasingMode, AntialiasingMode);
        GETSET_BINDABLE_ENUM_SETTING(CloseOnExitMode, Microsoft::Terminal::Settings::Model::CloseOnExitMode, CloseOnExit);
        GETSET_BINDABLE_ENUM_SETTING(ScrollState, Microsoft::Terminal::Control::ScrollbarState, ScrollState);
        GETSET_BINDABLE_ENUM_SETTING(PathTranslationStyle, Microsoft::Terminal::Control::PathTranslationStyle, PathTranslationStyle);

    private:
        Model::Profile _profile;
        winrt::guid _originalProfileGuid{};
        winrt::hstring _lastBgImagePath;
        winrt::hstring _lastStartingDirectoryPath;
        winrt::hstring _lastIconPath;
        Windows::Foundation::IInspectable _currentIconType{};
        Editor::AppearanceViewModel _defaultAppearanceViewModel;
        Windows::UI::Core::CoreDispatcher _dispatcher;

        winrt::Windows::UI::Xaml::Thickness _parsedPadding;

        void _InitializeCurrentBellSounds();
        void _PrepareModelForBellSoundModification();
        void _MarkDuplicateBellSoundDirectories();
        static Windows::Foundation::Collections::IObservableVector<Editor::Font> _MonospaceFontList;
        static Windows::Foundation::Collections::IObservableVector<Editor::Font> _FontList;
        static Windows::Foundation::Collections::IVector<Windows::Foundation::IInspectable> _BuiltInIcons;

        Model::CascadiaSettings _appSettings;
        Editor::AppearanceViewModel _unfocusedAppearanceViewModel;
        void _UpdateBuiltInIcons();
        void _DeduceCurrentIconType();
        void _DeduceCurrentBuiltInIcon();
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
