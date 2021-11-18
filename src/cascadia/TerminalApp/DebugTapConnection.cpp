// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "DebugTapConnection.h"

using namespace ::winrt::Microsoft::Terminal::TerminalConnection;
using namespace ::winrt::Windows::Foundation;
namespace winrt::Microsoft::TerminalApp::implementation
{
    // DebugInputTapConnection is an implementation detail of DebugTapConnection.
    // It wraps the _actual_ connection so it can hook WriteInput and forward it
    // into the actual debug panel.
    class DebugInputTapConnection : public winrt::implements<DebugInputTapConnection, ITerminalConnection>
    {
    public:
        DebugInputTapConnection(winrt::com_ptr<DebugTapConnection> pairedTap, ITerminalConnection wrappedConnection) :
            _pairedTap{ pairedTap },
            _wrappedConnection{ std::move(wrappedConnection) }
        {
        }
        void Initialize(const Windows::Foundation::Collections::ValueSet& /*settings*/) {}
        ~DebugInputTapConnection() = default;
        winrt::fire_and_forget Start()
        {
            // GH#11282: It's possible that we're about to be started, _before_
            // our paired connection is started. Both will get Start()'ed when
            // their owning TermControl is finally laid out. However, if we're
            // started first, then we'll immediately start printing to the other
            // control as well, which might not have initialized yet. If we do
            // that, we'll explode.
            //
            // Instead, wait here until the other connection is started too,
            // before actually starting the connection to the client app. This
            // will ensure both controls are initialized before the client app
            // is.
            co_await winrt::resume_background();
            _pairedTap->_start.wait();

            _wrappedConnection.Start();
        }
        void WriteInput(hstring const& data)
        {
            _pairedTap->_PrintInput(data);
            _wrappedConnection.WriteInput(data);
        }
        void Resize(uint32_t rows, uint32_t columns) { _wrappedConnection.Resize(rows, columns); }
        void Close() { _wrappedConnection.Close(); }
        winrt::event_token TerminalOutput(TerminalOutputHandler const& args) { return _wrappedConnection.TerminalOutput(args); };
        void TerminalOutput(winrt::event_token const& token) noexcept { _wrappedConnection.TerminalOutput(token); };
        winrt::event_token StateChanged(TypedEventHandler<ITerminalConnection, IInspectable> const& handler) { return _wrappedConnection.StateChanged(handler); };
        void StateChanged(winrt::event_token const& token) noexcept { _wrappedConnection.StateChanged(token); };
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

        // This is explained in the comment for GH#11282 above.
        _start.count_down();
    }

    void DebugTapConnection::WriteInput(hstring const& data)
    {
        // If the user types into the tap side, forward it to the input side
        if (auto strongInput{ _inputSide.get() })
        {
            auto inputAsTap{ winrt::get_self<DebugInputTapConnection>(strongInput) };
            inputAsTap->WriteInput(data);
        }
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
        if (auto strongConnection{ _wrappedConnection.get() })
        {
            return strongConnection.State();
        }
        return ConnectionState::Failed;
    }

    void DebugTapConnection::_OutputHandler(const hstring str)
    {
        _TerminalOutputHandlers(til::visualize_control_codes(str));
    }

    // Called by the DebugInputTapConnection to print user input
    void DebugTapConnection::_PrintInput(const hstring& str)
    {
        auto clean{ til::visualize_control_codes(str) };
        auto formatted{ wil::str_printf<std::wstring>(L"\x1b[91m%ls\x1b[m", clean.data()) };
        _TerminalOutputHandlers(formatted);
    }

    // Wire us up so that we can forward input through
    void DebugTapConnection::SetInputTap(const Microsoft::Terminal::TerminalConnection::ITerminalConnection& inputTap)
    {
        _inputSide = inputTap;
    }
}

// Function Description
// - Takes one connection and returns two connections:
//   1. One that can be used in place of the original connection (wrapped)
//   2. One that will print raw VT sequences sent into and received _from_ the original connection.
std::tuple<ITerminalConnection, ITerminalConnection> OpenDebugTapConnection(ITerminalConnection baseConnection)
{
    using namespace winrt::Microsoft::TerminalApp::implementation;
    auto debugSide{ winrt::make_self<DebugTapConnection>(baseConnection) };
    auto inputSide{ winrt::make_self<DebugInputTapConnection>(debugSide, baseConnection) };
    debugSide->SetInputTap(*inputSide);
    std::tuple<ITerminalConnection, ITerminalConnection> p{ *inputSide, *debugSide };
    return p;
}
