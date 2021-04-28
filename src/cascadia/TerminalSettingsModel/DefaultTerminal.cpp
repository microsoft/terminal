// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "DefaultTerminal.h"
#include "DefaultTerminal.g.cpp"

#include <LibraryResources.h>

using namespace winrt::Microsoft::Terminal::Settings;
using namespace winrt::Microsoft::Terminal::Settings::Model::implementation;

winrt::Windows::Foundation::Collections::IVector<Model::DefaultTerminal> DefaultTerminal::_available = winrt::single_threaded_vector<Model::DefaultTerminal>();
std::optional<Model::DefaultTerminal> DefaultTerminal::_current;

DefaultTerminal::DefaultTerminal(DelegationConfig::DelegationPackage pkg) :
    _pkg(pkg)
{
}

winrt::hstring DefaultTerminal::Name() const
{
    static const std::wstring def{ RS_(L"InboxWindowsConsoleName") };
    return _pkg.terminal.name.empty() ? winrt::hstring{ def } : winrt::hstring{ _pkg.terminal.name };
}

winrt::hstring DefaultTerminal::Version() const
{
    // If there's no version information... return empty string instead.
    if (DelegationConfig::PkgVersion{} == _pkg.terminal.version)
    {
        return winrt::hstring{};
    }

    const auto name = fmt::format(L"{}.{}.{}.{}", _pkg.terminal.version.major, _pkg.terminal.version.minor, _pkg.terminal.version.build, _pkg.terminal.version.revision);
    return winrt::hstring{ name };
}

winrt::hstring DefaultTerminal::Author() const
{
    static const std::wstring def{ RS_(L"InboxWindowsConsoleAuthor") };
    return _pkg.terminal.author.empty() ? winrt::hstring{ def } : winrt::hstring{ _pkg.terminal.author };
}

winrt::hstring DefaultTerminal::Icon() const
{
    static const std::wstring_view def{ L"\uE756" };
    return _pkg.terminal.logo.empty() ? winrt::hstring{ def } : winrt::hstring{ _pkg.terminal.logo };
}

void DefaultTerminal::Refresh()
{
    std::vector<DelegationConfig::DelegationPackage> allPackages;
    DelegationConfig::DelegationPackage currentPackage;

    LOG_IF_FAILED(DelegationConfig::s_GetAvailablePackages(allPackages, currentPackage));

    _available.Clear();
    for (const auto& pkg : allPackages)
    {
        auto p = winrt::make<DefaultTerminal>(pkg);

        _available.Append(p);

        if (pkg == currentPackage)
        {
            _current = p;
        }
    }
}

winrt::Windows::Foundation::Collections::IVectorView<Model::DefaultTerminal> DefaultTerminal::Available()
{
    Refresh();
    return _available.GetView();
}

Model::DefaultTerminal DefaultTerminal::Current()
{
    if (!_current)
    {
        Refresh();
    }
    return _current.value();
}

void DefaultTerminal::Current(const Model::DefaultTerminal& term)
{
    THROW_IF_FAILED(DelegationConfig::s_SetDefaultByPackage(winrt::get_self<DefaultTerminal>(term)->_pkg, true));
    _current = term;
}
