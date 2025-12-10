#pragma once

#include "SerialConnection.g.h"
#include "BaseTerminalConnection.h"
#include <til/spsc.h>

namespace winrt::Microsoft::Terminal::TerminalConnection::implementation
{
    struct SerialConnection : SerialConnectionT<SerialConnection>, BaseTerminalConnection<SerialConnection>
    {
        SerialConnection() = default;

        static winrt::Windows::Foundation::Collections::ValueSet CreateSettings(hstring const& deviceDescription, uint32_t rows, uint32_t columns, winrt::guid const& guid, winrt::guid const& profileGuid);

        void Initialize(winrt::Windows::Foundation::Collections::ValueSet const& settings);
        void Start();
        void WriteInput(array_view<char16_t const> data);
        void Resize(uint32_t rows, uint32_t columns);
        void Close();

        til::event<TerminalOutputHandler> TerminalOutput;

    private:
        std::wstring _devicePath;
        wil::unique_hfile _port;
        bool _dcbValid{ false };
        DCB _dcb{};
        void IoThreadBody();
        std::thread _ioThread;
        til::spsc::producer<char16_t> _chan{ nullptr };

        til::u16state _u16State;
    };
}
namespace winrt::Microsoft::Terminal::TerminalConnection::factory_implementation
{
    BASIC_FACTORY(SerialConnection)
}
