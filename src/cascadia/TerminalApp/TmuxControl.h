// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include <algorithm>
#include <charconv>
#include <cstdint>
#include <deque>
#include <functional>
#include <map>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace Microsoft::Terminal::Tmux
{
    inline uint32_t Number(const std::string_view text)
    {
        uint32_t value{};
        const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
        if (text.empty() || result.ec != std::errc{} || result.ptr != text.data() + text.size())
        {
            throw std::runtime_error("Invalid tmux number");
        }
        return value;
    }

    inline std::string_view Take(std::string_view& text, const char separator = ' ')
    {
        const auto end = text.find(separator);
        const auto result = text.substr(0, end);
        text = end == text.npos ? std::string_view{} : text.substr(end + 1);
        return result;
    }

    inline uint32_t Id(const std::string_view text, const char prefix)
    {
        if (text.empty() || text.front() != prefix)
        {
            throw std::runtime_error("Invalid tmux identifier");
        }
        return Number(text.substr(1));
    }

    inline void UnescapeAppend(std::string& result, std::string_view text)
    {
        while (!text.empty())
        {
            const auto escape = text.find('\\');
            result.append(text.substr(0, escape));
            if (escape == text.npos)
            {
                break;
            }
            text.remove_prefix(escape);
            if (text.size() >= 2 && text[1] == '\\')
            {
                result.push_back('\\');
                text.remove_prefix(2);
            }
            else if (text.size() >= 4 &&
                     text[1] >= '0' && text[1] <= '3' &&
                     text[2] >= '0' && text[2] <= '7' &&
                     text[3] >= '0' && text[3] <= '7')
            {
                result.push_back(static_cast<char>((text[1] - '0') * 64 + (text[2] - '0') * 8 + text[3] - '0'));
                text.remove_prefix(4);
            }
            else
            {
                result.push_back('\\');
                text.remove_prefix(1);
            }
        }
    }

    inline std::string Unescape(const std::string_view text)
    {
        std::string result;
        result.reserve(text.size());
        UnescapeAppend(result, text);
        return result;
    }

    struct Layout
    {
        enum class Kind
        {
            Leaf,
            LeftRight,
            TopBottom
        };

        Kind kind{ Kind::Leaf };
        uint32_t width{}, height{}, x{}, y{}, pane{};
        std::vector<Layout> children;

        static Layout Parse(const std::string_view value)
        {
            if (value.size() < 6 || value[4] != ',')
            {
                throw std::runtime_error("Invalid tmux layout");
            }
            uint32_t expected{};
            const auto result = std::from_chars(value.data(), value.data() + 4, expected, 16);
            uint16_t checksum{};
            for (const auto c : value.substr(5))
            {
                checksum = static_cast<uint16_t>((checksum >> 1) | ((checksum & 1) << 15));
                checksum = static_cast<uint16_t>(checksum + static_cast<unsigned char>(c));
            }
            if (result.ec != std::errc{} || result.ptr != value.data() + 4 || expected != checksum)
            {
                throw std::runtime_error("Invalid tmux layout checksum");
            }
            auto text = value.substr(5);
            size_t cells{};
            auto layout = _Parse(text, 0, cells);
            if (!text.empty())
            {
                throw std::runtime_error("Trailing tmux layout data");
            }
            return layout;
        }

        template<typename F>
        void Leaves(F&& callback) const
        {
            if (kind == Kind::Leaf)
            {
                callback(*this);
            }
            for (const auto& child : children)
            {
                child.Leaves(callback);
            }
        }

        template<typename F>
        std::pair<double, double> Measure(F&& measurePane) const
        {
            if (kind == Kind::Leaf)
            {
                return measurePane(pane);
            }
            std::pair<double, double> result{};
            for (const auto& child : children)
            {
                const auto size = child.Measure(measurePane);
                if (size.first <= 0 || size.second <= 0)
                {
                    return {};
                }
                if (kind == Kind::LeftRight)
                {
                    result.first += size.first + (result.first ? 1 : 0);
                    result.second = std::max(result.second, size.second);
                }
                else
                {
                    result.first = std::max(result.first, size.first);
                    result.second += size.second + (result.second ? 1 : 0);
                }
            }
            return result;
        }

    private:
        static uint32_t _Number(std::string_view& text, const char delimiter)
        {
            const auto end = text.find(delimiter);
            if (end == text.npos)
            {
                throw std::runtime_error("Incomplete tmux layout");
            }
            const auto value = Number(text.substr(0, end));
            text.remove_prefix(end + 1);
            return value;
        }

        static Layout _Parse(std::string_view& text, const size_t depth, size_t& cells)
        {
            if (depth > 64 || ++cells > 4096)
            {
                throw std::runtime_error("Tmux layout is too complex");
            }
            Layout node;
            node.width = _Number(text, 'x');
            node.height = _Number(text, ',');
            node.x = _Number(text, ',');
            const auto end = text.find_first_not_of("0123456789");
            node.y = Number(text.substr(0, end));
            text.remove_prefix(end == text.npos ? text.size() : end);
            if (!node.width || !node.height || node.width > 32767 || node.height > 32767 || text.empty())
            {
                throw std::runtime_error("Invalid tmux layout dimensions");
            }
            if (text.front() == ',')
            {
                text.remove_prefix(1);
                const auto paneEnd = text.find_first_not_of("0123456789");
                node.pane = Number(text.substr(0, paneEnd));
                text.remove_prefix(paneEnd == text.npos ? text.size() : paneEnd);
                return node;
            }
            const auto open = text.front();
            if (open != '{' && open != '[')
            {
                throw std::runtime_error("Invalid tmux layout split");
            }
            node.kind = open == '{' ? Kind::LeftRight : Kind::TopBottom;
            const auto close = open == '{' ? '}' : ']';
            do
            {
                text.remove_prefix(1);
                node.children.push_back(_Parse(text, depth + 1, cells));
            } while (!text.empty() && text.front() == ',');
            if (text.empty() || text.front() != close || node.children.size() < 2)
            {
                throw std::runtime_error("Incomplete tmux layout split");
            }
            text.remove_prefix(1);
            return node;
        }
    };

    struct Window
    {
        uint32_t id{}, activePane{};
        bool active{}, zoomed{};
        std::string name, layoutText;
        Layout layout;
    };

    struct Event
    {
        enum class Kind
        {
            Terminal,
            Enter,
            Exit,
            Output,
            Snapshot,
            Windows,
            Error
        };
        Kind kind{};
        uint32_t pane{};
        std::string data;
        std::vector<Window> windows;
    };

    // All methods are serialized by the connection, but none perform IO or call UI code.
    // In particular, sending input never waits for a preceding command's reply.
    class Client
    {
    public:
        explicit Client(const uint32_t historyLimit = 9001) :
            _historyLimit{ std::min(historyLimit, 100000u) }
        {
        }
        Client(const Client&) = delete;
        Client& operator=(const Client&) = delete;

        bool Active() const noexcept { return _active; }
        bool InspectingControlStream() const noexcept { return _control || !_probe.empty(); }
        bool HasPane(const uint32_t pane) const { return _panes.contains(pane); }

        std::vector<Event> TakeEvents() { return std::exchange(_events, {}); }
        std::vector<std::string> TakeWrites() { return std::exchange(_writes, {}); }

        void Feed(std::string_view bytes)
        {
            static constexpr std::string_view introducer{ "\x1bP1000p" };
            while (!bytes.empty())
            {
                if (_control && !_exiting)
                {
                    const auto end = bytes.find('\n');
                    const auto fragment = bytes.substr(0, end);
                    if (fragment.size() > 16 * 1024 * 1024 - _line.size())
                    {
                        throw std::runtime_error("Tmux control line exceeds 16 MiB");
                    }
                    if (end == bytes.npos)
                    {
                        _line.append(fragment);
                        return;
                    }
                    auto line = fragment;
                    if (!_line.empty())
                    {
                        _line.append(fragment);
                        line = _line;
                    }
                    if (!line.empty() && line.back() == '\r')
                    {
                        line.remove_suffix(1);
                    }
                    _Line(line);
                    _line.clear();
                    bytes.remove_prefix(end + 1);
                    continue;
                }
                const auto c = bytes.front();
                bytes.remove_prefix(1);
                if (!_control)
                {
                    if (!_probe.empty() || c == '\x1b')
                    {
                        _probe.push_back(c);
                        while (!introducer.starts_with(_probe))
                        {
                            _Terminal(std::string_view{ _probe }.substr(0, 1));
                            _probe.erase(0, 1);
                        }
                        if (_probe == introducer)
                        {
                            _probe.clear();
                            _control = _active = true;
                            _events.push_back({ Event::Kind::Enter, 0, {}, {} });
                        }
                    }
                    else
                    {
                        _Terminal(std::string_view{ &c, 1 });
                    }
                    continue;
                }
                if (_exiting)
                {
                    _probe.push_back(c);
                    if (_probe == "\x1b\\")
                    {
                        _probe.clear();
                        _control = _exiting = false;
                    }
                    else if (!std::string_view{ "\x1b\\" }.starts_with(_probe))
                    {
                        // -CC may append CR/LF before the ST.
                        _probe = c == '\x1b' ? "\x1b" : "";
                    }
                    continue;
                }
            }
        }

        void Disconnect()
        {
            if (_active)
            {
                _active = false;
                _panes.clear();
                _commands.clear();
                _writes.clear();
                _resizeMode = ResizeMode::Unknown;
                _requestedSizes.clear();
                _sentSizes.clear();
                _globalSize.reset();
                _deferredCaptures.clear();
                _statusRows = 1;
                _events.push_back({ Event::Kind::Exit, 0, {}, {} });
            }
        }

        void Abort(std::string message)
        {
            if (!_active)
            {
                return;
            }
            Disconnect();
            _inReply = false;
            _refreshPending = _refreshAgain = false;
            _reply.clear();
            _replySize = 0;
            _line.clear();
            _probe.clear();
            _exiting = true;
            _writes.clear();
            _writes.emplace_back("detach-client\n");
            _events.push_back({ Event::Kind::Error, 0, std::move(message), {} });
        }

        void ForceExit()
        {
            Disconnect();
            _control = _exiting = _inReply = _refreshPending = _refreshAgain = false;
            _line.clear();
            _probe.clear();
            _guard.clear();
            _reply.clear();
            _replySize = 0;
        }

        void Input(const uint32_t pane, const std::string_view bytes)
        {
            const auto state = _Pane(pane);
            if (!_active || !state)
            {
                return;
            }
            auto& input = state->input;
            input.append(bytes);
            const auto prefix = "send-keys -t %" + std::to_string(pane);
            auto command = prefix;
            size_t offset{}, count{};
            while (offset < input.size())
            {
                const auto lead = static_cast<unsigned char>(input[offset]);
                size_t length = lead < 0x80 ? 1 : lead >= 0xc2 && lead <= 0xdf ? 2 :
                                              lead >= 0xe0 && lead <= 0xef     ? 3 :
                                              lead >= 0xf0 && lead <= 0xf4     ? 4 :
                                                                                 1;
                if (offset + length > input.size())
                {
                    break;
                }
                uint32_t codepoint = lead < 0x80 ? lead : length == 1 ? 0xfffdu :
                                                                        lead & (0x7fu >> length);
                for (size_t i = 1; i < length; ++i)
                {
                    const auto next = static_cast<unsigned char>(input[offset + i]);
                    if ((next & 0xc0) != 0x80)
                    {
                        codepoint = 0xfffd;
                        length = 1;
                        break;
                    }
                    codepoint = (codepoint << 6) | (next & 0x3f);
                }
                if ((length == 2 && codepoint < 0x80) || (length == 3 && codepoint < 0x800) ||
                    (length == 4 && codepoint < 0x10000) || codepoint > 0x10ffff ||
                    (codepoint >= 0xd800 && codepoint <= 0xdfff))
                {
                    codepoint = 0xfffd;
                }
                offset += length;
                char hex[8];
                const auto encoded = std::to_chars(hex, hex + sizeof(hex), codepoint, 16);
                command += " 0x";
                command.append(hex, encoded.ptr);
                if (++count == 1024)
                {
                    Command(std::move(command));
                    command = prefix;
                    count = 0;
                }
            }
            input.erase(0, offset);
            if (count)
            {
                Command(std::move(command));
            }
        }

        void Command(std::string command)
        {
            if (_active)
            {
                _Request(std::move(command), {});
            }
        }

        void Split(const uint32_t pane, const bool horizontal, const bool before, const float fraction)
        {
            const auto state = _Pane(pane);
            if (!_active || !state || !(fraction > 0 && fraction < 1))
            {
                return;
            }
            const auto extent = horizontal ? state->width : state->height;
            const auto cells = std::max(1u, static_cast<uint32_t>(static_cast<float>(extent) * fraction));
            Command("split-window -t %" + std::to_string(pane) +
                    (horizontal ? " -h" : " -v") + (before ? " -b" : "") +
                    " -l " + std::to_string(cells));
        }

        void ResizeWindow(const uint32_t window, const uint32_t columns, const uint32_t rows)
        {
            if (!_active || !columns || !rows)
            {
                return;
            }
            const Size size{ std::min(columns, 32767u), std::min(rows, 32767u) };
            _requestedSizes[window] = size;
            if (_resizeMode == ResizeMode::Unknown)
            {
                _resizeMode = ResizeMode::Probing;
                _Request(_WindowSizeCommand(window, size), [this, window, size](const bool success, const auto&) {
                    _resizeMode = success ? ResizeMode::PerWindow : ResizeMode::Global;
                    if (success)
                    {
                        _sentSizes[window] = size;
                    }
                    _FlushSizes();
                    const auto captures = std::exchange(_deferredCaptures, {});
                    for (const auto pane : captures)
                    {
                        Capture(pane);
                    }
                });
            }
            else
            {
                _FlushSizes();
            }
        }

        void Refresh()
        {
            if (!_active)
            {
                return;
            }
            if (_refreshPending)
            {
                _refreshAgain = true;
                return;
            }
            _refreshPending = true;
            _Request("list-windows -F '#{window_id}\t#{pane_id}\t#{window_active}\t#{window_layout}\t#{window_zoomed_flag}\t#{status}\t#{window_name}'",
                     [this](const bool success, const auto& lines) {
                         _refreshPending = false;
                         if (success)
                         {
                             std::vector<Window> windows;
                             std::map<uint32_t, PaneState> panes;
                             for (auto line : lines)
                             {
                                 std::string_view text{ line };
                                 Window window;
                                 window.id = Id(Take(text, '\t'), '@');
                                 window.activePane = Id(Take(text, '\t'), '%');
                                 window.active = Take(text, '\t') == "1";
                                 window.layoutText = Take(text, '\t');
                                 window.zoomed = Take(text, '\t') == "1";
                                 const auto status = Take(text, '\t');
                                 _statusRows = status == "off" ? 0u : status == "on" || status.empty() ? 1u :
                                                                                                         std::min(Number(status), 5u);
                                 window.name = text;
                                 window.layout = Layout::Parse(window.layoutText);
                                 window.layout.Leaves([&](const auto& leaf) {
                                     if (panes.contains(leaf.pane))
                                     {
                                         throw std::runtime_error("Duplicate tmux pane");
                                     }
                                     auto old = _panes.find(leaf.pane);
                                     auto& state = panes.emplace(leaf.pane, old == _panes.end() ? PaneState{} : std::move(old->second)).first->second;
                                     state.width = leaf.width;
                                     state.height = leaf.height;
                                 });
                                 windows.push_back(std::move(window));
                             }
                             _panes = std::move(panes);
                             for (auto it = _requestedSizes.begin(); it != _requestedSizes.end();)
                             {
                                 if (std::none_of(windows.begin(), windows.end(), [&](const auto& window) { return window.id == it->first; }))
                                 {
                                     _sentSizes.erase(it->first);
                                     it = _requestedSizes.erase(it);
                                 }
                                 else
                                 {
                                     ++it;
                                 }
                             }
                             _FlushSizes();
                             _events.push_back({ Event::Kind::Windows, 0, {}, std::move(windows) });
                         }
                         if (std::exchange(_refreshAgain, false))
                         {
                             Refresh();
                         }
                     });
        }

        void Capture(const uint32_t pane)
        {
            auto it = _panes.find(pane);
            if (!_active || it == _panes.end() || it->second.capturing || it->second.ready)
            {
                return;
            }
            if (_resizeMode == ResizeMode::Probing)
            {
                _deferredCaptures.insert(pane);
                return;
            }
            it->second.capturing = true;
            const auto target = " -t %" + std::to_string(pane);
            const auto batchStart = _writes.size();
            // tmux runs this command sequence without yielding. Output preceding
            // the capture is represented by the snapshot, not replayed twice.
            _Request("display-message -p" + target +
                         " '#{cursor_x};#{cursor_y};#{scroll_region_upper};#{scroll_region_lower};"
                         "#{cursor_flag};#{insert_flag};#{keypad_cursor_flag};#{keypad_flag};#{wrap_flag};"
                         "#{origin_flag};#{mouse_all_flag};#{mouse_button_flag};#{mouse_standard_flag};"
                         "#{mouse_sgr_flag};#{bracket_paste_flag};#{alternate_on};"
                         "#{alternate_saved_x};#{alternate_saved_y};#{mouse_utf8_flag}'",
                     [this, pane](const bool success, const auto& lines) {
                         if (auto state = _Pane(pane))
                         {
                             if (!success || lines.size() != 1)
                             {
                                 _CaptureFailed(pane);
                                 return;
                             }
                             state->modes = lines.front();
                         }
                     });
            _Request("capture-pane -p -e -C -J -a -q -S -" + std::to_string(_historyLimit) + target,
                     [this, pane](const bool success, const auto& lines) {
                         if (auto state = _Pane(pane))
                         {
                             if (!success)
                             {
                                 _CaptureFailed(pane);
                                 return;
                             }
                             state->saved = _Screen(lines);
                         }
                     });
            _Request("capture-pane -p -e -C -J -S -" + std::to_string(_historyLimit) + target,
                     [this, pane](const bool success, const auto& lines) {
                         if (auto state = _Pane(pane))
                         {
                             if (!success)
                             {
                                 _CaptureFailed(pane);
                                 return;
                             }
                             state->screen = _Screen(lines);
                             state->tail.clear();
                         }
                     });
            _Request("capture-pane -p -P -C" + target,
                     [this, pane](const bool success, const auto& lines) {
                         if (auto state = _Pane(pane); state && state->capturing)
                         {
                             if (!success)
                             {
                                 _CaptureFailed(pane);
                                 return;
                             }
                             auto snapshot = _Restore(*state);
                             for (const auto& line : lines)
                             {
                                 snapshot += Unescape(line);
                             }
                             snapshot += state->tail;
                             state->ready = true;
                             state->capturing = false;
                             state->screen.clear();
                             state->saved.clear();
                             state->tail.clear();
                             _events.push_back({ Event::Kind::Snapshot, pane, std::move(snapshot), {} });
                         }
                     });
            std::string batch;
            for (size_t i = batchStart; i < _writes.size(); ++i)
            {
                if (!batch.empty())
                {
                    batch += " ; ";
                }
                batch.append(_writes[i], 0, _writes[i].size() - 1);
            }
            batch.push_back('\n');
            _writes.resize(batchStart);
            _writes.push_back(std::move(batch));
        }

    private:
        struct PaneState
        {
            bool capturing{}, ready{};
            uint32_t width{}, height{};
            std::string modes, screen, saved, tail, input;
        };
        enum class ResizeMode
        {
            Unknown,
            Probing,
            PerWindow,
            Global
        };
        using Size = std::pair<uint32_t, uint32_t>;
        ResizeMode _resizeMode{ ResizeMode::Unknown };
        std::map<uint32_t, Size> _requestedSizes, _sentSizes;
        std::optional<Size> _globalSize;
        std::set<uint32_t> _deferredCaptures;
        uint32_t _statusRows{ 1 };
        using Reply = std::function<void(bool, const std::vector<std::string>&)>;
        uint32_t _historyLimit;
        bool _control{}, _active{}, _exiting{}, _refreshPending{}, _refreshAgain{};
        std::string _probe, _line, _guard;
        bool _inReply{}, _commandReply{};
        size_t _replySize{};
        std::vector<std::string> _reply;
        std::deque<Reply> _commands;
        std::map<uint32_t, PaneState> _panes;
        std::vector<Event> _events;
        std::vector<std::string> _writes;

        static std::string _WindowSizeCommand(const uint32_t window, const Size size)
        {
            return "refresh-client -C @" + std::to_string(window) + ":" +
                   std::to_string(size.first) + "x" + std::to_string(size.second);
        }

        void _FlushSizes()
        {
            if (_resizeMode == ResizeMode::PerWindow)
            {
                for (const auto& [window, size] : _requestedSizes)
                {
                    if (!_sentSizes.contains(window) || _sentSizes.at(window) != size)
                    {
                        _sentSizes[window] = size;
                        _Request(_WindowSizeCommand(window, size), {});
                    }
                }
            }
            else if (_resizeMode == ResizeMode::Global && !_requestedSizes.empty())
            {
                auto size = _requestedSizes.begin()->second;
                for (const auto& entry : _requestedSizes)
                {
                    size.first = std::min(size.first, entry.second.first);
                    size.second = std::min(size.second, entry.second.second);
                }
                // Legacy -C describes the whole client, including its status rows.
                size.second = std::min(32767u, size.second + _statusRows);
                if (_globalSize != size)
                {
                    _globalSize = size;
                    _Request("refresh-client -C " + std::to_string(size.first) + "," + std::to_string(size.second), {});
                }
            }
        }

        void _Terminal(const std::string_view bytes)
        {
            if (_events.empty() || _events.back().kind != Event::Kind::Terminal)
            {
                _events.push_back({ Event::Kind::Terminal, 0, {}, {} });
            }
            _events.back().data += bytes;
        }

        void _Request(std::string command, Reply reply)
        {
            if (command.find_first_of("\r\n") != command.npos)
            {
                throw std::runtime_error("Newline in tmux command");
            }
            _commands.push_back(std::move(reply));
            command.push_back('\n');
            _writes.push_back(std::move(command));
        }

        PaneState* _Pane(const uint32_t pane)
        {
            const auto it = _panes.find(pane);
            return it == _panes.end() ? nullptr : &it->second;
        }

        void _CaptureFailed(const uint32_t pane)
        {
            if (auto state = _Pane(pane))
            {
                state->capturing = false;
            }
            _events.push_back({ Event::Kind::Error, pane, "Unable to restore tmux pane", {} });
        }

        static std::string _Screen(const std::vector<std::string>& lines)
        {
            std::string text;
            bool first = true;
            for (const auto& line : lines)
            {
                if (!std::exchange(first, false))
                {
                    text += "\r\n";
                }
                text += Unescape(line);
            }
            return text;
        }

        static std::string _Restore(const PaneState& state)
        {
            std::vector<uint32_t> modes;
            std::string_view values{ state.modes };
            for (;;)
            {
                const auto separator = values.find(';');
                const auto value = values.substr(0, separator);
                modes.push_back(value.empty() ? 0 : Number(value));
                if (separator == values.npos)
                {
                    break;
                }
                values.remove_prefix(separator + 1);
            }
            if (modes.size() != 19)
            {
                throw std::runtime_error("Incomplete tmux pane state");
            }
            const auto cursor = [](const uint32_t x, const uint32_t y) {
                return "\x1b[" + std::to_string(y + 1) + ";" + std::to_string(x + 1) + "H";
            };
            std::string text{ "\x1b"
                              "c" };
            if (modes[15])
            {
                text += state.saved;
                text += cursor(modes[16], modes[17]);
                text += "\x1b[?1049h";
            }
            text += state.screen;
            text += "\x1b[0m\x1b[" + std::to_string(modes[2] + 1) + ";" + std::to_string(modes[3] + 1) + "r";
            const std::pair<size_t, uint32_t> flags[]{
                { 4, 25 }, { 6, 1 }, { 8, 7 }, { 9, 6 }, { 10, 1003 }, { 11, 1002 }, { 12, 1000 }, { 13, 1006 }, { 14, 2004 }, { 18, 1005 }
            };
            for (const auto& [index, flag] : flags)
            {
                text += "\x1b[?" + std::to_string(flag) + (modes[index] ? "h" : "l");
            }
            text += modes[5] ? "\x1b[4h" : "\x1b[4l";
            text += modes[7] ? "\x1b=" : "\x1b>";
            text += cursor(modes[0], modes[9] && modes[1] >= modes[2] ? modes[1] - modes[2] : modes[1]);
            return text;
        }

        void _Line(const std::string_view line)
        {
            if (_inReply)
            {
                if (line == "%end " + _guard || line == "%error " + _guard)
                {
                    _inReply = false;
                    if (_commandReply)
                    {
                        if (_commands.empty())
                        {
                            throw std::runtime_error("Unexpected tmux command response");
                        }
                        auto callback = std::move(_commands.front());
                        _commands.pop_front();
                        if (callback)
                        {
                            callback(line.starts_with("%end "), _reply);
                        }
                        else if (line.starts_with("%error "))
                        {
                            _events.push_back({ Event::Kind::Error, 0, _reply.empty() ? "Tmux command failed" : _reply.front(), {} });
                        }
                    }
                    else
                    {
                        if (line.starts_with("%end "))
                        {
                            Refresh();
                        }
                        else
                        {
                            _events.push_back({ Event::Kind::Error, 0, _reply.empty() ? "Tmux attachment failed" : _reply.front(), {} });
                        }
                    }
                    _reply.clear();
                    _replySize = 0;
                }
                else
                {
                    _replySize += line.size();
                    if (_replySize > 64 * 1024 * 1024)
                    {
                        throw std::runtime_error("Tmux command response exceeds 64 MiB");
                    }
                    _reply.emplace_back(line);
                }
                return;
            }
            if (line.starts_with("%begin "))
            {
                _guard = line.substr(7);
                auto fields = std::string_view{ _guard };
                Number(Take(fields));
                Number(Take(fields));
                _commandReply = (Number(fields) & 1) != 0;
                _inReply = true;
                return;
            }
            std::string_view arguments{ line };
            const auto name = Take(arguments);
            if (name == "%output" || name == "%extended-output")
            {
                const auto pane = Id(Take(arguments), '%');
                if (name == "%extended-output")
                {
                    const auto colon = arguments.find(" : ");
                    if (colon == arguments.npos)
                    {
                        throw std::runtime_error("Invalid tmux extended output");
                    }
                    arguments.remove_prefix(colon + 3);
                }
                if (auto state = _Pane(pane))
                {
                    if (state->ready)
                    {
                        // Batch only bytes already received, preserving pane and
                        // lifecycle ordering without delaying interactive output.
                        if (_events.empty() || _events.back().kind != Event::Kind::Output ||
                            _events.back().pane != pane ||
                            _events.back().data.size() + arguments.size() > 64 * 1024)
                        {
                            _events.push_back({ Event::Kind::Output, pane, {}, {} });
                            _events.back().data.reserve(arguments.size());
                        }
                        UnescapeAppend(_events.back().data, arguments);
                    }
                    else if (state->capturing)
                    {
                        auto output = Unescape(arguments);
                        if (state->tail.size() + output.size() > 16 * 1024 * 1024)
                        {
                            throw std::runtime_error("Tmux pane initialization exceeds 16 MiB");
                        }
                        state->tail += output;
                    }
                }
            }
            else if (name == "%exit")
            {
                Disconnect();
                _exiting = true;
                _refreshPending = _refreshAgain = false;
            }
            else if (name == "%pause")
            {
                const auto pane = Id(Take(arguments), '%');
                if (auto state = _Pane(pane))
                {
                    state->ready = state->capturing = false;
                    Capture(pane);
                    Command("refresh-client -A '%" + std::to_string(pane) + ":continue'");
                }
            }
            else if (name == "%window-add" || name == "%window-close" || name == "%window-renamed" ||
                     name == "%layout-change" || name == "%session-changed" ||
                     name == "%session-window-changed" || name == "%window-pane-changed")
            {
                Refresh();
            }
        }
    };
}
