// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include <til/mutex.h>

#include "Pane.h"

namespace winrt::TerminalApp::implementation
{
    struct TerminalPage;

    class TmuxControl : public std::enable_shared_from_this<TmuxControl>
    {
    public:
        TmuxControl(TerminalPage& page);

        bool AcquireSingleUseLock(winrt::Microsoft::Terminal::Control::TermControl control) noexcept;
        bool TabIsTmuxControl(const winrt::com_ptr<Tab>& tab);
        void SplitPane(const winrt::com_ptr<Tab>& tab, winrt::Microsoft::Terminal::Settings::Model::SplitDirection direction);
        void FeedInput(std::wstring_view str);

    private:
        enum class State
        {
            Init,
            Attaching,
            Attached,
        };

        enum class ResponseInfoType
        {
            Ignore,
            DiscoverNewWindow,
            DiscoverWindows,
            CapturePane,
            DiscoverPanes,
        };

        struct ResponseInfo
        {
            ResponseInfoType type;
            union
            {
                struct
                {
                    int64_t paneId;
                } capturePane;
            } data;
        };

        enum class TmuxLayoutType
        {
            // A single leaf pane
            Pane,
            // Indicates the start of a horizontal split layout
            PushHorizontal,
            // Indicates the start of a vertical split layout
            PushVertical,
            // Indicates the end of the most recent split layout
            Pop,
        };

        struct TmuxLayout
        {
            TmuxLayoutType type = TmuxLayoutType::Pane;

            // Only set for: Pane, PushHorizontal, PushVertical
            til::CoordType width = 0;
            // Only set for: Pane, PushHorizontal, PushVertical
            til::CoordType height = 0;
            // Only set for: Pane
            int64_t id = -1;
        };

        // AttachedPane should not need to be copied. Anything else would be a mistake.
        // But if we added a constructor to it, we could not use designated initializers anymore.
        // This marker makes it possible.
        struct MoveOnlyMarker
        {
            MoveOnlyMarker() = default;
            MoveOnlyMarker(MoveOnlyMarker&&) = default;
            MoveOnlyMarker& operator=(MoveOnlyMarker&&) = default;
            MoveOnlyMarker(const MoveOnlyMarker&) = delete;
            MoveOnlyMarker& operator=(const MoveOnlyMarker&) = delete;
        };

        struct AttachedPane
        {
            int64_t windowId = -1;
            int64_t paneId = -1;
            winrt::Microsoft::Terminal::TerminalConnection::TmuxConnection connection{ nullptr };
            winrt::Microsoft::Terminal::Control::TermControl control{ nullptr };
            std::wstring outputBacklog;
            bool initialized = false;
            bool ignoreOutput = false;

            [[msvc::no_unique_address]] MoveOnlyMarker moveOnlyMarker;
        };

        safe_void_coroutine _parseLine(std::wstring line);

        void _handleAttach(); // A special case of _handleResponse()
        void _handleDetach();
        void _handleSessionChanged(int64_t sessionId);
        void _handleWindowAdd(int64_t windowId);
        void _handleWindowRenamed(int64_t windowId, winrt::hstring name);
        void _handleWindowClose(int64_t windowId);
        void _handleWindowPaneChanged(int64_t windowId, int64_t paneId);
        void _handleLayoutChange(int64_t windowId, std::wstring_view layout);
        void _handleResponse(std::wstring_view result);

        void _sendSetOption(std::wstring_view option);
        void _sendDiscoverWindows(int64_t sessionId);
        void _handleResponseDiscoverWindows(std::wstring_view response);
        void _sendDiscoverNewWindow(int64_t windowId);
        void _handleResponseDiscoverNewWindow(std::wstring_view response);
        void _sendCapturePane(int64_t paneId, til::CoordType history);
        void _handleResponseCapturePane(const ResponseInfo& info, std::wstring_view response);
        void _sendDiscoverPanes(int64_t windowId);
        void _handleResponseDiscoverPanes(std::wstring_view response);
        void _sendNewWindow();
        void _sendKillWindow(int64_t windowId);
        void _sendKillPane(int64_t paneId);
        void _sendSplitPane(std::shared_ptr<Pane> pane, winrt::Microsoft::Terminal::Settings::Model::SplitDirection direction);
        void _sendSelectWindow(int64_t windowId);
        void _sendSelectPane(int64_t paneId);
        void _sendResizeWindow(int64_t windowId, til::CoordType width, til::CoordType height);
        void _sendResizePane(int64_t paneId, til::CoordType width, til::CoordType height);
        void _sendSendKey(int64_t paneId, const std::wstring_view keys);

        void _sendIgnoreResponse(wil::zwstring_view cmd);
        void _sendWithResponseInfo(wil::zwstring_view cmd, ResponseInfo info);

        std::shared_ptr<Pane> _layoutCreateRecursive(int64_t windowId, std::wstring_view& remaining, TmuxLayout parent);
        std::wstring_view _layoutStripHash(std::wstring_view str);
        TmuxLayout _layoutParseNextToken(std::wstring_view& remaining);

        void _deliverOutputToPane(int64_t paneId, const std::wstring_view text);
        winrt::com_ptr<Tab> _getTab(int64_t windowId) const;
        void _newTab(int64_t windowId, winrt::hstring name, std::shared_ptr<Pane> pane);
        std::pair<AttachedPane&, std::shared_ptr<Pane>> _newPane(int64_t windowId, int64_t paneId);
        void _openNewTerminalViaDropdown();

        TerminalPage& _page; // Non-owning, because TerminalPage owns us
        winrt::Windows::System::DispatcherQueue _dispatcherQueue{ nullptr };
        winrt::Windows::UI::Xaml::Controls::MenuFlyoutItem _newTabMenu;

        winrt::Microsoft::Terminal::Control::TermControl _control{ nullptr };
        winrt::com_ptr<Tab> _controlTab{ nullptr };
        winrt::Microsoft::Terminal::Settings::Model::Profile _profile{ nullptr };
        State _state = State::Init;
        bool _inUse = false;

        std::wstring _lineBuffer;
        std::wstring _responseBuffer;
        bool _insideOutputBlock = false;

        winrt::event_token _detachKeyDownRevoker;
        winrt::event_token _windowSizeChangedRevoker;
        winrt::event_token _newTabClickRevoker;

        std::deque<ResponseInfo> _commandQueue;
        std::unordered_map<int64_t, AttachedPane> _attachedPanes;
        std::unordered_map<int64_t, winrt::com_ptr<Tab>> _attachedWindows;

        int64_t _sessionId = -1;
        int64_t _activePaneId = -1;
        int64_t _activeWindowId = -1;

        til::CoordType _terminalWidth = 0;
        til::CoordType _terminalHeight = 0;
        winrt::Windows::UI::Xaml::Thickness _thickness{ 0, 0, 0, 0 };
        float _fontWidth = 0;
        float _fontHeight = 0;

        std::pair<std::shared_ptr<Pane>, winrt::Microsoft::Terminal::Settings::Model::SplitDirection> _splittingPane{
            nullptr,
            winrt::Microsoft::Terminal::Settings::Model::SplitDirection::Right,
        };
    };
}
