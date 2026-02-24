/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Class Name:
- ConnectionInformation.h

Abstract:
- This is a helper object for storing both the name of a type of connection, and
  a bag of settings to use to initialize that connection.
- This helper is used primarily in cross-proc scenarios, to allow the window
  process to tell the content process the name of the connection type it wants
  created, and how to set that connection up. This is done so the connection can
  live entirely in the content process, without having to go through the window
  process at all.
--*/

#pragma once
#include "ConnectionInformation.g.h"

namespace winrt::Microsoft::Terminal::TerminalConnection::implementation
{
    struct ConnectionInformation : ConnectionInformationT<ConnectionInformation>
    {
        ConnectionInformation(const hstring& className,
                              const Windows::Foundation::Collections::ValueSet& settings);

        static TerminalConnection::ITerminalConnection CreateConnection(TerminalConnection::ConnectionInformation info);

        winrt::hstring ClassName() const { return _ClassName; }
        void ClassName(const winrt::hstring& value) { _ClassName = value; }

        WINRT_PROPERTY(Windows::Foundation::Collections::ValueSet, Settings);

    private:
        winrt::hstring _ClassName{};
    };
}
namespace winrt::Microsoft::Terminal::TerminalConnection::factory_implementation
{
    BASIC_FACTORY(ConnectionInformation);
}
