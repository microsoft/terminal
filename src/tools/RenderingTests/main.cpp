#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <conio.h>

#include <array>
#include <cassert>

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
    printUTF16(L"\x1B[9999;1HPress any key to continue...");
    _getch();
}

static void clear()
{
    printUTF16(
        L"\x1B[H" // move cursor to 0,0
        L"\x1B[2J" // clear screen
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
        struct ConsoleAttributeTest
        {
            const wchar_t* text = nullptr;
            WORD attribute = 0;
        };
        static constexpr ConsoleAttributeTest consoleAttributeTests[]{
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
            printfUTF16(L"\x1B[%d;5H%s", row + 1, t.text);

            WORD attributes[32];
            std::fill_n(&attributes[0], length, static_cast<WORD>(FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_RED | t.attribute));

            DWORD numberOfAttrsWritten;
            WriteConsoleOutputAttribute(outputHandle, attributes, length, { 4, row }, &numberOfAttrsWritten);

            row += 2;
        }

        struct VTAttributeTest
        {
            const wchar_t* text = nullptr;
            int sgr = 0;
        };
        static constexpr VTAttributeTest vtAttributeTests[]{
            { L"ANSI escape SGR:", 0 },
            { L"bold", 1 },
            { L"faint", 2 },
            { L"italic", 3 },
            { L"underline", 4 },
            { L"reverse", 7 },
            { L"strikethrough", 9 },
            { L"double underline", 21 },
            { L"overlined", 53 },
        };

        row = 3;
        for (const auto& t : vtAttributeTests)
        {
            printfUTF16(L"\x1B[%d;45H\x1b[%dm%s\x1b[m", row, t.sgr, t.text);
            row += 2;
        }

        printfUTF16(L"\x1B[%d;45H\x1b]8;;https://example.com\x1b\\hyperlink\x1b]8;;\x1b\\", row);

        wait();
        clear();
    }

    {
        printUTF16(
            L"\x1B[3;5HDECDWL Double Width \U0001FAE0 \x1B[45;92mA\u0353\u0353\x1B[m B\u036F\u036F"
            L"\x1B[4;3H\x1b#6DECDWL Double Width         \U0001FAE0 \x1B[45;92mA\u0353\u0353\x1B[m B\u036F\u036F"
            L"\x1B[7;5HDECDHL Double Height \U0001F952\U0001F6C1 A\u0353\u0353 \x1B[45;92mB\u036F\u036F\x1B[m \x1B[45;92mX\u0353\u0353\x1B[m Y\u036F\u036F"
            L"\x1B[8;3H\x1b#3DECDHL Double Height Top    \U0001F952 A\u0353\u0353 \x1B[45;92mB\u036F\u036F\x1B[m"
            L"\x1B[9;3H\x1b#4DECDHL Double Height Bottom \U0001F6C1 \x1B[45;92mX\u0353\u0353\x1B[m Y\u036F\u036F"
            L"\x1B[13;5H\x1b]8;;https://example.com\x1b\\DECDxL\x1b]8;;\x1b\\ <\x1B[45;92m!\x1B[m-- \x1B[3mitalic\x1b[m        \x1b[4munderline\x1b[m        \x1b[7mreverse\x1b[m"
            L"\x1B[15;5H\x1b]8;;https://example.com\x1b\\DECDxL\x1b]8;;\x1b\\ <\x1B[45;92m!\x1B[m-- \x1b[9mstrikethrough\x1b[m \x1b[21mdouble underline\x1b[m \x1b[53moverlined\x1b[m"
            L"\x1B[17;3H\x1b#6\x1b]8;;https://vt100.net/docs/vt510-rm/DECDWL.html\x1b\\DECDWL\x1b]8;;\x1b\\ <\x1B[45;92m!\x1B[m-- \x1B[3mitalic\x1b[m        \x1b[4munderline\x1b[m        \x1b[7mreverse\x1b[m"
            L"\x1B[19;3H\x1b#6\x1b]8;;https://vt100.net/docs/vt510-rm/DECDWL.html\x1b\\DECDWL\x1b]8;;\x1b\\ <\x1B[45;92m!\x1B[m-- \x1b[9mstrikethrough\x1b[m \x1b[21mdouble underline\x1b[m \x1b[53moverlined\x1b[m"
            L"\x1B[21;3H\x1b#3\x1b]8;;https://vt100.net/docs/vt510-rm/DECDHL.html\x1b\\DECDHL\x1b]8;;\x1b\\ <\x1B[45;92m!\x1B[m-- \x1B[3mitalic\x1b[m        \x1b[4munderline\x1b[m        \x1b[7mreverse\x1b[m"
            L"\x1B[22;3H\x1b#4\x1b]8;;https://vt100.net/docs/vt510-rm/DECDHL.html\x1b\\DECDHL\x1b]8;;\x1b\\ <\x1B[45;92m!\x1B[m-- \x1B[3mitalic\x1b[m        \x1b[4munderline\x1b[m        \x1b[7mreverse\x1b[m"
            L"\x1B[24;3H\x1b#3\x1b]8;;https://vt100.net/docs/vt510-rm/DECDHL.html\x1b\\DECDHL\x1b]8;;\x1b\\ <\x1B[45;92m!\x1B[m-- \x1b[9mstrikethrough\x1b[m \x1b[21mdouble underline\x1b[m \x1b[53moverlined\x1b[m"
            L"\x1B[25;3H\x1b#4\x1b]8;;https://vt100.net/docs/vt510-rm/DECDHL.html\x1b\\DECDHL\x1b]8;;\x1b\\ <\x1B[45;92m!\x1B[m-- \x1b[9mstrikethrough\x1b[m \x1b[21mdouble underline\x1b[m \x1b[53moverlined\x1b[m");

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

        for (const auto r : { 12, 14, 16, 18, 20, 21, 23, 24 })
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
            L"\x1B[3;5HDECDLD and DRCS test - it should show \"WT\" in a single cell"
            L"\x1B[5;5HRegular: " DRCS_SEQUENCE L""
            L"\x1B[7;3H\x1b#6DECDWL: " DRCS_SEQUENCE L""
            L"\x1B[9;3H\x1b#3DECDHL: " DRCS_SEQUENCE L""
            L"\x1B[10;3H\x1b#4DECDHL: " DRCS_SEQUENCE L""
            // We map soft fonts into the private use area starting at U+EF20. This test ensures
            // that we correctly map actual fallback glyphs mixed into the DRCS glyphs.
            L"\x1B[12;5HUnicode Fallback: \uE000\uE001" DRCS_SEQUENCE L"\uE003\uE004");
#undef DRCS_SEQUENCE

        wait();
    }

    return 0;
}
