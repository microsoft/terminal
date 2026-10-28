// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include <winrt/Microsoft.Terminal.TerminalConnection.h>

namespace winrt::Microsoft::Terminal::TerminalConnection
{
    inline ConptyConnection GetConptyConnection(ITerminalConnection connection)
    {
        for (size_t depth = 0; connection && depth < 8; ++depth)
        {
            if (const auto conpty = connection.try_as<ConptyConnection>())
            {
                return conpty;
            }
            const auto wrapper = connection.try_as<ITerminalConnectionWrapper>();
            if (!wrapper)
            {
                break;
            }
            connection = wrapper.UnderlyingConnection();
        }
        return nullptr;
    }
}
