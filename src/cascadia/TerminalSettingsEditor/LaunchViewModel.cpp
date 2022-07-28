// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "LaunchViewModel.h"
#include "LaunchViewModel.g.cpp"
#include "EnumEntry.h"

using namespace winrt::Windows::UI::Xaml::Navigation;
using namespace winrt::Windows::Foundation;
using namespace winrt::Microsoft::Terminal::Settings::Model;

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    LaunchViewModel::LaunchViewModel(Model::CascadiaSettings settings) :
        _Settings{ settings }
    {
        INITIALIZE_BINDABLE_ENUM_SETTING(FirstWindowPreference, FirstWindowPreference, FirstWindowPreference, L"Globals_FirstWindowPreference", L"Content");
        INITIALIZE_BINDABLE_ENUM_SETTING(LaunchMode, LaunchMode, LaunchMode, L"Globals_LaunchMode", L"Content");
        // More options were added to the JSON mapper when the enum was made into [Flags]
        // but we want to preserve the previous set of options in the UI.
        _LaunchModeList.RemoveAt(7); // maximizedFullscreenFocus
        _LaunchModeList.RemoveAt(6); // fullscreenFocus
        _LaunchModeList.RemoveAt(3); // maximizedFullscreen
        INITIALIZE_BINDABLE_ENUM_SETTING(WindowingBehavior, WindowingMode, WindowingMode, L"Globals_WindowingBehavior", L"Content");
    }

    double LaunchViewModel::InitialPosX()
    {
        const auto x = _Settings.GlobalSettings().InitialPosition().X;
        // If there's no value here, return NAN - XAML will ignore it and
        // put the placeholder text in the box instead
        auto xCoord = x.try_as<int64_t>();
        return xCoord.has_value() ? gsl::narrow_cast<double>(xCoord.value()) : NAN;
    }

    double LaunchViewModel::InitialPosY()
    {
        const auto y = _Settings.GlobalSettings().InitialPosition().Y;
        // If there's no value here, return NAN - XAML will ignore it and
        // put the placeholder text in the box instead
        auto yCoord = y.try_as<int64_t>();
        return yCoord.has_value() ? gsl::narrow_cast<double>(yCoord.value()) : NAN;
    }

    void LaunchViewModel::InitialPosX(double xCoord)
    {
        winrt::Windows::Foundation::IReference<int64_t> xCoordRef;
        // If the value was cleared, xCoord will be NAN, so check for that
        if (!isnan(xCoord))
        {
            xCoordRef = gsl::narrow_cast<int64_t>(xCoord);
        }
        const LaunchPosition newPos{ xCoordRef, _Settings.GlobalSettings().InitialPosition().Y };
        _Settings.GlobalSettings().InitialPosition(newPos);
    }

    void LaunchViewModel::InitialPosY(double yCoord)
    {
        winrt::Windows::Foundation::IReference<int64_t> yCoordRef;
        // If the value was cleared, yCoord will be NAN, so check for that
        if (!isnan(yCoord))
        {
            yCoordRef = gsl::narrow_cast<int64_t>(yCoord);
        }
        const LaunchPosition newPos{ _Settings.GlobalSettings().InitialPosition().X, yCoordRef };
        _Settings.GlobalSettings().InitialPosition(newPos);
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
