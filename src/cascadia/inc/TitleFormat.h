#pragma once

#include "TitleVariables.h"

#include <algorithm>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace Microsoft::Terminal
{
    class TitleFormat
    {
    public:
        enum class Surface
        {
            Tab,
            Window
        };

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
                    constexpr std::wstring_view environmentPrefix{ TitleVariables::EnvironmentPrefix };
                    const auto environment = name.starts_with(environmentPrefix);
                    const auto validName = environment ? name.size() > environmentPrefix.size() && name.find_first_of(L"{}") == std::wstring_view::npos :
                                                         !name.empty() && letter(name.front()) && std::all_of(name.begin(), name.end(), [&](const wchar_t value) {
                                                             return letter(value) || (value >= L'0' && value <= L'9');
                                                         });
                    if (!validName || (surface == Surface::Tab && name == TitleVariables::TabTitle))
                    {
                        _valid = false;
                        return;
                    }
                    if (name == TitleVariables::Separator)
                    {
                        _tokens.push_back({ Kind::Separator, {} });
                    }
                    else
                    {
                        static constexpr std::wstring_view names[]{
                            TitleVariables::PaneTitle, TitleVariables::TabTitle, TitleVariables::ProfileName, TitleVariables::WorkspaceName, TitleVariables::WindowName, TitleVariables::AppName, TitleVariables::TabIndex, TitleVariables::NumPanes, TitleVariables::NumOtherPanes, TitleVariables::OtherPanes, TitleVariables::Elevated, TitleVariables::ElevatedShield, TitleVariables::ShellName, TitleVariables::ShellPid, TitleVariables::Cwd, TitleVariables::CwdFolder
                        };
                        if (environment || std::find(std::begin(names), std::end(names), name) != std::end(names))
                        {
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
            if (path.empty())
                return {};
            const auto windows = (path.size() >= 2 && path[1] == L':') || path.front() == L'\\';
            const auto separators = windows ? L"/\\" : L"/";
            size_t rootLength = path.front() == L'/' || path.front() == L'\\' ? 1 : 0;
            const auto extended = path.starts_with(L"\\\\?\\");
            const auto extendedUnc = extended && path.size() >= 8 &&
                                     (path[4] == L'U' || path[4] == L'u') &&
                                     (path[5] == L'N' || path[5] == L'n') &&
                                     (path[6] == L'C' || path[6] == L'c') && path[7] == L'\\';
            const size_t driveOffset = extended ? 4 : 0;
            if (path.size() >= driveOffset + 2 && path[driveOffset + 1] == L':')
            {
                rootLength = driveOffset + 2;
                if (path.find_first_of(separators, rootLength) == rootLength)
                    ++rootLength;
            }
            else if (path.starts_with(L"\\\\"))
            {
                const auto serverEnd = path.find_first_of(separators, extendedUnc ? 8 : 2);
                const auto shareEnd = serverEnd == std::wstring_view::npos ? serverEnd : path.find_first_of(separators, serverEnd + 1);
                rootLength = shareEnd == std::wstring_view::npos ? path.size() : shareEnd + 1;
            }
            while (path.size() > rootLength && path.find_last_of(separators) == path.size() - 1)
            {
                path.remove_suffix(1);
            }
            if (path.size() <= rootLength)
                return std::wstring{ path };
            const auto last = path.find_last_of(separators);
            return std::wstring{ path.substr(last == std::wstring_view::npos ? rootLength : last + 1) };
        }

        bool HasWarning() const noexcept { return !_valid || _unknown; }

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
        enum class Kind
        {
            Literal,
            Variable,
            Separator
        };
        struct Token
        {
            Kind kind;
            std::wstring text;
        };
        std::vector<Token> _tokens;
        bool _valid{ true };
        bool _unknown{ false };
    };
}