// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "TmuxConnection.h"

using namespace winrt::Microsoft::Terminal::TerminalConnection;
using namespace ::Microsoft::Terminal::Tmux;

namespace winrt::TerminalApp::implementation
{
    TmuxSession::TmuxSession(Connection transport, const uint32_t historyLimit) :
        _client{ historyLimit },
        _transport{ std::move(transport) }
    {
    }

    TmuxSession::~TmuxSession()
    {
        Close();
    }

    void TmuxSession::Start()
    {
        {
            std::lock_guard lock{ _mutex };
            if (std::exchange(_started, true) || _closing)
            {
                return;
            }
        }
        const auto weak = weak_from_this();
        _outputRevoker = _transport.TerminalOutput(winrt::auto_revoke, [weak](const auto& bytes) {
            if (const auto self = weak.lock())
            {
                self->_Receive(bytes);
            }
        });
        _stateRevoker = _transport.StateChanged(winrt::auto_revoke, [weak](auto&&, auto&&) {
            if (const auto self = weak.lock())
            {
                if (self->_transport.State() >= ConnectionState::Closing)
                {
                    self->_Disconnected();
                }
                if (self->StateChanged)
                {
                    self->StateChanged();
                }
            }
        });
        _transport.Start();
    }

    void TmuxSession::Close()
    {
        {
            std::lock_guard lock{ _mutex };
            if (std::exchange(_closing, true))
            {
                return;
            }
            _writes.clear();
            _queuedBytes = 0;
        }
        _writeReady.notify_all();
        // Closing ConPTY also cancels a pending transport write before joining.
        _transport.Close();
        if (_writer.joinable())
        {
            _writer.join();
        }
        _outputRevoker.revoke();
        _stateRevoker.revoke();
        _Disconnected();
    }

    bool TmuxSession::Active() const
    {
        std::lock_guard lock{ _mutex };
        return !_closing && _client.Active();
    }

    void TmuxSession::ForceExit()
    {
        _Run([&] { _client.ForceExit(); });
    }

    void TmuxSession::_Run(const std::function<void()>& operation)
    {
        std::vector<Event> events;
        bool notifyWriter{};
        {
            std::lock_guard lock{ _mutex };
            const auto wasActive = _client.Active();
            operation();
            if (wasActive && !_client.Active())
            {
                _writes.clear();
                _queuedBytes = 0;
            }
            auto writes = _client.TakeWrites();
            size_t additionalBytes{};
            for (const auto& write : writes)
            {
                additionalBytes += write.size();
            }
            if (_queuedBytes + additionalBytes > 64 * 1024 * 1024)
            {
                _client.Abort("Input queue full; detached without killing remote programs.");
                _writes.clear();
                _queuedBytes = 0;
                writes = _client.TakeWrites();
            }
            for (auto& write : writes)
            {
                if (!_closing)
                {
                    _queuedBytes += write.size();
                    _writes.push_back(std::move(write));
                }
            }
            notifyWriter = !_writes.empty();
            if (notifyWriter && !_writer.joinable())
            {
                _writer = std::thread([this] { _WriteThread(); });
            }
            events = _client.TakeEvents();
        }
        if (notifyWriter)
        {
            _writeReady.notify_one();
        }
        // Never hold the session lock while calling a TermControl. WriteInput
        // is called with the terminal lock held on the other thread.
        _Dispatch(std::move(events));
    }

    void TmuxSession::_WriteThread()
    {
        winrt::init_apartment(winrt::apartment_type::multi_threaded);
        for (;;)
        {
            std::string bytes;
            {
                std::unique_lock lock{ _mutex };
                _writeReady.wait(lock, [this] { return _closing || !_writes.empty(); });
                if (_closing)
                {
                    return;
                }
                while (!_writes.empty() && bytes.size() < 64 * 1024)
                {
                    bytes += _writes.front();
                    _queuedBytes -= _writes.front().size();
                    _writes.pop_front();
                }
            }
            try
            {
                const auto text = til::u8u16(bytes);
                _transport.WriteInput(winrt_wstring_to_array_view(text));
            }
            catch (...)
            {
                LOG_CAUGHT_EXCEPTION();
                _Disconnected();
                return;
            }
        }
    }

    void TmuxSession::_Receive(const winrt::array_view<const char16_t> bytes)
    {
        try
        {
            const auto source = winrt_array_to_wstring_view(bytes);
            bool passthrough{};
            {
                std::lock_guard lock{ _mutex };
                passthrough = !_client.InspectingControlStream() &&
                              source.find(L"\x1bP") == source.npos &&
                              (source.empty() || source.back() != L'\x1b');
            }
            if (passthrough && PassthroughOutput)
            {
                PassthroughOutput(bytes);
                return;
            }
            const auto text = til::u16u8(source);
            _Run([&] { _client.Feed(text); });
        }
        catch (...)
        {
            LOG_CAUGHT_EXCEPTION();
            if (TerminalOutput)
            {
                TerminalOutput("\r\n[tmux control protocol error; detaching]\r\n");
            }
            _Run([&] { _client.Abort("Control protocol error; detached without killing remote programs."); });
        }
    }

    winrt::com_ptr<TmuxPaneConnection> TmuxSession::_Pane(const uint32_t pane)
    {
        std::lock_guard lock{ _mutex };
        const auto it = _panes.find(pane);
        return it == _panes.end() ? nullptr : it->second.get();
    }

    void TmuxSession::_Dispatch(std::vector<Event> events)
    {
        for (auto& event : events)
        {
            switch (event.kind)
            {
            case Event::Kind::Terminal:
                if (TerminalOutput)
                {
                    TerminalOutput(event.data);
                }
                break;
            case Event::Kind::Enter:
                if (TerminalOutput)
                {
                    TerminalOutput("\x1b[?9001l\x1b[?2004l\r\n" + til::u16u8(RS_(L"TmuxControlMenu")));
                }
                break;
            case Event::Kind::Windows:
            {
                std::vector<winrt::com_ptr<TmuxPaneConnection>> removed;
                {
                    std::lock_guard lock{ _mutex };
                    for (const auto& window : event.windows)
                    {
                        if (window.active)
                        {
                            _focused = std::pair{ window.id, window.activePane };
                        }
                    }
                    for (auto it = _panes.begin(); it != _panes.end();)
                    {
                        if (!_client.HasPane(it->first))
                        {
                            if (auto pane = it->second.get())
                            {
                                removed.push_back(std::move(pane));
                            }
                            it = _panes.erase(it);
                        }
                        else
                        {
                            ++it;
                        }
                    }
                }
                for (const auto& pane : removed)
                {
                    pane->Disconnected();
                }
                if (WindowsChanged)
                {
                    WindowsChanged(std::move(event.windows));
                }
                break;
            }
            case Event::Kind::Output:
            case Event::Kind::Snapshot:
                if (const auto pane = _Pane(event.pane))
                {
                    pane->Output(std::move(event.data));
                }
                break;
            case Event::Kind::Exit:
            {
                std::vector<winrt::com_ptr<TmuxPaneConnection>> panes;
                {
                    std::lock_guard lock{ _mutex };
                    for (const auto& [id, weak] : _panes)
                    {
                        if (auto pane = weak.get())
                        {
                            panes.push_back(std::move(pane));
                        }
                    }
                    _panes.clear();
                    _focused.reset();
                }
                for (const auto& pane : panes)
                {
                    pane->Disconnected();
                }
                if (WindowsChanged)
                {
                    WindowsChanged({});
                }
                if (TerminalOutput)
                {
                    TerminalOutput("\x1b[?9001h\r\n" + til::u16u8(RS_(L"TmuxDetached")) + "\r\n");
                }
                break;
            }
            case Event::Kind::Error:
                if (TerminalOutput)
                {
                    TerminalOutput("\r\n[tmux] " + event.data + "\r\n");
                }
                break;
            }
        }
    }

    void TmuxSession::_Disconnected()
    {
        _Run([&] { _client.Disconnect(); });
    }

    void TmuxSession::Input(const uint32_t pane, const std::string_view bytes)
    {
        _Run([&] { _client.Input(pane, bytes); });
    }

    void TmuxSession::Command(std::string command)
    {
        _Run([&] { _client.Command(std::move(command)); });
    }

    void TmuxSession::Capture(const uint32_t pane, const winrt::weak_ref<TmuxPaneConnection>& connection)
    {
        _Run([&] {
            _panes.insert_or_assign(pane, connection);
        });
        PaneResized();
    }

    void TmuxSession::CapturePending()
    {
        _Run([&] {
            for (const auto& pane : _panes)
            {
                _client.Capture(pane.first);
            }
        });
    }

    void TmuxSession::KillPane(const uint32_t pane)
    {
        _Run([&] {
            if (_client.HasPane(pane))
            {
                _client.Command("kill-pane -t %" + std::to_string(pane));
            }
        });
    }

    void TmuxSession::Split(const uint32_t pane, const bool horizontal, const bool before, const float size)
    {
        _Run([&] { _client.Split(pane, horizontal, before, size); });
    }

    void TmuxSession::Focus(const uint32_t window, const uint32_t pane)
    {
        _Run([&] {
            const auto focus = std::pair{ window, pane };
            if (_focused != focus)
            {
                _focused = focus;
                _client.Command("select-window -t @" + std::to_string(window));
                _client.Command("select-pane -t %" + std::to_string(pane));
            }
        });
    }

    void TmuxSession::ResizeWindow(const uint32_t window, const uint32_t columns, const uint32_t rows)
    {
        if (!columns || !rows)
        {
            return;
        }
        _Run([&] {
            _client.ResizeWindow(window, columns, rows);
        });
    }

    void TmuxSession::ResizePane(const uint32_t pane, const char direction)
    {
        Command("resize-pane -t %" + std::to_string(pane) + " -" + direction + " 5");
    }

    void TmuxSession::PaneResized()
    {
        if (ResizeRequested && !_resizeScheduled.exchange(true))
        {
            ResizeRequested();
        }
    }

    TmuxGatewayConnection::TmuxGatewayConnection(std::shared_ptr<TmuxSession> session) :
        _session{ std::move(session) }
    {
        _session->PassthroughOutput = [weak = get_weak()](const winrt::array_view<const char16_t> bytes) {
            if (const auto self = weak.get())
            {
                self->TerminalOutput.raise(bytes);
            }
        };
        _session->TerminalOutput = [weak = get_weak()](const std::string_view bytes) {
            if (const auto self = weak.get())
            {
                std::wstring text;
                {
                    std::lock_guard lock{ self->_decodeMutex };
                    FAILED_LOG(til::u8u16(bytes, text, self->_utf8));
                }
                self->TerminalOutput.raise(winrt_wstring_to_array_view(text));
            }
        };
        _session->StateChanged = [weak = get_weak()] {
            if (const auto self = weak.get())
            {
                self->StateChanged.raise(*self, nullptr);
            }
        };
    }

    TmuxGatewayConnection::~TmuxGatewayConnection()
    {
        _session->Close();
    }

    safe_void_coroutine TmuxGatewayConnection::final_release(std::unique_ptr<TmuxGatewayConnection> self)
    {
        co_await winrt::resume_background();
        self.reset();
    }

    void TmuxGatewayConnection::Start() { _session->Start(); }
    void TmuxGatewayConnection::Close() { _session->Close(); }
    void TmuxGatewayConnection::Resize(const uint32_t rows, const uint32_t columns) { _session->Transport().Resize(rows, columns); }
    winrt::guid TmuxGatewayConnection::SessionId() const noexcept { return _session->Transport().SessionId(); }
    ConnectionState TmuxGatewayConnection::State() const noexcept { return _session->Transport().State(); }

    void TmuxGatewayConnection::WriteInput(const winrt::array_view<const char16_t> bytes)
    {
        if (!_session->Active())
        {
            _session->Transport().WriteInput(bytes);
            return;
        }
        if (bytes.size() == 1)
        {
            if (bytes[0] == '\x1b')
            {
                _session->Command("detach-client");
            }
            else if (bytes[0] == 'n' || bytes[0] == 'N')
            {
                _session->Command("new-window");
            }
            else if (bytes[0] == 'X')
            {
                _session->ForceExit();
            }
        }
    }

    TmuxPaneConnection::TmuxPaneConnection(std::shared_ptr<TmuxSession> session, const uint32_t window, const uint32_t pane) :
        _session{ std::move(session) },
        _window{ window },
        _pane{ pane }
    {
    }

    void TmuxPaneConnection::Start()
    {
        auto expected = ConnectionState::NotConnected;
        if (_state.compare_exchange_strong(expected, ConnectionState::Connected))
        {
            StateChanged.raise(*this, nullptr);
            _session->Capture(_pane, get_weak());
        }
    }

    void TmuxPaneConnection::WriteInput(const winrt::array_view<const char16_t> bytes)
    {
        if (State() == ConnectionState::Connected)
        {
            _session->Input(_pane, til::u16u8(winrt_array_to_wstring_view(bytes)));
        }
    }

    void TmuxPaneConnection::Resize(const uint32_t rows, const uint32_t columns)
    {
        const auto size = (static_cast<uint64_t>(rows) << 32) | columns;
        if (_size.exchange(size) != size)
        {
            _session->PaneResized();
        }
    }

    void TmuxPaneConnection::Close()
    {
        // Connection teardown is not a request to kill the remote program.
        // Explicit close-pane/tab actions call Kill; exiting Terminal detaches.
        Disconnected();
        std::unique_lock lock{ _outputMutex };
        _output.clear();
        if (_drainThread != std::this_thread::get_id())
        {
            _outputIdle.wait(lock, [this] { return !_draining; });
        }
    }

    void TmuxPaneConnection::Disconnected()
    {
        if (_state.exchange(ConnectionState::Closed) != ConnectionState::Closed)
        {
            StateChanged.raise(*this, nullptr);
        }
    }

    void TmuxPaneConnection::Kill() { _session->KillPane(_pane); }
    void TmuxPaneConnection::Split(const bool horizontal, const bool before, const float size) { _session->Split(_pane, horizontal, before, size); }
    void TmuxPaneConnection::Focus() { _session->Focus(_window, _pane); }
    void TmuxPaneConnection::ResizeBy(const char direction) { _session->ResizePane(_pane, direction); }
    void TmuxPaneConnection::ToggleZoom() { _session->Command("resize-pane -Z -t %" + std::to_string(_pane)); }
    void TmuxPaneConnection::NewWindow() { _session->Command("new-window"); }

    void TmuxPaneConnection::Output(std::string bytes)
    {
        {
            std::lock_guard lock{ _outputMutex };
            if (State() != ConnectionState::Connected)
            {
                return;
            }
            _output.push_back(std::move(bytes));
            if (std::exchange(_draining, true))
            {
                return;
            }
            _drainThread = std::this_thread::get_id();
        }
        for (;;)
        {
            std::string next;
            {
                std::lock_guard lock{ _outputMutex };
                if (_output.empty())
                {
                    _draining = false;
                    _drainThread = {};
                    _outputIdle.notify_all();
                    return;
                }
                next = std::move(_output.front());
                _output.pop_front();
            }
            try
            {
                std::wstring text;
                if (SUCCEEDED(til::u8u16(next, text, _utf8)))
                {
                    TerminalOutput.raise(winrt_wstring_to_array_view(text));
                }
            }
            CATCH_LOG();
        }
    }
}
