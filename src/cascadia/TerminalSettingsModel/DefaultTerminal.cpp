// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "DefaultTerminal.h"
#include "DefaultTerminal.g.cpp"

#include <LibraryResources.h>

using namespace winrt::Microsoft::Terminal::Settings;
using namespace winrt::Microsoft::Terminal::Settings::Model::implementation;

DefaultTerminal::DefaultTerminal(DelegationConfig::DelegationPackage&& pkg) :
    _pkg{ pkg }
{
}

winrt::hstring DefaultTerminal::Name() const
{
    switch (_pkg.pair.kind)
    {
    case DelegationConfig::DelegationPairKind::Default:
        return RS_(L"DefaultWindowsConsoleName");
    case DelegationConfig::DelegationPairKind::Conhost:
        return RS_(L"InboxWindowsConsoleName");
    default:
        return winrt::hstring{ _pkg.info.name };
    }
}

winrt::hstring DefaultTerminal::Version() const
{
    // If there's no version information... return empty string instead.
    const auto& version = _pkg.info.version;
    if (DelegationConfig::PkgVersion{} == version)
    {
        return winrt::hstring{};
    }

    fmt::wmemory_buffer buffer;
    fmt::format_to(buffer, L"{}.{}.{}.{}", version.major, version.minor, version.build, version.revision);
    return winrt::hstring{ buffer.data(), gsl::narrow_cast<winrt::hstring::size_type>(buffer.size()) };
}

winrt::hstring DefaultTerminal::Author() const
{
    switch (_pkg.pair.kind)
    {
    case DelegationConfig::DelegationPairKind::Default:
        return {}; // The "Let Windows decide" option has no author.
    case DelegationConfig::DelegationPairKind::Conhost:
        return RS_(L"InboxWindowsConsoleAuthor");
    default:
        return winrt::hstring{ _pkg.info.author };
    }
}

winrt::hstring DefaultTerminal::Icon() const
{
    return _pkg.info.logo.empty() ? winrt::hstring{ L"\uE756" } : winrt::hstring{ _pkg.info.logo };
}

std::pair<std::vector<Model::DefaultTerminal>, Model::DefaultTerminal> DefaultTerminal::Available()
{
    // The potential of returning nullptr for defaultTerminal feels weird, but XAML can
    // handle that appropriately and will select nothing as current in the dropdown.
    std::vector<Model::DefaultTerminal> defaultTerminals;
    Model::DefaultTerminal defaultTerminal{ nullptr };

    std::vector<DelegationConfig::DelegationPackage> allPackages;
    DelegationConfig::DelegationPackage currentPackage;
    LOG_IF_FAILED(DelegationConfig::s_GetAvailablePackages(allPackages, currentPackage));

    for (auto& pkg : allPackages)
    {
        // Be a bit careful here: We're moving pkg into the constructor.
        // Afterwards it'll be invalid, so we need to cache isCurrent.
        const auto isCurrent = pkg == currentPackage;
        auto p = winrt::make<DefaultTerminal>(std::move(pkg));

        if (isCurrent)
        {
            defaultTerminal = p;
        }

        defaultTerminals.emplace_back(std::move(p));
    }

    return { std::move(defaultTerminals), std::move(defaultTerminal) };
}

bool DefaultTerminal::HasCurrent()
{
    std::vector<DelegationConfig::DelegationPackage> allPackages;
    DelegationConfig::DelegationPackage currentPackage{ DelegationConfig::DefaultDelegationPair };
    LOG_IF_FAILED(DelegationConfig::s_GetAvailablePackages(allPackages, currentPackage));
    return !currentPackage.pair.IsDefault();
}

void DefaultTerminal::Current(const Model::DefaultTerminal& term)
{
    THROW_IF_FAILED(DelegationConfig::s_SetDefaultByPackage(winrt::get_self<DefaultTerminal>(term)->_pkg));

    TraceLoggingWrite(g_hSettingsModelProvider,
                      "DefaultTerminalChanged",
                      TraceLoggingWideString(term.Name().c_str(), "TerminalName", "the name of the default terminal"),
                      TraceLoggingWideString(term.Version().c_str(), "TerminalVersion", "the version of the default terminal"),
                      TraceLoggingWideString(term.Author().c_str(), "TerminalAuthor", "the author of the default terminal"),
                      TraceLoggingKeyword(MICROSOFT_KEYWORD_MEASURES),
                      TelemetryPrivacyDataTag(PDT_ProductAndServiceUsage));
}
