#include "pch.h"
#include "DebugTapConnection.h"

using namespace ::winrt::Microsoft::Terminal::TerminalConnection;
using namespace ::winrt::Windows::Foundation;
namespace winrt::Microsoft::TerminalApp::implementation
{
    class DebugInputTapConnection : public winrt::implements<DebugInputTapConnection, ITerminalConnection>
    {
    public:
        DebugInputTapConnection(winrt::com_ptr<DebugTapConnection> pairedTap, ITerminalConnection wrappedConnection) :
            _pairedTap{ pairedTap },
            _wrappedConnection{ std::move(wrappedConnection) }
        {
        }
        ~DebugInputTapConnection() = default;
        void Start() { _wrappedConnection.Start(); }
        void WriteInput(hstring const& data)
        {
            _pairedTap->PrintInput(data);
            _wrappedConnection.WriteInput(data);
        }
        void Resize(uint32_t rows, uint32_t columns) { _wrappedConnection.Resize(rows, columns); }
        void Close() { _wrappedConnection.Close(); }
        winrt::event_token TerminalOutput(TerminalOutputHandler const& args) { return _wrappedConnection.TerminalOutput(args); };
        void TerminalOutput(winrt::event_token const& tok) noexcept { _wrappedConnection.TerminalOutput(tok); };
        winrt::event_token StateChanged(TypedEventHandler<ITerminalConnection, IInspectable> const& handler) { return _wrappedConnection.StateChanged(handler); };
        void StateChanged(winrt::event_token const& tok) noexcept { _wrappedConnection.StateChanged(tok); };
        ConnectionState State() const noexcept { return _wrappedConnection.State(); }

    private:
        winrt::com_ptr<DebugTapConnection> _pairedTap;
        ITerminalConnection _wrappedConnection;
    };

    DebugTapConnection::DebugTapConnection(ITerminalConnection wrappedConnection)
    {
        _outputRevoker = wrappedConnection.TerminalOutput(winrt::auto_revoke, { this, &DebugTapConnection::_OutputHandler });
        _stateChangedRevoker = wrappedConnection.StateChanged(winrt::auto_revoke, [this](auto&& /*s*/, auto&& /*e*/) {
            _StateChangedHandlers(*this, nullptr);
        });
        _wrappedConnection = wrappedConnection;
    }
    DebugTapConnection::~DebugTapConnection()
    {
    }
    void DebugTapConnection::Start()
    {
        // presume the wrapped connection is started.
    }
    void DebugTapConnection::WriteInput(hstring const& /*data*/)
    {
        // no input is written.
    }
    void DebugTapConnection::Resize(uint32_t /*rows*/, uint32_t /*columns*/)
    {
        // no resize events are propagated
    }
    void DebugTapConnection::Close()
    {
        _outputRevoker.revoke();
        _stateChangedRevoker.revoke();
        _wrappedConnection = nullptr;
    }
    ConnectionState DebugTapConnection::State() const noexcept
    {
        if (auto strongConnection = _wrappedConnection.get())
        {
            return strongConnection.State();
        }
        return ConnectionState::Failed;
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
        _TerminalOutputHandlers(_sanitizeString(str));
    }
    void DebugTapConnection::PrintInput(const hstring& str)
    {
        auto clean{ _sanitizeString(str) };
        auto formatted{ wil::str_printf<std::wstring>(L"\x1b[1;31m%ls\x1b[m", clean.data()) };
        _TerminalOutputHandlers(formatted);
    }
}

using namespace winrt::Microsoft::TerminalApp::implementation;
std::tuple<ITerminalConnection, ITerminalConnection> OpenDebugTapConnection(ITerminalConnection baseConnection)
{
    auto b = winrt::make_self<DebugTapConnection>(baseConnection);
    auto a = winrt::make_self<DebugInputTapConnection>(b, baseConnection);
    std::tuple<ITerminalConnection, ITerminalConnection> p{ *a, *b };
    return p;
}
