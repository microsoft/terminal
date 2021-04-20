// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "DefaultTerminal.h"
#include "DefaultTerminal.g.cpp"

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
    // TODO:
    //[3:10 PM] Dustin Howett
    //you can make defaultName a wstring_view and return either winrt::hstring{ _pkg.terminal.name } or winrt : hstring{ defaultName }

    //                                                                                                          [3:10 PM] Dustin Howett this will ensure that we don't need to copy defaultName every time

    static const std::wstring defaultName{ L"Windows Console Host" };
    const auto& name = _pkg.terminal.name.empty() ? defaultName : _pkg.terminal.name;
    return winrt::hstring{ name };
}

winrt::hstring DefaultTerminal::Version() const
{
    const auto name = fmt::format(L"{}.{}.{}.{}", _pkg.terminal.version.major, _pkg.terminal.version.minor, _pkg.terminal.version.build, _pkg.terminal.version.revision);
    return winrt::hstring{ name };
}

winrt::hstring DefaultTerminal::Author() const
{
    static const std::wstring defaultName{ L"Microsoft Corporation" };
    const auto& name = _pkg.terminal.author.empty() ? defaultName : _pkg.terminal.author;
    return winrt::hstring{ name };
}

winrt::hstring DefaultTerminal::Icon() const
{
    static const std::wstring defaultName{ L"\uE756" };
    const auto& name = _pkg.terminal.logo.empty() ? defaultName : _pkg.terminal.logo;
    return winrt::hstring{ name };
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
    THROW_IF_FAILED(DelegationConfig::s_SetDefaultByPackage(winrt::get_self<DefaultTerminal>(term)->_pkg));
    _current = term;
}
