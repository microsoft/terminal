// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "LaunchViewModel.h"
#include "LaunchViewModel.g.cpp"
#include "EnumEntry.h"

#include <LibraryResources.h>
#include <WtExeUtils.h>

using namespace winrt::Windows::UI::Xaml::Navigation;
using namespace winrt::Windows::Foundation;
using namespace winrt::Microsoft::Terminal::Settings::Model;
using namespace winrt::Windows::UI::Xaml::Data;

static constexpr std::wstring_view StartupTaskName = L"StartTerminalOnLoginTask";

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    // For ComboBox an empty SelectedItem string denotes no selection.
    // What we want instead is for "Use system language" to be selected by default.
    // --> "und" is synonymous for "Use system language".
    constexpr std::wstring_view systemLanguageTag{ L"und" };

    static constexpr std::array appLanguageTags{
        L"en-US",
        L"de-DE",
        L"es-ES",
        L"fr-FR",
        L"it-IT",
        L"ja",
        L"ko",
        L"pt-BR",
        L"qps-PLOC",
        L"qps-PLOCA",
        L"qps-PLOCM",
        L"ru",
        L"zh-Hans",
        L"zh-Hant",
    };

    LaunchViewModel::LaunchViewModel(Model::CascadiaSettings settings) :
        _Settings{ settings }
    {
        _useDefaultLaunchPosition = isnan(InitialPosX()) && isnan(InitialPosY());

        INITIALIZE_BINDABLE_ENUM_SETTING(DefaultInputScope, DefaultInputScope, winrt::Microsoft::Terminal::Control::DefaultInputScope, L"Globals_DefaultInputScope", L"Content");
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
            else if (viewModelProperty == L"InitialCols" || viewModelProperty == L"InitialRows")
            {
                _NotifyChanges(L"LaunchSizeCurrentValue");
            }
        });
    }

    winrt::hstring LaunchViewModel::LanguageDisplayConverter(const winrt::hstring& tag)
    {
        if (tag == systemLanguageTag)
        {
            return RS_(L"Globals_LanguageDefault");
        }

        winrt::Windows::Globalization::Language language{ tag };
        return language.NativeName();
    }

    // Returns whether the language selector is available/shown.
    //
    // winrt::Windows::Globalization::ApplicationLanguages::PrimaryLanguageOverride()
    // doesn't work for unpackaged applications. The corresponding code in TerminalApp is disabled.
    // It would be confusing for our users if we presented a dysfunctional language selector.
    bool LaunchViewModel::LanguageSelectorAvailable()
    {
        return IsPackaged();
    }

    // Returns the list of languages the user may override the application language with.
    // The returned list are BCP 47 language tags like {"und", "en-US", "de-DE", "es-ES", ...}.
    // "und" is short for "undefined" and is synonymous for "Use system language" in this code.
    winrt::Windows::Foundation::Collections::IObservableVector<winrt::hstring> LaunchViewModel::LanguageList()
    {
        if (_languageList)
        {
            return _languageList;
        }

        if (!LanguageSelectorAvailable())
        {
            _languageList = {};
            return _languageList;
        }

        // In order to return the language list this code does the following:
        // [1] Get all possible languages we want to allow the user to choose.
        //     We have to acquire languages from multiple sources, creating duplicates. See below at [1].
        // [2] Sort languages by their ASCII tags, forcing the UI in a consistent/stable order.
        //     I wanted to sort the localized language names initially, but it turned out to be complex.
        // [3] Remove potential duplicates in our language list from [1].
        //     We don't want to have en-US twice in the list, do we?
        // [4] Optionally remove unwanted language tags (like pseudo-localizations).

        std::vector<winrt::hstring> tags;

        // [1]:
        {
            // ManifestLanguages contains languages the app ships with.
            // Unfortunately, we cannot use this source. Our manifest must contain the
            // ~100 languages that are localized for the shell extension and start menu
            // presentation so we align with Windows display languages for those surfaces.
            // However, the actual content of our application is limited to a much smaller
            // subset of approximately 14 languages. As such, we will code the limited
            // subset of languages that we support for selection within the Settings
            // dropdown to steer users towards the ones that we can display in the app.

            // As per the function definition, the first item
            // is always "Use system language" ("und").
            tags.emplace_back(systemLanguageTag);

            // Add our hard-coded languages after the system definition.
            for (const auto& v : appLanguageTags)
            {
                tags.push_back(v);
            }
        }

        // NOTE: The size of tags is always >0, due to tags[0] being hard-coded to "und".
        const auto tagsBegin = ++tags.begin();
        const auto tagsEnd = tags.end();

        // [2]:
        std::sort(tagsBegin, tagsEnd);

        // I'd love for both, std::unique and std::remove_if, to occur in a single loop,
        // but the code turned out to be complex and even less maintainable, so I gave up.
        {
            // [3] part 1:
            auto it = std::unique(tagsBegin, tagsEnd);

            // The qps- languages are useful for testing ("pseudo-localization").
            // --> Leave them in if debug features are enabled.
            if (!_Settings.GlobalSettings().DebugFeaturesEnabled())
            {
                // [4] part 1:
                it = std::remove_if(tagsBegin, it, [](const winrt::hstring& tag) -> bool {
                    return til::starts_with(tag, L"qps-");
                });
            }

            // [3], [4] part 2 (completing the so called "erase-remove idiom"):
            tags.erase(it, tagsEnd);
        }

        _languageList = winrt::single_threaded_observable_vector(std::move(tags));
        return _languageList;
    }

    winrt::Windows::Foundation::IInspectable LaunchViewModel::CurrentLanguage()
    {
        if (_currentLanguage)
        {
            return _currentLanguage;
        }

        if (!LanguageSelectorAvailable())
        {
            _currentLanguage = {};
            return _currentLanguage;
        }

        // NOTE: PrimaryLanguageOverride throws if this instance is unpackaged.
        auto currentLanguage = winrt::Windows::Globalization::ApplicationLanguages::PrimaryLanguageOverride();
        if (currentLanguage.empty())
        {
            currentLanguage = systemLanguageTag;
        }

        _currentLanguage = winrt::box_value(currentLanguage);
        return _currentLanguage;
    }

    void LaunchViewModel::CurrentLanguage(const winrt::Windows::Foundation::IInspectable& tag)
    {
        _currentLanguage = tag;

        const auto currentLanguage = winrt::unbox_value<winrt::hstring>(_currentLanguage);
        if (currentLanguage == systemLanguageTag)
        {
            _Settings.GlobalSettings().ClearLanguage();
        }
        else
        {
            _Settings.GlobalSettings().Language(currentLanguage);
        }
    }

    winrt::hstring LaunchViewModel::LaunchSizeCurrentValue() const
    {
        return winrt::hstring{ fmt::format(FMT_COMPILE(L"{} × {}"), InitialCols(), InitialRows()) };
    }

    winrt::hstring LaunchViewModel::LaunchParametersCurrentValue()
    {
        const auto launchModeString = CurrentLaunchMode().as<EnumEntry>()->EnumName();

        winrt::hstring result;

        // Append the launch position part
        if (UseDefaultLaunchPosition())
        {
            result = fmt::format(FMT_COMPILE(L"{}, {}"), launchModeString, RS_(L"Globals_DefaultLaunchPositionCheckbox/Content"));
        }
        else
        {
            const auto xPosString = isnan(InitialPosX()) ? RS_(L"Globals_LaunchModeDefault/Content") : winrt::to_hstring(gsl::narrow_cast<int>(InitialPosX()));
            const auto yPosString = isnan(InitialPosY()) ? RS_(L"Globals_LaunchModeDefault/Content") : winrt::to_hstring(gsl::narrow_cast<int>(InitialPosY()));
            result = fmt::format(FMT_COMPILE(L"{}, ({},{})"), launchModeString, xPosString, yPosString);
        }

        // Append the CenterOnLaunch part
        result = CenterOnLaunch() ? winrt::hstring{ fmt::format(FMT_COMPILE(L"{}, {}"), result, RS_(L"Globals_CenterOnLaunchCentered")) } : result;
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
            if (!profile.Deleted() && !profile.Orphaned())
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

    bool LaunchViewModel::StartOnUserLoginAvailable()
    {
        return IsPackaged();
    }

    safe_void_coroutine LaunchViewModel::PrepareStartOnUserLoginSettings()
    {
        if (!StartOnUserLoginAvailable())
        {
            co_return;
        }

        auto strongThis{ get_strong() };
        auto task{ co_await winrt::Windows::ApplicationModel::StartupTask::GetAsync(StartupTaskName) };
        _startOnUserLoginTask = std::move(task);
        _NotifyChanges(L"StartOnUserLoginConfigurable", L"StartOnUserLoginStatefulHelpText", L"StartOnUserLogin");
    }

    bool LaunchViewModel::StartOnUserLoginConfigurable()
    {
        if (!_startOnUserLoginTask)
        {
            return false;
        }
        namespace WAM = winrt::Windows::ApplicationModel;
        const auto state{ _startOnUserLoginTask.State() };
        // Terminal cannot change the state of the login task if it is any of the "ByUser" or "ByPolicy" states.
        return state == WAM::StartupTaskState::Disabled || state == WAM::StartupTaskState::Enabled;
    }

    winrt::hstring LaunchViewModel::StartOnUserLoginStatefulHelpText()
    {
        if (_startOnUserLoginTask)
        {
            namespace WAM = winrt::Windows::ApplicationModel;
            switch (_startOnUserLoginTask.State())
            {
            case WAM::StartupTaskState::EnabledByPolicy:
            case WAM::StartupTaskState::DisabledByPolicy:
                return winrt::hstring{ L"\uE72E " } /*lock icon*/ + RS_(L"Globals_StartOnUserLogin_UnavailableByPolicy");
            case WAM::StartupTaskState::DisabledByUser:
                return RS_(L"Globals_StartOnUserLogin_DisabledByUser");
            case WAM::StartupTaskState::Enabled:
            case WAM::StartupTaskState::Disabled:
            default:
                break; // fall through to the common case (no task, not configured, etc.)
            }
        }
        return RS_(L"Globals_StartOnUserLogin/HelpText");
    }

    bool LaunchViewModel::StartOnUserLogin()
    {
        if (!_startOnUserLoginTask)
        {
            return false;
        }
        namespace WAM = winrt::Windows::ApplicationModel;
        const auto state{ _startOnUserLoginTask.State() };
        return state == WAM::StartupTaskState::Enabled || state == WAM::StartupTaskState::EnabledByPolicy;
    }

    safe_void_coroutine LaunchViewModel::StartOnUserLogin(bool enable)
    {
        if (!_startOnUserLoginTask)
        {
            co_return;
        }

        auto strongThis{ get_strong() };
        if (enable)
        {
            co_await _startOnUserLoginTask.RequestEnableAsync();
        }
        else
        {
            _startOnUserLoginTask.Disable();
        }
        // Any of these could have changed in response to an attempt to enable (e.g. it was disabled in task manager since our last check)
        _NotifyChanges(L"StartOnUserLoginConfigurable", L"StartOnUserLoginStatefulHelpText", L"StartOnUserLogin");
    }
}
