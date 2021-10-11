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
    return _pkg.terminal.name.empty() ? winrt::hstring{ RS_(L"InboxWindowsConsoleName") } : winrt::hstring{ _pkg.terminal.name };
}

winrt::hstring DefaultTerminal::Version() const
{
    // If there's no version information... return empty string instead.
    const auto& version = _pkg.terminal.version;
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
    return _pkg.terminal.author.empty() ? winrt::hstring{ RS_(L"InboxWindowsConsoleAuthor") } : winrt::hstring{ _pkg.terminal.author };
}

winrt::hstring DefaultTerminal::Icon() const
{
    return _pkg.terminal.logo.empty() ? winrt::hstring{ L"\uE756" } : winrt::hstring{ _pkg.terminal.logo };
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
    DelegationConfig::DelegationPackage currentPackage;
    LOG_IF_FAILED(DelegationConfig::s_GetAvailablePackages(allPackages, currentPackage));

    // Good old conhost has a hardcoded GUID of {00000000-0000-0000-0000-000000000000}.
    return currentPackage.terminal.clsid != CLSID{};
}

void DefaultTerminal::Current(const Model::DefaultTerminal& term)
{
    THROW_IF_FAILED(DelegationConfig::s_SetDefaultByPackage(winrt::get_self<DefaultTerminal>(term)->_pkg, true));

    TraceLoggingWrite(g_hSettingsModelProvider,
                      "DefaultTerminalChanged",
                      TraceLoggingWideString(term.Name().c_str(), "TerminalName", "the name of the default terminal"),
                      TraceLoggingWideString(term.Version().c_str(), "TerminalVersion", "the version of the default terminal"),
                      TraceLoggingWideString(term.Author().c_str(), "TerminalAuthor", "the author of the default terminal"),
                      TraceLoggingKeyword(MICROSOFT_KEYWORD_MEASURES),
                      TelemetryPrivacyDataTag(PDT_ProductAndServiceUsage));
}
