// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include <regex>
#include <vector>
#include <unordered_map>
#include <functional>

#include "Pane.h"

namespace winrt::TerminalApp::implementation
{
    struct TerminalPage;

    class TmuxControl
    {
    public:
        TmuxControl(TerminalPage& page);

        bool AcquireSingleUseLock() noexcept;
        void FeedInput(std::wstring_view str);
        bool TabIsTmuxControl(const winrt::com_ptr<Tab>& tab);
        void SplitPane(const winrt::com_ptr<Tab>& tab, winrt::Microsoft::Terminal::Settings::Model::SplitDirection direction);

    private:
        enum class State
        {
            INIT,
            ATTACHING,
            ATTACHED,
        };

        enum class CommandState
        {
            READY,
            WAITING,
        };

        enum class EventType
        {
            BEGIN,
            END,
            ERR,

            ATTACH,
            DETACH,
            LAYOUT_CHANGED,
            NOTHING,
            OUTPUT,
            RESPONSE,
            SESSION_CHANGED,
            UNLINKED_WINDOW_CLOSE,
            WINDOW_ADD,
            WINDOW_CLOSE,
            WINDOW_PANE_CHANGED,
            WINDOW_RENAMED,
        };

        struct Event
        {
            EventType type{ EventType::NOTHING };
            int64_t sessionId{ -1 };
            int64_t windowId{ -1 };
            int64_t paneId{ -1 };

            std::wstring response;
        };

        // Command structs
        struct Command
        {
        public:
            virtual std::wstring GetCommand() = 0;
            virtual bool ResultHandler(const std::wstring& /*result*/, TmuxControl& /*tmux*/) { return true; };
        };

        struct AttachDone : public Command
        {
        public:
            std::wstring GetCommand() override;
            bool ResultHandler(const std::wstring& result, TmuxControl& tmux) override;
        };

        struct CapturePane : public Command
        {
        public:
            std::wstring GetCommand() override;
            bool ResultHandler(const std::wstring& result, TmuxControl& tmux) override;

            int64_t paneId{ -1 };
            til::CoordType cursorX{ 0 };
            til::CoordType cursorY{ 0 };
            til::CoordType history{ 0 };
        };

        struct DiscoverPanes : public Command
        {
        public:
            std::wstring GetCommand() override;
            bool ResultHandler(const std::wstring& result, TmuxControl& tmux) override;

            int64_t sessionId{ -1 };
            int64_t windowId{ -1 };
            bool newWindow{ false };
        };

        struct DiscoverWindows : public Command
        {
        public:
            std::wstring GetCommand() override;
            bool ResultHandler(const std::wstring& result, TmuxControl& tmux) override;

            int64_t sessionId{ -1 };
        };

        struct KillPane : public Command
        {
        public:
            std::wstring GetCommand() override;

            int64_t paneId{ -1 };
        };

        struct KillWindow : public Command
        {
        public:
            std::wstring GetCommand() override;

            int64_t windowId{ -1 };
        };

        struct ListPanes : public Command
        {
        public:
            std::wstring GetCommand() override;
            bool ResultHandler(const std::wstring& result, TmuxControl& tmux) override;

            int64_t windowId{ -1 };
            til::CoordType history{ 2000 };
        };

        struct ListWindow : public Command
        {
        public:
            std::wstring GetCommand() override;
            bool ResultHandler(const std::wstring& result, TmuxControl& tmux) override;

            int64_t windowId{ -1 };
            int64_t sessionId{ -1 };
        };

        struct NewWindow : public Command
        {
        public:
            std::wstring GetCommand() override;
        };

        struct ResizePane : public Command
        {
        public:
            std::wstring GetCommand() override;

            til::CoordType width{ 0 };
            til::CoordType height{ 0 };
            int64_t paneId{ -1 };
        };

        struct ResizeWindow : public Command
        {
        public:
            std::wstring GetCommand() override;
            til::CoordType width{ 0 };
            til::CoordType height{ 0 };
            int64_t windowId{ -1 };
        };

        struct SelectWindow : public Command
        {
        public:
            std::wstring GetCommand() override;

            int64_t windowId{ -1 };
        };

        struct SelectPane : public Command
        {
        public:
            std::wstring GetCommand() override;

            int64_t paneId{ -1 };
        };

        struct SendKey : public Command
        {
        public:
            std::wstring GetCommand() override;

            int64_t paneId{ -1 };
            std::wstring keys;
            wchar_t key{ '\0' };
        };

        struct SetOption : public Command
        {
        public:
            std::wstring GetCommand() override;

            std::wstring option;
        };

        struct SplitPane : public Command
        {
        public:
            std::wstring GetCommand() override;

            int64_t paneId{ -1 };
            winrt::Microsoft::Terminal::Settings::Model::SplitDirection direction{ winrt::Microsoft::Terminal::Settings::Model::SplitDirection::Left };
        };

        // Layout structs
        enum class TmuxLayoutType
        {
            SINGLE_PANE,
            SPLIT_HORIZONTAL,
            SPLIT_VERTICAL,
        };

        struct TmuxPaneLayout
        {
            til::CoordType width;
            til::CoordType height;
            til::CoordType left;
            til::CoordType top;
            int64_t id;
        };

        struct TmuxWindowLayout
        {
            TmuxLayoutType type = TmuxLayoutType::SINGLE_PANE;
            std::vector<TmuxPaneLayout> panes;
        };

        struct TmuxWindow
        {
            int64_t sessionId = 0;
            int64_t windowId = 0;
            til::CoordType width = 0;
            til::CoordType height = 0;
            til::CoordType history = 0;
            bool active = false;
            std::wstring name;
            std::wstring layoutChecksum;
            std::vector<TmuxWindowLayout> layout;
        };

        struct TmuxPane
        {
            int64_t sessionId = 0;
            int64_t windowId = 0;
            int64_t paneId = 0;
            til::CoordType cursorX = 0;
            til::CoordType cursorY = 0;
            bool active = false;
        };

        struct AttachedPane
        {
            int64_t windowId = 0;
            int64_t paneId = 0;
            winrt::Microsoft::Terminal::Control::TermControl control;
            winrt::Microsoft::Terminal::TerminalConnection::TmuxConnection connection;
            bool initialized = false;
        };

        // Private methods
        void _AttachSession();
        void _DetachSession();
        void _SetupProfile();
        void _CreateNewTabMenu();

        float _ComputeSplitSize(til::CoordType newSize, til::CoordType originSize, winrt::Microsoft::Terminal::Settings::Model::SplitDirection direction) const;
        winrt::com_ptr<Tab> _GetTab(int64_t windowId) const;

        void _SendOutput(int64_t paneId, const std::wstring& text);
        void _Output(int64_t paneId, const std::wstring& result);
        void _CloseWindow(int64_t windowId);
        void _RenameWindow(int64_t windowId, const std::wstring& name);
        void _NewWindowFinalize(int64_t windowId, int64_t paneId, const std::wstring& windowName);
        void _SplitPaneFinalize(int64_t windowId, int64_t paneId);
        std::shared_ptr<Pane> _NewPane(int64_t windowId, int64_t paneId);

        bool _SyncPaneState(std::vector<TmuxPane> panes, til::CoordType history);
        bool _SyncWindowState(std::vector<TmuxWindow> windows);

        void _EventHandler(const Event& e);
        void _Parse(const std::wstring& buffer);
        bool _Advance(wchar_t ch);

        // Tmux command methods
        void _AttachDone();
        void _CapturePane(int64_t paneId, til::CoordType cursorX, til::CoordType cursorY, til::CoordType history);
        void _DiscoverPanes(int64_t sessionId, int64_t windowId, bool newWindow);
        void _DiscoverWindows(int64_t sessionId);
        void _KillPane(int64_t paneId);
        void _KillWindow(int64_t windowId);
        void _ListWindow(int64_t sessionId, int64_t windowId);
        void _ListPanes(int64_t windowId, til::CoordType history);
        void _NewWindow();
        void _OpenNewTerminalViaDropdown();
        void _ResizePane(int64_t paneId, til::CoordType width, til::CoordType height);
        void _ResizeWindow(int64_t windowId, til::CoordType width, til::CoordType height);
        void _SelectPane(int64_t paneId);
        void _SelectWindow(int64_t windowId);
        void _SendKey(int64_t paneId, const std::wstring keys);
        void _SetOption(const std::wstring& option);
        void _SplitPane(std::shared_ptr<Pane> pane, winrt::Microsoft::Terminal::Settings::Model::SplitDirection direction);

        void _CommandHandler(const std::wstring& result);
        void _SendCommand(std::unique_ptr<Command> cmd);
        void _ScheduleCommand();

        // Private variables
        TerminalPage& _page; // Non-owning, because TerminalPage owns us
        State _state{ State::INIT };
        CommandState _cmdState{ CommandState::READY };
        Event _event;
        winrt::Microsoft::Terminal::Settings::Model::Profile _profile{ nullptr };
        winrt::Microsoft::Terminal::Control::TermControl _control{ nullptr };
        TerminalApp::Tab _controlTab{ nullptr };
        winrt::Windows::System::DispatcherQueue _dispatcherQueue{ nullptr };

        winrt::event_token _detachKeyDownRevoker;
        winrt::event_token _windowSizeChangedRevoker;
        winrt::event_token _newTabClickRevoker;

        ::winrt::Windows::UI::Xaml::Controls::MenuFlyoutItem _newTabMenu;

        std::vector<wchar_t> _dcsBuffer;
        std::deque<std::unique_ptr<TmuxControl::Command>> _cmdQueue;
        std::unordered_map<int64_t, AttachedPane> _attachedPanes;
        std::unordered_map<int64_t, winrt::com_ptr<Tab>> _attachedWindows;
        std::unordered_map<int64_t, std::wstring> _outputBacklog;

        int64_t _sessionId{ -1 };

        til::CoordType _terminalWidth{ 0 };
        til::CoordType _terminalHeight{ 0 };

        float _fontWidth{ 0 };
        float _fontHeight{ 0 };

        ::winrt::Windows::UI::Xaml::Thickness _thickness{ 0, 0, 0, 0 };

        std::pair<std::shared_ptr<Pane>, winrt::Microsoft::Terminal::Settings::Model::SplitDirection> _splittingPane{ nullptr, winrt::Microsoft::Terminal::Settings::Model::SplitDirection::Right };

        int64_t _activePaneId{ -1 };
        int64_t _activeWindowId{ -1 };

        std::function<void(const std::wstring_view string)> _Print;
        std::atomic<bool> _inUse{ false };
    };
}
