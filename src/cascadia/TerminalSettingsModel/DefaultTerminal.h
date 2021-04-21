/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- DefaultTerminal.h

Abstract:
- A Default Terminal is an application that can register
  as the handler window or "terminal" for a command-line
  application. This class is the model for presenting
  handler options in the Windows Terminal Settings UI.

Author(s):
- Michael Niksa <miniksa> - 20-Apr-2021

--*/

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
        hstring Author() const;
        hstring Version() const;
        hstring Icon() const;

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
