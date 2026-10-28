// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "TmuxControl.h"
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <til/unicode.h>

namespace winrt::TerminalApp::implementation
{
    class TmuxPaneConnection;

    struct __declspec(uuid("3f9f8c52-7221-4f3b-8cf5-15da3fe85271")) ITmuxGatewayIdentity : ::IUnknown
    {
    };

    struct __declspec(uuid("97d03528-7854-4eb1-99eb-2f76fd48e85a")) ITmuxPaneIdentity : ::IUnknown
    {
    };

    class TmuxSession : public std::enable_shared_from_this<TmuxSession>
    {
    public:
        using Window = ::Microsoft::Terminal::Tmux::Window;
        using Connection = Microsoft::Terminal::TerminalConnection::ITerminalConnection;

        explicit TmuxSession(Connection transport, uint32_t historyLimit);
        ~TmuxSession();
        void Start();
        void Close();
        void ForceExit();
        void Input(uint32_t pane, std::string_view bytes);
        void Command(std::string command);
        void Capture(uint32_t pane, const winrt::weak_ref<TmuxPaneConnection>& connection);
        void CapturePending();
        void KillPane(uint32_t pane);
        void Split(uint32_t pane, bool horizontal, bool before, float size);
        void Focus(uint32_t window, uint32_t pane);
        void ResizeWindow(uint32_t window, uint32_t columns, uint32_t rows);
        void ResizePane(uint32_t pane, char direction);
        void PaneResized();
        void AcknowledgeResize() noexcept { _resizeScheduled = false; }
        bool Active() const;
        bool WindowOwned() const noexcept { return _windowOwned.load(); }
        void WindowOwned(bool value);
        void CloseGateway();

        std::function<void(std::string_view)> TerminalOutput;
        std::function<void(const winrt::array_view<const char16_t>)> PassthroughOutput;
        std::function<void(std::vector<Window>)> WindowsChanged;
        std::function<void()> ResizeRequested;
        std::function<void()> StateChanged;

        Connection Transport() const { return _transport; }

    private:
        mutable std::mutex _mutex;
        std::condition_variable _writeReady;
        ::Microsoft::Terminal::Tmux::Client _client;
        Connection _transport;
        Connection::TerminalOutput_revoker _outputRevoker;
        Connection::StateChanged_revoker _stateRevoker;
        std::map<uint32_t, winrt::weak_ref<TmuxPaneConnection>> _panes;
        std::optional<std::pair<uint32_t, uint32_t>> _focused;
        std::deque<std::string> _writes;
        size_t _queuedBytes{};
        bool _started{}, _closing{};
        std::thread _writer;
        std::atomic<bool> _resizeScheduled{};
        std::atomic<bool> _windowOwned{};
        std::atomic<bool> _gatewayClosed{};

        void _Run(const std::function<void()>& operation);
        void _Dispatch(std::vector<::Microsoft::Terminal::Tmux::Event> events);
        void _WriteThread();
        void _Receive(const winrt::array_view<const char16_t> bytes);
        void _Disconnected();
        winrt::com_ptr<TmuxPaneConnection> _Pane(uint32_t pane);
    };

    class TmuxGatewayConnection : public winrt::implements<TmuxGatewayConnection, Microsoft::Terminal::TerminalConnection::ITerminalConnection, Microsoft::Terminal::TerminalConnection::ITerminalConnectionWrapper, winrt::cloaked<ITmuxGatewayIdentity>>
    {
    public:
        explicit TmuxGatewayConnection(std::shared_ptr<TmuxSession> session, Windows::Foundation::IInspectable window = nullptr);
        static winrt::com_ptr<TmuxGatewayConnection> From(const Microsoft::Terminal::TerminalConnection::ITerminalConnection& connection);
        ~TmuxGatewayConnection();
        static safe_void_coroutine final_release(std::unique_ptr<TmuxGatewayConnection> self);
        void Initialize(const Windows::Foundation::Collections::ValueSet&) {}
        void Start();
        void WriteInput(const winrt::array_view<const char16_t> bytes);
        void Resize(uint32_t rows, uint32_t columns);
        void Close();
        winrt::guid SessionId() const noexcept;
        Microsoft::Terminal::TerminalConnection::ConnectionState State() const noexcept;
        Microsoft::Terminal::TerminalConnection::ITerminalConnection UnderlyingConnection() const { return _session->Transport(); }
        bool InControlMode() const { return _session->Active(); }
        std::shared_ptr<TmuxSession> Session() const { return _session; }

        til::event<Microsoft::Terminal::TerminalConnection::TerminalOutputHandler> TerminalOutput;
        til::typed_event<Microsoft::Terminal::TerminalConnection::ITerminalConnection, Windows::Foundation::IInspectable> StateChanged;

    private:
        std::shared_ptr<TmuxSession> _session;
        Windows::Foundation::IInspectable _window;
        std::atomic<bool> _closed{};
        std::recursive_mutex _callbackMutex;
        std::mutex _decodeMutex;
        til::u8state _utf8;
    };

    class TmuxPaneConnection : public winrt::implements<TmuxPaneConnection, Microsoft::Terminal::TerminalConnection::ITerminalConnection, Microsoft::Terminal::TerminalConnection::ITerminalResponseHandler, winrt::cloaked<ITmuxPaneIdentity>>
    {
    public:
        TmuxPaneConnection(std::shared_ptr<TmuxSession> session, uint32_t window, uint32_t pane);
        static winrt::com_ptr<TmuxPaneConnection> From(const Microsoft::Terminal::TerminalConnection::ITerminalConnection& connection);
        void Initialize(const Windows::Foundation::Collections::ValueSet&) {}
        void Start();
        void WriteInput(const winrt::array_view<const char16_t> bytes);
        // tmux has already answered VT queries on the application's PTY.
        void WriteResponse(const winrt::array_view<const char16_t>) {}
        void Resize(uint32_t rows, uint32_t columns);
        void Close();
        winrt::guid SessionId() const noexcept { return {}; }
        Microsoft::Terminal::TerminalConnection::ConnectionState State() const noexcept { return _state.load(); }

        void Output(std::string bytes);
        void Disconnected();
        void Kill();
        void Split(bool horizontal, bool before, float size);
        void Focus();
        void ResizeBy(char direction);
        void ToggleZoom();
        void NewWindow();
        uint32_t PaneId() const noexcept { return _pane; }
        std::shared_ptr<TmuxSession> Session() const { return _session; }

        til::event<Microsoft::Terminal::TerminalConnection::TerminalOutputHandler> TerminalOutput;
        til::typed_event<Microsoft::Terminal::TerminalConnection::ITerminalConnection, Windows::Foundation::IInspectable> StateChanged;

    private:
        std::shared_ptr<TmuxSession> _session;
        uint32_t _window, _pane;
        std::atomic<Microsoft::Terminal::TerminalConnection::ConnectionState> _state{ Microsoft::Terminal::TerminalConnection::ConnectionState::NotConnected };
        std::atomic<uint64_t> _size{};
        std::mutex _outputMutex;
        std::condition_variable _outputIdle;
        std::deque<std::string> _output;
        bool _draining{};
        std::thread::id _drainThread;
        til::u8state _utf8;
    };
}
