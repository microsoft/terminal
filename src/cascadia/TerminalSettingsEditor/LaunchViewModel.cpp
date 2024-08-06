// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "LaunchViewModel.h"
#include "LaunchViewModel.g.cpp"
#include "EnumEntry.h"
#include <LibraryResources.h>

using namespace winrt::Windows::UI::Xaml::Navigation;
using namespace winrt::Windows::Foundation;
using namespace winrt::Microsoft::Terminal::Settings::Model;
using namespace winrt::Windows::UI::Xaml::Data;

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    LaunchViewModel::LaunchViewModel(Model::CascadiaSettings settings) :
        _Settings{ settings }
    {
        _useDefaultLaunchPosition = isnan(InitialPosX()) && isnan(InitialPosY());

        INITIALIZE_BINDABLE_ENUM_SETTING(FirstWindowPreference, FirstWindowPreference, FirstWindowPreference, L"Globals_FirstWindowPreference", L"Content");
        INITIALIZE_BINDABLE_ENUM_SETTING(LaunchMode, LaunchMode, LaunchMode, L"Globals_LaunchMode", L"Content");
        // More options were added to the JSON mapper when the enum was made into [Flags]
        // but we want to preserve the previous set of options in the UI.
        _LaunchModeList.RemoveAt(7); // maximizedFullscreenFocus
        _LaunchModeList.RemoveAt(6); // fullscreenFocus
        _LaunchModeList.RemoveAt(3); // maximizedFullscreen
        INITIALIZE_BINDABLE_ENUM_SETTING(WindowingBehavior, WindowingMode, WindowingMode, L"Globals_WindowingBehavior", L"Content");

        // Add a property changed handler to our own property changed event.
        // This propagates changes from the settings model to anybody listening to our
        // unique view model members.
        PropertyChanged([this](auto&&, const PropertyChangedEventArgs& args) {
            const auto viewModelProperty{ args.PropertyName() };
            if (viewModelProperty == L"CenterOnLaunch")
            {
                _NotifyChanges(L"LaunchParametersCurrentValue");
            }
        });
    }

    winrt::hstring LaunchViewModel::LaunchParametersCurrentValue()
    {
        const auto launchModeString = CurrentLaunchMode().as<EnumEntry>()->EnumName();

        winrt::hstring result;

        // Append the launch position part
        if (UseDefaultLaunchPosition())
        {
            result = fmt::format(L"{}, {}", launchModeString, RS_(L"Globals_DefaultLaunchPositionCheckbox/Content"));
        }
        else
        {
            const std::wstring xPosString = isnan(InitialPosX()) ? RS_(L"Globals_LaunchModeDefault/Content").c_str() : std::to_wstring(gsl::narrow_cast<int>(InitialPosX()));
            const std::wstring yPosString = isnan(InitialPosY()) ? RS_(L"Globals_LaunchModeDefault/Content").c_str() : std::to_wstring(gsl::narrow_cast<int>(InitialPosY()));
            result = fmt::format(L"{}, ({},{})", launchModeString, xPosString, yPosString);
        }

        // Append the CenterOnLaunch part
        result = CenterOnLaunch() ? winrt::hstring{ fmt::format(L"{}, {}", result, RS_(L"Globals_CenterOnLaunchCentered")) } : result;
        return result;
    }

    double LaunchViewModel::InitialPosX()
    {
        const auto x = _Settings.GlobalSettings().InitialPosition().X;
        // If there's no value here, return NAN - XAML will ignore it and
        // put the placeholder text in the box instead
        const auto xCoord = x.try_as<int32_t>();
        return xCoord.has_value() ? gsl::narrow_cast<double>(xCoord.value()) : NAN;
    }

    double LaunchViewModel::InitialPosY()
    {
        const auto y = _Settings.GlobalSettings().InitialPosition().Y;
        // If there's no value here, return NAN - XAML will ignore it and
        // put the placeholder text in the box instead
        const auto yCoord = y.try_as<int32_t>();
        return yCoord.has_value() ? gsl::narrow_cast<double>(yCoord.value()) : NAN;
    }

    void LaunchViewModel::InitialPosX(double xCoord)
    {
        winrt::Windows::Foundation::IReference<int32_t> xCoordRef;
        // If the value was cleared, xCoord will be NAN, so check for that
        if (!isnan(xCoord))
        {
            xCoordRef = gsl::narrow_cast<int32_t>(xCoord);
        }
        const LaunchPosition newPos{ xCoordRef, _Settings.GlobalSettings().InitialPosition().Y };
        _Settings.GlobalSettings().InitialPosition(newPos);
        _NotifyChanges(L"LaunchParametersCurrentValue");
    }

    void LaunchViewModel::InitialPosY(double yCoord)
    {
        winrt::Windows::Foundation::IReference<int32_t> yCoordRef;
        // If the value was cleared, yCoord will be NAN, so check for that
        if (!isnan(yCoord))
        {
            yCoordRef = gsl::narrow_cast<int32_t>(yCoord);
        }
        const LaunchPosition newPos{ _Settings.GlobalSettings().InitialPosition().X, yCoordRef };
        _Settings.GlobalSettings().InitialPosition(newPos);
        _NotifyChanges(L"LaunchParametersCurrentValue");
    }

    void LaunchViewModel::UseDefaultLaunchPosition(bool useDefaultPosition)
    {
        _useDefaultLaunchPosition = useDefaultPosition;
        if (useDefaultPosition)
        {
            InitialPosX(NAN);
            InitialPosY(NAN);
        }
        _NotifyChanges(L"UseDefaultLaunchPosition", L"LaunchParametersCurrentValue", L"InitialPosX", L"InitialPosY");
    }

    bool LaunchViewModel::UseDefaultLaunchPosition()
    {
        return _useDefaultLaunchPosition;
    }

    winrt::Windows::Foundation::IInspectable LaunchViewModel::CurrentLaunchMode()
    {
        return winrt::box_value<winrt::Microsoft::Terminal::Settings::Editor::EnumEntry>(_LaunchModeMap.Lookup(_Settings.GlobalSettings().LaunchMode()));
    }

    void LaunchViewModel::CurrentLaunchMode(const winrt::Windows::Foundation::IInspectable& enumEntry)
    {
        if (const auto ee = enumEntry.try_as<winrt::Microsoft::Terminal::Settings::Editor::EnumEntry>())
        {
            const auto setting = winrt::unbox_value<LaunchMode>(ee.EnumValue());
            _Settings.GlobalSettings().LaunchMode(setting);
            _NotifyChanges(L"LaunchParametersCurrentValue");
        }
    }

    winrt::Windows::Foundation::Collections::IObservableVector<winrt::Microsoft::Terminal::Settings::Editor::EnumEntry> LaunchViewModel::LaunchModeList()
    {
        return _LaunchModeList;
    }

    winrt::Windows::Foundation::IInspectable LaunchViewModel::CurrentDefaultProfile()
    {
        const auto defaultProfileGuid{ _Settings.GlobalSettings().DefaultProfile() };
        return winrt::box_value(_Settings.FindProfile(defaultProfileGuid));
    }

    void LaunchViewModel::CurrentDefaultProfile(const IInspectable& value)
    {
        const auto profile{ winrt::unbox_value<Model::Profile>(value) };
        _Settings.GlobalSettings().DefaultProfile(profile.Guid());
    }

    winrt::Windows::Foundation::Collections::IObservableVector<Model::Profile> LaunchViewModel::DefaultProfiles() const
    {
        const auto allProfiles = _Settings.AllProfiles();

        std::vector<Model::Profile> profiles;
        profiles.reserve(allProfiles.Size());

        // Remove profiles from the selection which have been explicitly deleted.
        // We do want to show hidden profiles though, as they are just hidden
        // from menus, but still work as the startup profile for instance.
        for (const auto& profile : allProfiles)
        {
            if (!profile.Deleted())
            {
                profiles.emplace_back(profile);
            }
        }

        return winrt::single_threaded_observable_vector(std::move(profiles));
    }

    winrt::Windows::Foundation::IInspectable LaunchViewModel::CurrentDefaultTerminal()
    {
        return winrt::box_value(_Settings.CurrentDefaultTerminal());
    }

    void LaunchViewModel::CurrentDefaultTerminal(const IInspectable& value)
    {
        const auto defaultTerminal{ winrt::unbox_value<Model::DefaultTerminal>(value) };
        _Settings.CurrentDefaultTerminal(defaultTerminal);
    }

    winrt::Windows::Foundation::Collections::IObservableVector<Model::DefaultTerminal> LaunchViewModel::DefaultTerminals() const
    {
        return _Settings.DefaultTerminals();
    }
}
