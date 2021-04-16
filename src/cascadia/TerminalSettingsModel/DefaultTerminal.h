// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "DefaultTerminal.g.h"
#include "../inc/cppwinrt_utils.h"

#include "../../propslib/DelegationConfig.hpp"

namespace winrt::Microsoft::Terminal::Settings::Model::implementation
{
    struct DefaultTerminal : public DefaultTerminalT<DefaultTerminal>
    {
        DefaultTerminal(DelegationConfig::DelegationPackage pkg);

        hstring Name() const;

        static void Refresh();
        static Windows::Foundation::Collections::IVectorView<Model::DefaultTerminal> Available();
        static Model::DefaultTerminal Current();
        static void Current(const Model::DefaultTerminal& term);

    private:
        DelegationConfig::DelegationPackage _pkg;
        static Windows::Foundation::Collections::IVector<Model::DefaultTerminal> _available;
        static std::optional<Model::DefaultTerminal> _current;
    };
}

namespace winrt::Microsoft::Terminal::Settings::Model::factory_implementation
{
    BASIC_FACTORY(DefaultTerminal);
}
