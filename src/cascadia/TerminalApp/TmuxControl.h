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

        bool AcquireSingleUseLock(winrt::Microsoft::Terminal::Control::TermControl control) noexcept;
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

        enum class ResponseInfoType
        {
            Ignore,
            DiscoverNewWindow,
            DiscoverWindows,
            DiscoverPanes,
            ListWindow,
            ListPanes,
            CapturePane,
        };

        struct ResponseInfo
        {
            ResponseInfoType type;
            union
            {
                struct
                {
                    til::CoordType history;
                } listPanes;
                struct
                {
                    int64_t paneId;
                    til::CoordType cursorX;
                    til::CoordType cursorY;
                } capturePane;
            } data;
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
            int64_t id;
            til::CoordType width;
            til::CoordType height;
        };

        struct TmuxWindowLayout
        {
            TmuxLayoutType type = TmuxLayoutType::SINGLE_PANE;
            til::CoordType width;
            til::CoordType height;
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

        static std::vector<TmuxControl::TmuxWindowLayout> _parseLayout(std::wstring_view remaining);
        static std::vector<TmuxControl::TmuxWindowLayout> _ParseTmuxWindowLayout(std::wstring layout);

        safe_void_coroutine _parseLine(std::wstring str);

        void _handleAttach();
        void _handleDetach();
        void _handleOutput(int64_t paneId, const std::wstring_view result);
        void _handleWindowRenamed(int64_t windowId, const std::wstring_view name);
        void _handleWindowClose(int64_t windowId);
        void _handleWindowPaneChanged(int64_t windowId, int64_t paneId);
        void _handleResponse(std::wstring_view result);

        void _sendSetOption(std::wstring_view option);
        void _sendDiscoverWindows(int64_t sessionId);
        void _handleResponseDiscoverWindows(std::wstring_view response);
        void _sendDiscoverNewWindow(int64_t windowId);
        void _handleResponseDiscoverNewWindow(std::wstring_view response);
        void _sendDiscoverPanes(int64_t sessionId);
        void _handleResponseDiscoverPanes(std::wstring_view response);
        void _sendListWindow(int64_t sessionId);
        void _handleResponseListWindow(std::wstring_view response);
        void _sendListPanes(int64_t windowId, til::CoordType history);
        void _handleResponseListPanes(const ResponseInfo& info, std::wstring_view response);
        void _sendCapturePane(int64_t paneId, til::CoordType cursorX, til::CoordType cursorY, til::CoordType history);
        void _handleResponseCapturePane(const ResponseInfo& info, std::wstring response);
        void _sendNewWindow();
        void _sendKillWindow(int64_t windowId);
        void _sendKillPane(int64_t paneId);
        void _sendSplitPane(std::shared_ptr<Pane> pane, winrt::Microsoft::Terminal::Settings::Model::SplitDirection direction);
        void _sendSelectWindow(int64_t windowId);
        void _sendSelectPane(int64_t paneId);
        void _sendResizeWindow(int64_t windowId, til::CoordType width, til::CoordType height);
        void _sendResizePane(int64_t paneId, til::CoordType width, til::CoordType height);
        void _sendSendKey(int64_t paneId, const std::wstring_view keys);

        float _ComputeSplitSize(til::CoordType newSize, til::CoordType originSize, winrt::Microsoft::Terminal::Settings::Model::SplitDirection direction) const;
        winrt::com_ptr<Tab> _getTab(int64_t windowId) const;

        void _deliverOutputToPane(int64_t paneId, const std::wstring_view text);
        void _newWindowFinalize(int64_t windowId, int64_t paneId, const std::wstring_view windowName);
        std::shared_ptr<Pane> _newPane(int64_t windowId, int64_t paneId);

        // Tmux command methods
        void _OpenNewTerminalViaDropdown();

        void _sendIgnoreResponse(wil::zwstring_view cmd);
        void _sendWithResponseInfo(wil::zwstring_view cmd, ResponseInfo info);

        // Private variables
        TerminalPage& _page; // Non-owning, because TerminalPage owns us
        winrt::Windows::System::DispatcherQueue _dispatcherQueue{ nullptr };
        winrt::Microsoft::Terminal::Control::TermControl _control{ nullptr };
        winrt::com_ptr<Tab> _controlTab{ nullptr };
        winrt::Microsoft::Terminal::Settings::Model::Profile _profile{ nullptr };
        State _state{ State::INIT };
        winrt::Windows::UI::Xaml::Controls::MenuFlyoutItem _newTabMenu;

        std::wstring _lineBuffer;
        std::wstring _responseBuffer;

        winrt::event_token _detachKeyDownRevoker;
        winrt::event_token _windowSizeChangedRevoker;
        winrt::event_token _newTabClickRevoker;

        std::deque<ResponseInfo> _commandQueue;
        std::unordered_map<int64_t, AttachedPane> _attachedPanes;
        std::unordered_map<int64_t, winrt::com_ptr<Tab>> _attachedWindows;
        std::unordered_map<int64_t, std::wstring> _outputBacklog;

        int64_t _sessionId = -1;
        int64_t _activePaneId = -1;
        int64_t _activeWindowId = -1;

        til::CoordType _terminalWidth = 0;
        til::CoordType _terminalHeight = 0;

        float _fontWidth = 0;
        float _fontHeight = 0;

        winrt::Windows::UI::Xaml::Thickness _thickness{ 0, 0, 0, 0 };

        std::pair<std::shared_ptr<Pane>, winrt::Microsoft::Terminal::Settings::Model::SplitDirection> _splittingPane{ nullptr, winrt::Microsoft::Terminal::Settings::Model::SplitDirection::Right };

        std::atomic<bool> _inUse{ false };
        bool _insideOutputBlock = false;
    };
}
