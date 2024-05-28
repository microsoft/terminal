#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <conio.h>

#include <array>
#include <cassert>
#include <string_view>

// The following list of colors is only used as a debug aid and not part of the final product.
// They're licensed under:
//
//   Apache-Style Software License for ColorBrewer software and ColorBrewer Color Schemes
//
//   Copyright (c) 2002 Cynthia Brewer, Mark Harrower, and The Pennsylvania State University.
//
//   Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file except in compliance with the License.
//   You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
//   Unless required by applicable law or agreed to in writing, software distributed
//   under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
//   CONDITIONS OF ANY KIND, either express or implied. See the License for the
//   specific language governing permissions and limitations under the License.
//
namespace colorbrewer
{
    inline constexpr uint32_t pastel1[]{
        0xfbb4ae,
        0xb3cde3,
        0xccebc5,
        0xdecbe4,
        0xfed9a6,
        0xffffcc,
        0xe5d8bd,
        0xfddaec,
        0xf2f2f2,
    };
}

// Another variant of "defer" for C++.
namespace
{
    namespace detail
    {

        template<typename F>
        class scope_guard
        {
        public:
            scope_guard(F f) noexcept :
                func(std::move(f))
            {
            }

            ~scope_guard()
            {
                func();
            }

            scope_guard(const scope_guard&) = delete;
            scope_guard(scope_guard&& rhs) = delete;
            scope_guard& operator=(const scope_guard&) = delete;
            scope_guard& operator=(scope_guard&&) = delete;

        private:
            F func;
        };

        enum class scope_guard_helper
        {
        };

        template<typename F>
        scope_guard<F> operator+(scope_guard_helper /*unused*/, F&& fn)
        {
            return scope_guard<F>(std::forward<F>(fn));
        }

    } // namespace detail

// The extra indirection is necessary to prevent __LINE__ to be treated literally.
#define _DEFER_CONCAT_IMPL(a, b) a##b
#define _DEFER_CONCAT(a, b) _DEFER_CONCAT_IMPL(a, b)
#define defer const auto _DEFER_CONCAT(_defer_, __LINE__) = ::detail::scope_guard_helper() + [&]()
}

static void printUTF16(const wchar_t* str)
{
    WriteConsoleW(GetStdHandle(STD_OUTPUT_HANDLE), str, static_cast<DWORD>(wcslen(str)), nullptr, nullptr);
}

// wprintf() in the uCRT prints every single wchar_t individually and thus breaks surrogate
// pairs apart which Windows Terminal treats as invalid input and replaces it with U+FFFD.
static void printfUTF16(_In_z_ _Printf_format_string_ wchar_t const* const format, ...)
{
    std::array<wchar_t, 128> buffer;

    va_list args;
    va_start(args, format);
    const auto length = _vsnwprintf_s(buffer.data(), buffer.size(), _TRUNCATE, format, args);
    va_end(args);

    assert(length >= 0);
    WriteConsoleW(GetStdHandle(STD_OUTPUT_HANDLE), buffer.data(), length, nullptr, nullptr);
}

static void wait()
{
    printUTF16(L"\x1b[9999;1HPress any key to continue...");
    _getch();
}

static void clear()
{
    printUTF16(
        L"\x1b[H" // move cursor to 0,0
        L"\x1b[2J" // clear screen
    );
}

int main()
{
    const auto outputHandle = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD consoleMode = ENABLE_PROCESSED_OUTPUT | ENABLE_WRAP_AT_EOL_OUTPUT;
    GetConsoleMode(outputHandle, &consoleMode);
    SetConsoleMode(outputHandle, consoleMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING | DISABLE_NEWLINE_AUTO_RETURN);
    defer
    {
        SetConsoleMode(outputHandle, consoleMode);
    };

    printUTF16(
        L"\x1b[?1049h" // enable alternative screen buffer
    );
    defer
    {
        printUTF16(
            L"\x1b[?1049l" // disable alternative screen buffer
        );
    };

    {
        struct AttributeTest
        {
            const wchar_t* text = nullptr;
            WORD attribute = 0;
        };

        {
            static constexpr AttributeTest consoleAttributeTests[]{
                { L"Console attributes:", 0 },
#define MAKE_TEST_FOR_ATTRIBUTE(attr) { L## #attr, attr }
                MAKE_TEST_FOR_ATTRIBUTE(COMMON_LVB_GRID_HORIZONTAL),
                MAKE_TEST_FOR_ATTRIBUTE(COMMON_LVB_GRID_LVERTICAL),
                MAKE_TEST_FOR_ATTRIBUTE(COMMON_LVB_GRID_RVERTICAL),
                MAKE_TEST_FOR_ATTRIBUTE(COMMON_LVB_REVERSE_VIDEO),
                MAKE_TEST_FOR_ATTRIBUTE(COMMON_LVB_UNDERSCORE),
#undef MAKE_TEST_FOR_ATTRIBUTE
                { L"all gridlines", COMMON_LVB_GRID_HORIZONTAL | COMMON_LVB_GRID_LVERTICAL | COMMON_LVB_GRID_RVERTICAL | COMMON_LVB_UNDERSCORE },
                { L"all attributes", COMMON_LVB_GRID_HORIZONTAL | COMMON_LVB_GRID_LVERTICAL | COMMON_LVB_GRID_RVERTICAL | COMMON_LVB_REVERSE_VIDEO | COMMON_LVB_UNDERSCORE },
            };

            SHORT row = 2;
            for (const auto& t : consoleAttributeTests)
            {
                const auto length = static_cast<DWORD>(wcslen(t.text));
                printfUTF16(L"\x1b[%d;5H%s", row + 1, t.text);

                WORD attributes[32];
                std::fill_n(&attributes[0], length, static_cast<WORD>(FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_RED | t.attribute));

                DWORD numberOfAttrsWritten;
                WriteConsoleOutputAttribute(outputHandle, attributes, length, { 4, row }, &numberOfAttrsWritten);

                row += 2;
            }
        }

        {
            static constexpr AttributeTest basicSGR[]{
                { L"bold", 1 },
                { L"faint", 2 },
                { L"italic", 3 },
                { L"underline", 4 },
                { L"reverse", 7 },
                { L"strikethrough", 9 },
                { L"double underline", 21 },
                { L"overlined", 53 },
            };

            printfUTF16(L"\x1b[3;39HANSI escape SGR:");

            int row = 5;
            for (const auto& t : basicSGR)
            {
                printfUTF16(L"\x1b[%d;39H\x1b[%dm%s\x1b[m", row, t.attribute, t.text);
                row += 2;
            }

            printfUTF16(L"\x1b[%d;39H\x1b]8;;https://example.com\x1b\\hyperlink\x1b]8;;\x1b\\", row);
        }

        {
            static constexpr AttributeTest styledUnderlines[]{
                { L"straight", 1 },
                { L"double", 2 },
                { L"curly", 3 },
                { L"dotted", 4 },
                { L"dashed", 5 },
            };

            printfUTF16(L"\x1b[3;63HStyled Underlines:");

            int row = 5;
            for (const auto& t : styledUnderlines)
            {
                printfUTF16(L"\x1b[%d;63H\x1b[4:%dm", row, t.attribute);

                const auto len = wcslen(t.text);
                for (size_t i = 0; i < len; ++i)
                {
                    const auto color = colorbrewer::pastel1[i % std::size(colorbrewer::pastel1)];
                    printfUTF16(L"\x1b[58:2::%d:%d:%dm%c", (color >> 16) & 0xff, (color >> 8) & 0xff, color & 0xff, t.text[i]);
                }

                printfUTF16(L"\x1b[m");
                row += 2;
            }
        }

        wait();
        clear();
    }

    {
        printUTF16(
            L"\x1b[3;5HDECDWL Double Width \U0001FAE0 \x1b[45;92mA\u0353\u0353\x1b[m B\u036F\u036F"
            L"\x1b[4;3H\x1b#6DECDWL Double Width         \U0001FAE0 \x1b[45;92mA\u0353\u0353\x1b[m B\u036F\u036F"
            L"\x1b[7;5HDECDHL Double Height \U0001F952\U0001F6C1 A\u0353\u0353 \x1b[45;92mB\u036F\u036F\x1b[m \x1b[45;92mX\u0353\u0353\x1b[m Y\u036F\u036F"
            L"\x1b[8;3H\x1b#3DECDHL Double Height Top    \U0001F952 A\u0353\u0353 \x1b[45;92mB\u036F\u036F\x1b[m"
            L"\x1b[9;3H\x1b#4DECDHL Double Height Bottom \U0001F6C1 \x1b[45;92mX\u0353\u0353\x1b[m Y\u036F\u036F"
            L"\x1b[12;5H\x1b]8;;https://example.com\x1b\\DECDxL\x1b]8;;\x1b\\ <\x1b[45;92m!\x1b[m-- \x1b[3;4:3;58:2::255:0:0mita\x1b[58:2::0:255:0mlic\x1b[m        \x1b[4munderline\x1b[m        \x1b[7mreverse\x1b[m"
            L"\x1b[14;5H\x1b]8;;https://example.com\x1b\\DECDxL\x1b]8;;\x1b\\ <\x1b[45;92m!\x1b[m-- \x1b[9mstrikethrough\x1b[m \x1b[21mdouble underline\x1b[m \x1b[53moverlined\x1b[m"
            L"\x1b[16;3H\x1b#6\x1b]8;;https://vt100.net/docs/vt510-rm/DECDWL.html\x1b\\DECDWL\x1b]8;;\x1b\\ <\x1b[45;92m!\x1b[m-- \x1b[3;4:3;58:2::255:0:0mita\x1b[58:2::0:255:0mlic\x1b[m        \x1b[4munderline\x1b[m        \x1b[7mreverse\x1b[m"
            L"\x1b[18;3H\x1b#6\x1b]8;;https://vt100.net/docs/vt510-rm/DECDWL.html\x1b\\DECDWL\x1b]8;;\x1b\\ <\x1b[45;92m!\x1b[m-- \x1b[9mstrikethrough\x1b[m \x1b[21mdouble underline\x1b[m \x1b[53moverlined\x1b[m"
            L"\x1b[20;3H\x1b#3\x1b]8;;https://vt100.net/docs/vt510-rm/DECDHL.html\x1b\\DECDHL\x1b]8;;\x1b\\ <\x1b[45;92m!\x1b[m-- \x1b[3;4:3;58:2::255:0:0mita\x1b[58:2::0:255:0mlic\x1b[m        \x1b[4munderline\x1b[m        \x1b[7mreverse\x1b[m"
            L"\x1b[21;3H\x1b#4\x1b]8;;https://vt100.net/docs/vt510-rm/DECDHL.html\x1b\\DECDHL\x1b]8;;\x1b\\ <\x1b[45;92m!\x1b[m-- \x1b[3;4:3;58:2::255:0:0mita\x1b[58:2::0:255:0mlic\x1b[m        \x1b[4munderline\x1b[m        \x1b[7mreverse\x1b[m"
            L"\x1b[23;3H\x1b#3\x1b]8;;https://vt100.net/docs/vt510-rm/DECDHL.html\x1b\\DECDHL\x1b]8;;\x1b\\ <\x1b[45;92m!\x1b[m-- \x1b[9mstrikethrough\x1b[m \x1b[21mdouble underline\x1b[m \x1b[53moverlined\x1b[m"
            L"\x1b[24;3H\x1b#4\x1b]8;;https://vt100.net/docs/vt510-rm/DECDHL.html\x1b\\DECDHL\x1b]8;;\x1b\\ <\x1b[45;92m!\x1b[m-- \x1b[9mstrikethrough\x1b[m \x1b[21mdouble underline\x1b[m \x1b[53moverlined\x1b[m");

        static constexpr WORD attributes[]{
            FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_RED | COMMON_LVB_GRID_HORIZONTAL,
            FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_RED | COMMON_LVB_GRID_HORIZONTAL,
            FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_RED | COMMON_LVB_GRID_LVERTICAL,
            FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_RED | COMMON_LVB_GRID_LVERTICAL,
            FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_RED | COMMON_LVB_GRID_RVERTICAL,
            FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_RED | COMMON_LVB_GRID_RVERTICAL,
            FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_RED | COMMON_LVB_UNDERSCORE,
            FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_RED | COMMON_LVB_UNDERSCORE,
        };

        DWORD numberOfAttrsWritten;
        DWORD offset = 0;

        for (const auto r : { 11, 13, 15, 17, 19, 20, 22, 23 })
        {
            COORD coord;
            coord.X = r > 14 ? 2 : 4;
            coord.X += offset ? 2 : 0;
            coord.Y = static_cast<SHORT>(r);
            WriteConsoleOutputAttribute(outputHandle, &attributes[offset], 4, coord, &numberOfAttrsWritten);
            offset = (offset + 4) & 7;
        }

        wait();
        clear();
    }

    {
        defer
        {
            // Setting an empty DRCS gets us back to the regular font.
            printUTF16(L"\x1bP1;1;2{ @\x1b\\");
        };

        constexpr auto width = 14;
        const auto glyph =
            "W   W         "
            "W   W         "
            "W W W         "
            "W W W         "
            "W W W         "
            "W W W  TTTTTTT"
            " W W      T   "
            "          T   "
            "          T   "
            "          T   "
            "          T   "
            "          T   ";

        // Convert the above visual glyph to sixels
        wchar_t rows[2][width];
        for (int r = 0; r < 2; ++r)
        {
            const auto glyphData = &glyph[r * width * 6];

            for (int x = 0; x < width; ++x)
            {
                unsigned int accumulator = 0;
                for (int y = 5; y >= 0; --y)
                {
                    const auto isSet = glyphData[y * width + x] != ' ';
                    accumulator <<= 1;
                    accumulator |= static_cast<unsigned int>(isSet);
                }

                rows[r][x] = static_cast<wchar_t>(L'?' + accumulator);
            }
        }

        // DECDLD - Dynamically Redefinable Character Sets
        printfUTF16(
            // * Pfn  | font number             | 1    |
            // * Pcn  | starting character      | 3    | = ASCII 0x23 "#"
            // * Pe   | erase control           | 2    | erase all
            //   Pcmw | character matrix width  | %d   | `width` pixels
            //   Pw   | font width              | 0    | 80 columns
            //   Pt   | text or full cell       | 0    | text
            //   Pcmh | character matrix height | 0    | 12 pixels
            //   Pcss | character set size      | 0    | 94
            // * Dscs | character set name      | " @" | unregistered soft set
            L"\x1bP1;3;2;%d{ @%.15s/%.15s\x1b\\",
            width,
            rows[0],
            rows[1]);

#define DRCS_SEQUENCE L"\x1b( @#\x1b(A"
        printUTF16(
            L"\x1b[3;5HDECDLD and DRCS test - it should show \"WT\" in a single cell"
            L"\x1b[5;5HRegular: " DRCS_SEQUENCE L""
            L"\x1b[7;3H\x1b#6DECDWL: " DRCS_SEQUENCE L""
            L"\x1b[9;3H\x1b#3DECDHL: " DRCS_SEQUENCE L""
            L"\x1b[10;3H\x1b#4DECDHL: " DRCS_SEQUENCE L""
            // We map soft fonts into the private use area starting at U+EF20. This test ensures
            // that we correctly map actual fallback glyphs mixed into the DRCS glyphs.
            L"\x1b[12;5HUnicode Fallback: \uE000\uE001" DRCS_SEQUENCE L"\uE003\uE004");
#undef DRCS_SEQUENCE

        wait();
    }

    return 0;
}
