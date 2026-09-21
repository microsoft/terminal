#pragma once

#include <algorithm>
#include <functional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace Microsoft::Terminal
{
    class TitleFormat
    {
    public:
        enum class Surface { Tab, Window };

        TitleFormat() = default;

        TitleFormat(const std::wstring_view text, const Surface surface)
        {
            std::wstring literal;
            const auto flush = [&] {
                if (!literal.empty())
                {
                    _tokens.push_back({ Kind::Literal, std::move(literal) });
                    literal.clear();
                }
            };
            for (size_t offset = 0; offset < text.size();)
            {
                if (text.substr(offset, 3) == L"$${")
                {
                    literal += L"${";
                    offset += 3;
                }
                else if (text.substr(offset, 2) == L"${")
                {
                    flush();
                    const auto end = text.find(L'}', offset + 2);
                    if (end == std::wstring_view::npos)
                    {
                        _valid = false;
                        return;
                    }
                    const auto name = text.substr(offset + 2, end - offset - 2);
                    const auto letter = [](const wchar_t value) {
                        return (value >= L'a' && value <= L'z') || (value >= L'A' && value <= L'Z');
                    };
                    const auto environment = name.substr(0, 4) == L"env:";
                    const auto validName = environment ? name.size() > 4 && name.find_first_of(L"{}") == std::wstring_view::npos :
                        !name.empty() && letter(name.front()) && std::all_of(name.begin(), name.end(), [&](const wchar_t value) {
                            return letter(value) || (value >= L'0' && value <= L'9');
                        });
                    if (!validName || (surface == Surface::Tab && name == L"tabTitle"))
                    {
                        _valid = false;
                        return;
                    }
                    if (name == L"separator")
                    {
                        _tokens.push_back({ Kind::Separator, {} });
                    }
                    else
                    {
                        static constexpr std::wstring_view names[]{ L"paneTitle", L"tabTitle", L"profileName", L"workspaceName", L"windowName", L"appName", L"tabIndex", L"numPanes", L"numOtherPanes", L"otherPanes", L"elevated", L"elevatedShield", L"shellName", L"shellPid", L"processName", L"cwd", L"cwdFolder" };
                        if (environment || std::find(std::begin(names), std::end(names), name) != std::end(names))
                        {
                            _dependencies.emplace(name);
                            _tokens.push_back({ Kind::Variable, std::wstring{ name } });
                        }
                        else
                        {
                            _unknown = true;
                        }
                    }
                    offset = end + 1;
                }
                else
                {
                    literal += text[offset++];
                }
            }
            flush();
        }

        bool Valid() const noexcept { return _valid; }
        static std::wstring DirectoryName(std::wstring_view path)
        {
            if (path.empty()) return {};
            const auto windows = (path.size() >= 2 && path[1] == L':') || path.substr(0, 2) == L"\\\\";
            const auto separators = windows ? L"/\\" : L"/";
            size_t rootLength = path.front() == L'/' ? 1 : 0;
            if (windows && path.size() >= 3 && path[1] == L':' && path.find_first_of(separators, 2) == 2)
            {
                rootLength = 3;
            }
            else if (path.substr(0, 2) == L"\\\\")
            {
                const auto serverEnd = path.find_first_of(separators, 2);
                const auto shareEnd = serverEnd == std::wstring_view::npos ? serverEnd : path.find_first_of(separators, serverEnd + 1);
                rootLength = shareEnd == std::wstring_view::npos ? path.size() : shareEnd + 1;
            }
            while (path.size() > rootLength && path.find_last_of(separators) == path.size() - 1)
            {
                path.remove_suffix(1);
            }
            if (path.size() <= rootLength) return std::wstring{ path };
            const auto last = path.find_last_of(separators);
            return std::wstring{ last == std::wstring_view::npos ? path : path.substr(last + 1) };
        }

        bool HasWarning() const noexcept { return !_valid || _unknown; }
        const std::set<std::wstring, std::less<>>& Dependencies() const noexcept { return _dependencies; }

        template<typename Resolver>
        std::wstring Evaluate(const Resolver& resolve, const std::wstring_view separator) const
        {
            if (!_valid)
            {
                return {};
            }
            std::wstring result;
            std::wstring segment;
            const auto flush = [&] {
                if (!segment.empty())
                {
                    if (!result.empty())
                    {
                        result += separator;
                    }
                    result += segment;
                    segment.clear();
                }
            };
            for (const auto& token : _tokens)
            {
                switch (token.kind)
                {
                case Kind::Literal:
                    segment += token.text;
                    break;
                case Kind::Variable:
                    segment += resolve(std::wstring_view{ token.text });
                    break;
                case Kind::Separator:
                    flush();
                    break;
                }
            }
            flush();
            for (auto& character : result)
            {
                if (character < L' ' || (character >= L'\x7f' && character <= L'\x9f') || character == L'\u2028' || character == L'\u2029')
                {
                    character = L' ';
                }
            }
            const auto first = result.find_first_not_of(L" \t\r\n\v\f");
            if (first == std::wstring::npos)
            {
                return {};
            }
            return result.substr(first, result.find_last_not_of(L" \t\r\n\v\f") - first + 1);
        }

    private:
        enum class Kind { Literal, Variable, Separator };
        struct Token
        {
            Kind kind;
            std::wstring text;
        };
        std::vector<Token> _tokens;
        std::set<std::wstring, std::less<>> _dependencies;
        bool _valid{ true };
        bool _unknown{ false };
    };
}