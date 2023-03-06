#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <conio.h>

#include <array>

#include <wil/resource.h>

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

// wprintf() in the uCRT prints every single wchar_t individually and thus breaks surrogate
// pairs apart which Windows Terminal treats as invalid input and replaces it with U+FFFD.
static void printfUTF16(_In_z_ _Printf_format_string_ wchar_t const* const format, ...)
{
    wchar_t buffer[128];

    va_list args;
    va_start(args, format);
    const auto length = _vsnwprintf_s(buffer, _countof(buffer), _TRUNCATE, format, args);
    va_end(args);

    WriteConsoleW(GetStdHandle(STD_OUTPUT_HANDLE), buffer, length, nullptr, nullptr);
}

static void writeAsciiWithAttribute(WORD attribute, const wchar_t* text)
{
    const auto outputHandle = GetStdHandle(STD_OUTPUT_HANDLE);
    const auto length = static_cast<DWORD>(wcslen(text));

    CONSOLE_SCREEN_BUFFER_INFO info{};
    GetConsoleScreenBufferInfo(outputHandle, &info);

    WORD attributes[128];
    std::fill_n(&attributes[0], length, static_cast<WORD>(attribute | FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_RED));

    DWORD numberOfAttrsWritten;
    WriteConsoleW(outputHandle, text, length, nullptr, nullptr);
    WriteConsoleOutputAttribute(outputHandle, attributes, length, info.dwCursorPosition, &numberOfAttrsWritten);
}

static void wait()
{
    printfUTF16(L"\r\nPress any key to continue...");
    _getch();
}

static void clear()
{
    printfUTF16(
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

    printfUTF16(
        L"\x1b[?1049h" // enable alternative screen buffer
        L"\x1B[H" // move cursor to 0,0
    );
    defer
    {
        printfUTF16(
            L"\x1b[?1049l" // disable alternative screen buffer
        );
    };

    {
        struct Test
        {
            WORD attribute = 0;
            const wchar_t* text = nullptr;
        };
        static constexpr Test tests[]{
#define MAKE_TEST_FOR_ATTRIBUTE(attr) Test{ attr, L## #attr }
            MAKE_TEST_FOR_ATTRIBUTE(COMMON_LVB_GRID_HORIZONTAL),
            MAKE_TEST_FOR_ATTRIBUTE(COMMON_LVB_GRID_LVERTICAL),
            MAKE_TEST_FOR_ATTRIBUTE(COMMON_LVB_GRID_RVERTICAL),
            MAKE_TEST_FOR_ATTRIBUTE(COMMON_LVB_REVERSE_VIDEO),
            MAKE_TEST_FOR_ATTRIBUTE(COMMON_LVB_UNDERSCORE),
#undef MAKE_TEST_FOR_ATTRIBUTE
        };

        for (const auto& t : tests)
        {
            printfUTF16(L"\r\n    ");
            writeAsciiWithAttribute(t.attribute, t.text);
            printfUTF16(L"\r\n    ");
        }
    }

    wait();
    clear();

    {
        struct Test
        {
            WORD sgr = 0;
            const wchar_t* name = nullptr;
        };
        static constexpr Test tests[]{
            { 3, L"italic" },
            { 4, L"underline" },
            { 7, L"reverse" },
            { 9, L"strikethrough" },
            { 21, L"double underline" },
            { 53, L"overlined" },
        };

        for (const auto& t : tests)
        {
            printfUTF16(L"\r\n    \x1b[%dm%s \\x1b[%dm\x1b[m\r\n    ", t.sgr, t.name, t.sgr);
        }

        printfUTF16(L"\r\n    \x1b]8;;https://example.com\x1b\\hyperlink \\x1b]8;;https://example.com\\x1b\\\\hyperlink\\x1b]8;;\\x1b\\\\\x1b]8;;\x1b\\\r\n    ");
    }

    wait();
    return 0;
}
