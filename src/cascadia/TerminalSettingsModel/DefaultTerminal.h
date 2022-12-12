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

#include "../../propslib/DelegationConfig.hpp"

namespace winrt::Microsoft::Terminal::Settings::Model::implementation
{
    struct DefaultTerminal : public DefaultTerminalT<DefaultTerminal>
    {
        explicit DefaultTerminal(DelegationConfig::DelegationPackage&& pkg);

        hstring ToString()
        {
            return hstring{ fmt::format(L"{}, {}, {}", Name(), Author(), Version()) };
        }

        hstring Name() const;
        hstring Author() const;
        hstring Version() const;
        hstring Icon() const;

        static std::pair<std::vector<Model::DefaultTerminal>, Model::DefaultTerminal> Available();
        static bool HasCurrent();
        static void Current(const Model::DefaultTerminal& term);

    private:
        DelegationConfig::DelegationPackage _pkg;
    };
}
