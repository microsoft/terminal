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
    return winrt::to_hstring(_pkg.terminal.name.c_str());
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
