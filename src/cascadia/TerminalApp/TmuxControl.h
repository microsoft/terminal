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
        using StringHandler = std::function<bool(const wchar_t)>;
        using PrintHandler = std::function<void(const std::wstring_view)>;
        using StringHandlerProducer = std::function<StringHandler(PrintHandler)>;
        using SplitDirection = winrt::Microsoft::Terminal::Settings::Model::SplitDirection;

    public:
        TmuxControl(TerminalPage& page);
        StringHandler TmuxControlHandlerProducer(const winrt::Microsoft::Terminal::Control::TermControl control, const PrintHandler print);
        bool TabIsTmuxControl(const winrt::com_ptr<Tab>& tab);
        void SplitPane(const winrt::com_ptr<Tab>& tab, SplitDirection direction);

    private:
        enum class State : int
        {
            INIT,
            ATTACHING,
            ATTACHED,
        };

        enum class CommandState : int
        {
            READY,
            WAITING,
        };

        enum class EventType : int
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
            int sessionId{ -1 };
            int windowId{ -1 };
            int paneId{ -1 };

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

            int paneId{ -1 };
            int cursorX{ 0 };
            int cursorY{ 0 };
            int history{ 0 };
        };

        struct DiscoverPanes : public Command
        {
        public:
            std::wstring GetCommand() override;
            bool ResultHandler(const std::wstring& result, TmuxControl& tmux) override;

            int sessionId{ -1 };
            int windowId{ -1 };
            bool newWindow{ false };
        };

        struct DiscoverWindows : public Command
        {
        public:
            std::wstring GetCommand() override;
            bool ResultHandler(const std::wstring& result, TmuxControl& tmux) override;

            int sessionId{ -1 };
        };

        struct KillPane : public Command
        {
        public:
            std::wstring GetCommand() override;

            int paneId{ -1 };
        };

        struct KillWindow : public Command
        {
        public:
            std::wstring GetCommand() override;

            int windowId{ -1 };
        };

        struct ListPanes : public Command
        {
        public:
            std::wstring GetCommand() override;
            bool ResultHandler(const std::wstring& result, TmuxControl& tmux) override;

            int windowId{ -1 };
            int history{ 2000 };
        };

        struct ListWindow : public Command
        {
        public:
            std::wstring GetCommand() override;
            bool ResultHandler(const std::wstring& result, TmuxControl& tmux) override;

            int windowId{ -1 };
            int sessionId{ -1 };
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

            int width{ 0 };
            int height{ 0 };
            int paneId{ -1 };
        };

        struct ResizeWindow : public Command
        {
        public:
            std::wstring GetCommand() override;
            int width{ 0 };
            int height{ 0 };
            int windowId{ -1 };
        };

        struct SelectWindow : public Command
        {
        public:
            std::wstring GetCommand() override;

            int windowId{ -1 };
        };

        struct SelectPane : public Command
        {
        public:
            std::wstring GetCommand() override;

            int paneId{ -1 };
        };

        struct SendKey : public Command
        {
        public:
            std::wstring GetCommand() override;

            int paneId{ -1 };
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

            int paneId{ -1 };
            SplitDirection direction{ SplitDirection::Left };
        };

        // Layout structs
        enum TmuxLayoutType : int
        {
            SINGLE_PANE,
            SPLIT_HORIZONTAL,
            SPLIT_VERTICAL,
        };

        struct TmuxPaneLayout
        {
            int width;
            int height;
            int left;
            int top;
            int id;
        };

        struct TmuxWindowLayout
        {
            TmuxLayoutType type{ SINGLE_PANE };
            std::vector<TmuxPaneLayout> panes;
        };

        struct TmuxWindow
        {
            int sessionId{ -1 };
            int windowId{ -1 };
            int width{ 0 };
            int height{ 0 };
            int history{ 2000 };
            bool active{ false };
            std::wstring name;
            std::wstring layoutChecksum;
            std::vector<TmuxWindowLayout> layout;
        };

        struct TmuxPane
        {
            int sessionId;
            int windowId;
            int paneId;
            int cursorX;
            int cursorY;
            bool active;
        };

        struct AttachedPane
        {
            int windowId;
            int paneId;
            winrt::Microsoft::Terminal::Control::TermControl control;
            winrt::Microsoft::Terminal::TerminalConnection::TmuxConnection connection;
            bool initialized{ false };
        };

        // Private methods
        void _AttachSession();
        void _DetachSession();
        void _SetupProfile();
        void _CreateNewTabMenu();

        float _ComputeSplitSize(int newSize, int originSize, SplitDirection direction) const;
        TerminalApp::Tab _GetTab(int windowId) const;

        void _SendOutput(int paneId, const std::wstring& text);
        void _Output(int paneId, const std::wstring& result);
        void _CloseWindow(int windowId);
        void _RenameWindow(int windowId, const std::wstring& name);
        void _NewWindowFinalize(int windowId, int paneId, const std::wstring& windowName);
        void _SplitPaneFinalize(int windowId, int paneId);
        std::shared_ptr<Pane> _NewPane(int windowId, int paneId);

        bool _SyncPaneState(std::vector<TmuxPane> panes, int history);
        bool _SyncWindowState(std::vector<TmuxWindow> windows);
        std::vector<TmuxWindowLayout> _ParseTmuxWindowLayout(std::wstring& layout);

        void _EventHandler(const Event& e);
        void _Parse(const std::wstring& buffer);
        bool _Advance(wchar_t ch);

        // Tmux command methods
        void _AttachDone();
        void _CapturePane(int paneId, int cursorX, int cursorY, int history);
        void _DiscoverPanes(int sessionId, int windowId, bool newWindow);
        void _DiscoverWindows(int sessionId);
        void _KillPane(int paneId);
        void _KillWindow(int windowId);
        void _ListWindow(int sessionId, int windowId);
        void _ListPanes(int windowId, int history);
        void _NewWindow();
        void _OpenNewTerminalViaDropdown();
        void _ResizePane(int paneId, int width, int height);
        void _ResizeWindow(int windowId, int width, int height);
        void _SelectPane(int paneId);
        void _SelectWindow(int windowId);
        void _SendKey(int paneId, const std::wstring keys);
        void _SetOption(const std::wstring& option);
        void _SplitPane(std::shared_ptr<Pane> pane, SplitDirection direction);

        void _CommandHandler(const std::wstring& result);
        void _SendCommand(std::unique_ptr<Command> cmd);
        void _ScheduleCommand();

        // Private variables
        State _state{ State::INIT };
        CommandState _cmdState{ CommandState::READY };
        Event _event;
        TerminalPage& _page;
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
        std::unordered_map<int, AttachedPane> _attachedPanes;
        std::unordered_map<int, TerminalApp::Tab> _attachedWindows;
        std::unordered_map<int, std::wstring> _outputBacklog;

        int _sessionId{ -1 };

        int _terminalWidth{ 0 };
        int _terminalHeight{ 0 };

        float _fontWidth{ 0 };
        float _fontHeight{ 0 };

        ::winrt::Windows::UI::Xaml::Thickness _thickness{ 0, 0, 0, 0 };

        std::pair<std::shared_ptr<Pane>, SplitDirection> _splittingPane{ nullptr, SplitDirection::Right };

        int _activePaneId{ -1 };
        int _activeWindowId{ -1 };

        std::function<void(const std::wstring_view string)> _Print;
        bool _inUse{ false };
        std::mutex _inUseMutex;
    };
}
