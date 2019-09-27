#include "pch.h"
#include "DebugTapConnection.h"

namespace winrt::Microsoft::TerminalApp::implementation
{
    class DebugInputTapConnection : public winrt::implements<DebugInputTapConnection, winrt::Microsoft::Terminal::TerminalConnection::ITerminalConnection>
    {
    public:
        DebugInputTapConnection(DebugTapConnection& pairedTap, Microsoft::Terminal::TerminalConnection::ITerminalConnection wrappedConnection) :
            _pairedTap{ pairedTap },
            _wrappedConnection{ std::move(wrappedConnection) }
        {
        }
        ~DebugInputTapConnection() = default;
        void Start() { _wrappedConnection.Start(); }
        void WriteInput(hstring const& data)
        {
            _pairedTap.PrintInput(data);
            _wrappedConnection.WriteInput(data);
        }
        void Resize(uint32_t rows, uint32_t columns) { _wrappedConnection.Resize(rows, columns); }
        void Close() { _wrappedConnection.Close(); }
        winrt::event_token TerminalOutput(Microsoft::Terminal::TerminalConnection::TerminalOutputEventArgs const& args) { return _wrappedConnection.TerminalOutput(args); };
        void TerminalOutput(winrt::event_token const& tok) noexcept { _wrappedConnection.TerminalOutput(tok); };
        winrt::event_token TerminalDisconnected(Microsoft::Terminal::TerminalConnection::TerminalDisconnectedEventArgs const& args) { return _wrappedConnection.TerminalDisconnected(args); };
        void TerminalDisconnected(winrt::event_token const& tok) noexcept { _wrappedConnection.TerminalDisconnected(tok); };

    private:
        DebugTapConnection& _pairedTap;
        Microsoft::Terminal::TerminalConnection::ITerminalConnection _wrappedConnection;
    };

    DebugTapConnection::DebugTapConnection(Microsoft::Terminal::TerminalConnection::ITerminalConnection wrappedConnection)
    {
        _outputRevoker = wrappedConnection.TerminalOutput(winrt::auto_revoke, { this, &DebugTapConnection::_OutputHandler });
        _disconnectedRevoker = wrappedConnection.TerminalDisconnected(winrt::auto_revoke, { this, &DebugTapConnection::_DisconnectedHandler });
        _wrappedConnection = wrappedConnection;
    }
    DebugTapConnection::~DebugTapConnection()
    {
    }
    void DebugTapConnection::Start()
    {
        // presume the wrapped connection is started.
    }
    void DebugTapConnection::WriteInput(hstring const& data)
    {
        // no input is written.
    }
    void DebugTapConnection::Resize(uint32_t rows, uint32_t columns)
    {
        _columns = columns;
        _off = 0;
    }
    void DebugTapConnection::Close()
    {
        _outputRevoker.revoke();
        _disconnectedRevoker.revoke();
        _wrappedConnection = nullptr;
    }
    static std::wstring _sanitizeString(const std::wstring_view str)
    {
        std::wstring newString{ str.begin(), str.end() };
        for (auto& ch : newString)
        {
            if (ch < 0x20)
            {
                ch += 0x2400;
            }
            else if (ch == 0x20)
            {
                ch = 0x2423; // replace space with ␣
            }
            else if (ch == 0x7f)
            {
                ch = 0x2421; // replace del with ␡
            }
        }
        return newString;
    }
    void DebugTapConnection::_OutputHandler(const hstring str)
    {
        _terminalOutput(_sanitizeString(str));
    }
    void DebugTapConnection::_DisconnectedHandler()
    {
        _terminalDisconnected();
    }
    void DebugTapConnection::PrintInput(const hstring& str)
    {
        auto clean{ _sanitizeString(str) };
        auto formatted{ wil::str_printf<std::wstring>(L"\x1b[1;31m%ls\x1b[m", clean.data()) };
        _terminalOutput(formatted);
    }
    DEFINE_EVENT(DebugTapConnection, TerminalOutput, _terminalOutput, Microsoft::Terminal::TerminalConnection::TerminalOutputEventArgs);
    DEFINE_EVENT(DebugTapConnection, TerminalDisconnected, _terminalDisconnected, Microsoft::Terminal::TerminalConnection::TerminalDisconnectedEventArgs);
}

using namespace winrt::Microsoft::TerminalApp::implementation;
std::tuple<winrt::Microsoft::Terminal::TerminalConnection::ITerminalConnection, winrt::Microsoft::Terminal::TerminalConnection::ITerminalConnection> OpenDebugTapConnection(winrt::Microsoft::Terminal::TerminalConnection::ITerminalConnection baseConnection)
{
    auto b = winrt::make_self<DebugTapConnection>(baseConnection);
    auto a = winrt::make_self<DebugInputTapConnection>(*b, baseConnection);
    std::tuple<winrt::Microsoft::Terminal::TerminalConnection::ITerminalConnection, winrt::Microsoft::Terminal::TerminalConnection::ITerminalConnection> p{ *a, *b };
    return p;
}
