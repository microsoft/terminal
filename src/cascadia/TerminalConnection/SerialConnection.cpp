#include "pch.h"

#include "SerialConnection.h"
#include "SerialConnection.g.cpp"

using namespace winrt::Microsoft::Terminal::TerminalConnection;

namespace winrt::Microsoft::Terminal::TerminalConnection::implementation
{
    winrt::Windows::Foundation::Collections::ValueSet SerialConnection::CreateSettings(hstring const& deviceDescription, uint32_t rows, uint32_t columns, winrt::guid const& guid, winrt::guid const& profileGuid)
    {
        Windows::Foundation::Collections::ValueSet vs{};

        vs.Insert(L"device", Windows::Foundation::PropertyValue::CreateString(deviceDescription));
        vs.Insert(L"initialRows", Windows::Foundation::PropertyValue::CreateUInt32(rows));
        vs.Insert(L"initialCols", Windows::Foundation::PropertyValue::CreateUInt32(columns));
        vs.Insert(L"guid", Windows::Foundation::PropertyValue::CreateGuid(guid));
        vs.Insert(L"profileGuid", Windows::Foundation::PropertyValue::CreateGuid(profileGuid));

        return vs;
    }

    void SerialConnection::Initialize(winrt::Windows::Foundation::Collections::ValueSet const& settings)
    {
        std::wstring device{ winrt::unbox_value<hstring>(settings.Lookup(L"device")) };
        if (auto pos = device.find_first_of(L": "); pos != std::wstring::npos)
        {
            std::wstring comm{ device.substr(pos + 1) };
            if (!comm.empty())
            {
                _dcb.DCBlength = sizeof(_dcb);
                _dcbValid = !!BuildCommDCBW(comm.c_str(), &_dcb); // OK to fail
                if (!_dcbValid)
                {
                    LOG_LAST_ERROR();
                }
            }
            device.resize(pos);
        }
        _devicePath = std::move(device);
    }

    void SerialConnection::Start()
    {
        _transitionToState(ConnectionState::Connecting);
        _port.reset(CreateFileW(_devicePath.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr));
        if (_dcbValid)
        {
            SetCommState(_port.get(), &_dcb);
        }

        COMMTIMEOUTS timeouts{
            .ReadIntervalTimeout = MAXDWORD,
            .ReadTotalTimeoutMultiplier = MAXDWORD,
            .ReadTotalTimeoutConstant = 10000,
        };
        SetCommTimeouts(_port.get(), &timeouts);

        auto strong{ get_strong() };
        _ioThread = std::thread{
            [strong = std::move(strong), this]() {
                _transitionToState(ConnectionState::Connected);
                IoThreadBody();
                // strong maintains our lifetime until the I/O thread dies
            }
        };
    }
    void SerialConnection::WriteInput(array_view<char16_t const> buffer)
    {
        const auto data = winrt_array_to_wstring_view(buffer);
        std::string temp;
        LOG_IF_FAILED(til::u16u8(data, temp, _u16State));
        if (!WriteFile(_port.get(), temp.data(), static_cast<DWORD>(temp.length()), nullptr, nullptr))
        {
            _transitionToState(ConnectionState::Failed);
            _port.reset();
        }
        //FlushFileBuffers(_port.get());
    }
    void SerialConnection::Resize(uint32_t rows, uint32_t columns)
    {
        UNREFERENCED_PARAMETER(rows);
        UNREFERENCED_PARAMETER(columns);
        return;
    }
    void SerialConnection::Close()
    {
        _transitionToState(ConnectionState::Closing);
        _port.reset();
        _ioThread.join();
        _transitionToState(ConnectionState::Closed);
    }

    void SerialConnection::IoThreadBody()
    {
        char buffer[128 * 1024];
        DWORD read = 0;

        til::u8state u8State;
        std::wstring wstr;

        for (;;)
        {
            DCB ndcb{};
            ndcb.DCBlength = sizeof(ndcb);
            GetCommState(_port.get(), &ndcb);
            // When we have a `wstr` that's ready for processing we must do so without blocking.
            // Otherwise, whatever the user typed will be delayed until the next IO operation.
            // With overlapped IO that's not a problem because the ReadFile() calls won't block.
            if (!ReadFile(_port.get(), &buffer[0], sizeof(buffer), &read, nullptr))
            {
                if (GetLastError() != ERROR_IO_PENDING)
                {
                    break;
                }
            }

            // If we hit a parsing error, eat it. It's bad utf-8, we can't do anything with it.
            LOG_IF_FAILED(til::u8u16({ &buffer[0], gsl::narrow_cast<size_t>(read) }, wstr, u8State));

            if (!wstr.empty())
            {
                try
                {
                    TerminalOutput.raise(wstr);
                }
                CATCH_LOG();
            }

            if (read == 0)
            {
                // TODO???
                //break;
            }

            if (_isStateAtOrBeyond(ConnectionState::Closing))
            {
                break;
            }
        }

        return;
    }
}
