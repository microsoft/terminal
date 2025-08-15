// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"

#include "arena.h"
#include "conhost.h"
#include "utils.h"

#define ENABLE_TEST_OUTPUT_WRITE 1
#define ENABLE_TEST_OUTPUT_SCROLL 1
#define ENABLE_TEST_OUTPUT_FILL 1
#define ENABLE_TEST_OUTPUT_READ 1
#define ENABLE_TEST_INPUT 1
#define ENABLE_TEST_CLIPBOARD 1

using Measurements = std::span<int32_t>;
using MeasurementsPerBenchmark = std::span<Measurements>;

struct BenchmarkContext
{
    bool wants_more() const;
    void mark_beg();
    void mark_end();
    size_t rand();

    HWND hwnd = nullptr;
    HANDLE input = nullptr;
    HANDLE output = nullptr;

    mem::Arena& arena;
    std::string_view utf8_4Ki;
    std::string_view utf8_128Ki;
    std::wstring_view utf16_4Ki;
    std::wstring_view utf16_128Ki;
    std::span<WORD> attr_4Ki;
    std::span<CHAR_INFO> char_4Ki;
    std::span<INPUT_RECORD> input_4Ki;

    Measurements m_measurements;
    size_t m_measurements_off = 0;
    int64_t m_time = 0;
    int64_t m_time_limit = 0;
    size_t m_rng_state = 0;
};

struct Benchmark
{
    const char* title;
    void (*exec)(BenchmarkContext& ctx);
};

struct AccumulatedResults
{
    size_t trace_count;
    // These are arrays of size trace_count
    std::string_view* trace_names;
    MeasurementsPerBenchmark* measurments;
};

static constexpr COORD s_buffer_size{ 120, 9001 };
static constexpr COORD s_viewport_size{ 120, 30 };

static constexpr Benchmark s_benchmarks[] = {
#if ENABLE_TEST_OUTPUT_WRITE
    Benchmark{
        .title = "WriteConsoleA 4Ki",
        .exec = [](BenchmarkContext& ctx) {
            while (ctx.wants_more())
            {
                ctx.mark_beg();
                const auto res = WriteConsoleA(ctx.output, ctx.utf8_4Ki.data(), static_cast<DWORD>(ctx.utf8_4Ki.size()), nullptr, nullptr);
                ctx.mark_end();
                debugAssert(res == TRUE);
            }
        },
    },
    Benchmark{
        .title = "WriteConsoleW 4Ki",
        .exec = [](BenchmarkContext& ctx) {
            while (ctx.wants_more())
            {
                ctx.mark_beg();
                const auto res = WriteConsoleW(ctx.output, ctx.utf16_4Ki.data(), static_cast<DWORD>(ctx.utf16_4Ki.size()), nullptr, nullptr);
                ctx.mark_end();
                debugAssert(res == TRUE);
            }
        },
    },
    Benchmark{
        .title = "WriteConsoleA 128Ki",
        .exec = [](BenchmarkContext& ctx) {
            while (ctx.wants_more())
            {
                ctx.mark_beg();
                const auto res = WriteConsoleA(ctx.output, ctx.utf8_128Ki.data(), static_cast<DWORD>(ctx.utf8_128Ki.size()), nullptr, nullptr);
                ctx.mark_end();
                debugAssert(res == TRUE);
            }
        },
    },
    Benchmark{
        .title = "WriteConsoleW 128Ki",
        .exec = [](BenchmarkContext& ctx) {
            while (ctx.wants_more())
            {
                ctx.mark_beg();
                const auto res = WriteConsoleW(ctx.output, ctx.utf16_128Ki.data(), static_cast<DWORD>(ctx.utf16_128Ki.size()), nullptr, nullptr);
                ctx.mark_end();
                debugAssert(res == TRUE);
            }
        },
    },
    Benchmark{
        .title = "WriteConsoleOutputAttribute 4Ki",
        .exec = [](BenchmarkContext& ctx) {
            static constexpr COORD pos{ 0, 0 };
            DWORD written;

            while (ctx.wants_more())
            {
                ctx.mark_beg();
                const auto res = WriteConsoleOutputAttribute(ctx.output, ctx.attr_4Ki.data(), static_cast<DWORD>(ctx.attr_4Ki.size()), pos, &written);
                ctx.mark_end();
                debugAssert(res == TRUE);
            }
        },
    },
    Benchmark{
        .title = "WriteConsoleOutputCharacterW 4Ki",
        .exec = [](BenchmarkContext& ctx) {
            static constexpr COORD pos{ 0, 0 };
            DWORD written;

            while (ctx.wants_more())
            {
                ctx.mark_beg();
                const auto res = WriteConsoleOutputCharacterW(ctx.output, ctx.utf16_4Ki.data(), static_cast<DWORD>(ctx.utf16_4Ki.size()), pos, &written);
                ctx.mark_end();
                debugAssert(res == TRUE);
            }
        },
    },
    Benchmark{
        .title = "WriteConsoleOutputW 4Ki",
        .exec = [](BenchmarkContext& ctx) {
            static constexpr COORD pos{ 0, 0 };
            static constexpr COORD size{ 64, 64 };
            static constexpr SMALL_RECT rect{ 0, 0, 63, 63 };

            while (ctx.wants_more())
            {
                auto written = rect;

                ctx.mark_beg();
                const auto res = WriteConsoleOutputW(ctx.output, ctx.char_4Ki.data(), size, pos, &written);
                ctx.mark_end();
                debugAssert(res == TRUE);
            }
        },
    },
#endif
#if ENABLE_TEST_OUTPUT_SCROLL
    Benchmark{
        .title = "ScrollConsoleScreenBufferW 4Ki",
        .exec = [](BenchmarkContext& ctx) {
            for (int i = 0; i < 10; i++)
            {
                WriteConsoleW(ctx.output, ctx.utf16_128Ki.data(), static_cast<DWORD>(ctx.utf16_128Ki.size()), nullptr, nullptr);
            }

            static constexpr CHAR_INFO fill{ L' ', FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_RED };
            static constexpr size_t w = 64;
            static constexpr size_t h = 64;

            while (ctx.wants_more())
            {
                auto r = ctx.rand();
                const auto srcLeft = (r >> 0) % (s_buffer_size.X - w);
                const auto srcTop = (r >> 16) % (s_buffer_size.Y - h);

                size_t dstLeft;
                size_t dstTop;
                do
                {
                    r = ctx.rand();
                    dstLeft = (r >> 0) % (s_buffer_size.X - w);
                    dstTop = (r >> 16) % (s_buffer_size.Y - h);
                } while (srcLeft == dstLeft && srcTop == dstTop);

                const SMALL_RECT scrollRect{
                    .Left = static_cast<SHORT>(srcLeft),
                    .Top = static_cast<SHORT>(srcTop),
                    .Right = static_cast<SHORT>(srcLeft + w - 1),
                    .Bottom = static_cast<SHORT>(srcTop + h - 1),
                };
                const COORD destOrigin{
                    .X = static_cast<SHORT>(dstLeft),
                    .Y = static_cast<SHORT>(dstTop),
                };

                ctx.mark_beg();
                const auto res = ScrollConsoleScreenBufferW(ctx.output, &scrollRect, nullptr, destOrigin, &fill);
                ctx.mark_end();
                debugAssert(res == TRUE);
            }
        },
    },
    Benchmark{
        .title = "ScrollConsoleScreenBufferW vertical",
        .exec = [](BenchmarkContext& ctx) {
            for (int i = 0; i < 10; i++)
            {
                WriteConsoleW(ctx.output, ctx.utf16_128Ki.data(), static_cast<DWORD>(ctx.utf16_128Ki.size()), nullptr, nullptr);
            }

            static constexpr CHAR_INFO fill{ L' ', FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_RED };
            static constexpr size_t h = (4096 + s_buffer_size.X / 2) / s_buffer_size.X;

            while (ctx.wants_more())
            {
                auto r = ctx.rand();
                const auto srcTop = r % (s_buffer_size.Y - h);

                size_t dstTop;
                do
                {
                    r = ctx.rand();
                    dstTop = r % (s_buffer_size.Y - h);
                } while (srcTop == dstTop);

                const SMALL_RECT scrollRect{
                    .Left = 0,
                    .Top = static_cast<SHORT>(srcTop),
                    .Right = s_buffer_size.X - 1,
                    .Bottom = static_cast<SHORT>(srcTop + h - 1),
                };
                const COORD destOrigin{
                    .X = 0,
                    .Y = static_cast<SHORT>(dstTop),
                };

                ctx.mark_beg();
                const auto res = ScrollConsoleScreenBufferW(ctx.output, &scrollRect, nullptr, destOrigin, &fill);
                ctx.mark_end();
                debugAssert(res == TRUE);
            }
        },
    },
#endif
#if ENABLE_TEST_OUTPUT_FILL
    Benchmark{
        .title = "FillConsoleOutputAttribute 4Ki",
        .exec = [](BenchmarkContext& ctx) {
            static constexpr COORD pos{ 0, 0 };
            DWORD written;

            while (ctx.wants_more())
            {
                ctx.mark_beg();
                FillConsoleOutputAttribute(ctx.output, FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_RED, 4096, pos, &written);
                ctx.mark_end();
                debugAssert(written == 4096);
            }
        },
    },
    Benchmark{
        .title = "FillConsoleOutputCharacterW 4Ki",
        .exec = [](BenchmarkContext& ctx) {
            static constexpr COORD pos{ 0, 0 };
            DWORD written;

            while (ctx.wants_more())
            {
                ctx.mark_beg();
                FillConsoleOutputCharacterW(ctx.output, L'A', 4096, pos, &written);
                ctx.mark_end();
                debugAssert(written == 4096);
            }
        },
    },
#endif
#if ENABLE_TEST_OUTPUT_READ
    Benchmark{
        .title = "ReadConsoleOutputAttribute 4Ki",
        .exec = [](BenchmarkContext& ctx) {
            static constexpr COORD pos{ 0, 0 };
            const auto scratch = mem::get_scratch_arena(ctx.arena);
            const auto buf = scratch.arena.push_uninitialized<WORD>(4096);
            DWORD read;

            WriteConsoleW(ctx.output, ctx.utf16_128Ki.data(), static_cast<DWORD>(ctx.utf16_128Ki.size()), nullptr, nullptr);

            while (ctx.wants_more())
            {
                ctx.mark_beg();
                ReadConsoleOutputAttribute(ctx.output, buf, 4096, pos, &read);
                ctx.mark_end();
                debugAssert(read == 4096);
            }
        },
    },
    Benchmark{
        .title = "ReadConsoleOutputCharacterW 4Ki",
        .exec = [](BenchmarkContext& ctx) {
            static constexpr COORD pos{ 0, 0 };
            const auto scratch = mem::get_scratch_arena(ctx.arena);
            const auto buf = scratch.arena.push_uninitialized<wchar_t>(4096);
            DWORD read;

            WriteConsoleW(ctx.output, ctx.utf16_128Ki.data(), static_cast<DWORD>(ctx.utf16_128Ki.size()), nullptr, nullptr);

            while (ctx.wants_more())
            {
                ctx.mark_beg();
                ReadConsoleOutputCharacterW(ctx.output, buf, 4096, pos, &read);
                ctx.mark_end();
                debugAssert(read == 4096);
            }
        },
    },
    Benchmark{
        .title = "ReadConsoleOutputW 4Ki",
        .exec = [](BenchmarkContext& ctx) {
            static constexpr COORD pos{ 0, 0 };
            static constexpr COORD size{ 64, 64 };
            static constexpr SMALL_RECT rect{ 0, 0, 63, 63 };
            const auto scratch = mem::get_scratch_arena(ctx.arena);
            const auto buf = scratch.arena.push_uninitialized<CHAR_INFO>(size.X * size.Y);

            WriteConsoleW(ctx.output, ctx.utf16_128Ki.data(), static_cast<DWORD>(ctx.utf16_128Ki.size()), nullptr, nullptr);

            while (ctx.wants_more())
            {
                auto read = rect;

                ctx.mark_beg();
                ReadConsoleOutputW(ctx.output, buf, size, pos, &read);
                ctx.mark_end();
                debugAssert(read.Right == 63 && read.Bottom == 63);
            }
        },
    },
#endif
#if ENABLE_TEST_INPUT
    Benchmark{
        .title = "WriteConsoleInputW 4Ki",
        .exec = [](BenchmarkContext& ctx) {
            DWORD written;

            FlushConsoleInputBuffer(ctx.input);

            while (ctx.wants_more())
            {
                ctx.mark_beg();
                WriteConsoleInputW(ctx.input, ctx.input_4Ki.data(), static_cast<DWORD>(ctx.input_4Ki.size()), &written);
                ctx.mark_end();
                debugAssert(written == ctx.input_4Ki.size());

                FlushConsoleInputBuffer(ctx.input);
            }
        },
    },
    Benchmark{
        .title = "ReadConsoleInputW 4Ki",
        .exec = [](BenchmarkContext& ctx) {
            const auto scratch = mem::get_scratch_arena(ctx.arena);
            const auto buf = scratch.arena.push_uninitialized<INPUT_RECORD>(ctx.input_4Ki.size());
            DWORD written, read;

            FlushConsoleInputBuffer(ctx.input);

            while (ctx.wants_more())
            {
                WriteConsoleInputW(ctx.input, ctx.input_4Ki.data(), static_cast<DWORD>(ctx.input_4Ki.size()), &written);
                debugAssert(written == ctx.input_4Ki.size());

                ctx.mark_beg();
                ReadConsoleInputW(ctx.input, buf, static_cast<DWORD>(ctx.input_4Ki.size()), &read);
                ctx.mark_end();
                debugAssert(read == ctx.input_4Ki.size());
            }
        },
    },
    Benchmark{
        .title = "ReadConsoleW 4Ki",
        .exec = [](BenchmarkContext& ctx) {
            const auto scratch = mem::get_scratch_arena(ctx.arena);
            const auto cap = static_cast<DWORD>(ctx.input_4Ki.size()) * 4;
            const auto buf = scratch.arena.push_uninitialized<wchar_t>(cap);
            DWORD written, read;

            FlushConsoleInputBuffer(ctx.input);

            while (ctx.wants_more())
            {
                WriteConsoleInputW(ctx.input, ctx.input_4Ki.data(), static_cast<DWORD>(ctx.input_4Ki.size()), &written);
                debugAssert(written == ctx.input_4Ki.size());

                ctx.mark_beg();
                ReadConsoleW(ctx.input, buf, cap, &read, nullptr);
                debugAssert(read == ctx.input_4Ki.size());
                ctx.mark_end();
            }
        },
    },
#endif
#if ENABLE_TEST_CLIPBOARD
    Benchmark{
        .title = "Clipboard copy 4Ki",
        .exec = [](BenchmarkContext& ctx) {
            WriteConsoleW(ctx.output, ctx.utf16_4Ki.data(), static_cast<DWORD>(ctx.utf8_4Ki.size()), nullptr, nullptr);

            while (ctx.wants_more())
            {
                SendMessageW(ctx.hwnd, WM_SYSCOMMAND, 0xFFF5 /* ID_CONSOLE_SELECTALL */, 0);

                ctx.mark_beg();
                SendMessageW(ctx.hwnd, WM_SYSCOMMAND, 0xFFF0 /* ID_CONSOLE_COPY */, 0);
                ctx.mark_end();
            }
        },
    },
    Benchmark{
        .title = "Clipboard paste 4Ki",
        .exec = [](BenchmarkContext& ctx) {
            set_clipboard(ctx.hwnd, ctx.utf16_4Ki);
            FlushConsoleInputBuffer(ctx.input);

            while (ctx.wants_more())
            {
                ctx.mark_beg();
                SendMessageW(ctx.hwnd, WM_SYSCOMMAND, 0xFFF1 /* ID_CONSOLE_PASTE */, 0);
                ctx.mark_end();

                FlushConsoleInputBuffer(ctx.input);
            }
        },
    },
#endif
};

static constexpr size_t s_benchmarks_count = _countof(s_benchmarks);
static constexpr size_t s_samples_min = 20;
static constexpr size_t s_samples_max = 1000;

// 128 characters and 124 columns.
static constexpr std::string_view s_payload_utf8{ "Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna alΑΒΓΔΕ" };
// 128 characters and 128 columns.
static constexpr std::wstring_view s_payload_utf16{ L"Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua.ΑΒΓΔΕ" };

static constexpr WORD s_payload_attr = FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_RED;
static constexpr CHAR_INFO s_payload_char{
    .Char = { .UnicodeChar = L'A' },
    .Attributes = s_payload_attr,
};
static constexpr INPUT_RECORD s_payload_record{
    .EventType = KEY_EVENT,
    .Event = {
        .KeyEvent = {
            .bKeyDown = TRUE,
            .wRepeatCount = 1,
            .wVirtualKeyCode = 'A',
            .wVirtualScanCode = 0,
            .uChar = 'A',
            .dwControlKeyState = 0,
        },
    },
};

static bool print_warning();
static AccumulatedResults* prepare_results(mem::Arena& arena, std::span<const wchar_t*> paths);
static std::span<Measurements> run_benchmarks_for_path(mem::Arena& arena, const wchar_t* path);
static void generate_html(mem::Arena& arena, const AccumulatedResults* results);

int wmain(int argc, const wchar_t* argv[])
try
{
    if (argc < 2)
    {
        mem::print_literal("Usage: %s [paths to conhost.exe]...");
        return 1;
    }

    check_spawn_conhost_dll(argc, argv);

    const auto cp = GetConsoleCP();
    const auto output_cp = GetConsoleOutputCP();
    const auto restore_cp = wil::scope_exit([&]() {
        SetConsoleCP(cp);
        SetConsoleOutputCP(output_cp);
    });
    SetConsoleCP(CP_UTF8);
    SetConsoleOutputCP(CP_UTF8);

    const auto scratch = mem::get_scratch_arena();

    const std::span paths{ &argv[1], static_cast<size_t>(argc) - 1 };
    const auto results = prepare_results(scratch.arena, paths);
    if (!results)
    {
        return 1;
    }

    if (!print_warning())
    {
        return 0;
    }

    for (size_t trace_idx = 0; trace_idx < results->trace_count; ++trace_idx)
    {
        const auto title = results->trace_names[trace_idx];
        print_format(scratch.arena, "\r\n# %.*s\r\n", title.size(), title.data());

        // I found that waiting between tests fixes weird bugs when launching very old conhost versions.
        if (trace_idx != 0)
        {
            Sleep(5000);
        }

        results->measurments[trace_idx] = run_benchmarks_for_path(scratch.arena, paths[trace_idx]);
    }

    generate_html(scratch.arena, results);
    return 0;
}
catch (const wil::ResultException& e)
{
    printf("Exception: %08x\n", e.GetErrorCode());
    return 1;
}
catch (...)
{
    printf("Unknown exception\n");
    return 1;
}

static bool print_warning()
{
    mem::print_literal(
        "This will overwrite any existing measurements.html in your current working directory.\r\n"
        "\r\n"
        "For best test results:\r\n"
        "* Make sure your system is fully idle and your CPU cool\r\n"
        "* Move your cursor to a corner of your screen and don't move it over the conhost window(s)\r\n"
        "* Exit or stop any background applications, including Windows Defender (if possible)\r\n"
        "\r\n"
        "Continue? [Yn] ");

    for (;;)
    {
        INPUT_RECORD rec;
        DWORD read;
        if (!ReadConsoleInputW(GetStdHandle(STD_INPUT_HANDLE), &rec, 1, &read) || read == 0)
        {
            return false;
        }

        if (rec.EventType == KEY_EVENT && rec.Event.KeyEvent.bKeyDown)
        {
            // Transforms the character to uppercase if it's lowercase.
            const auto ch = rec.Event.KeyEvent.uChar.UnicodeChar & 0xDF;
            if (ch == L'N')
            {
                return false;
            }
            if (ch == L'\r' || ch == L'Y')
            {
                break;
            }
        }
    }

    mem::print_literal("\r\n");
    return true;
}

static AccumulatedResults* prepare_results(mem::Arena& arena, std::span<const wchar_t*> paths)
{
    for (const auto path : paths)
    {
        const auto attr = GetFileAttributesW(path);
        if (attr == INVALID_FILE_ATTRIBUTES || (attr & FILE_ATTRIBUTE_DIRECTORY) != 0)
        {
            print_format(arena, "Invalid path: %S\r\n", path);
            return nullptr;
        }
    }

    const auto trace_count = paths.size();
    const auto results = arena.push_zeroed<AccumulatedResults>();

    results->trace_count = trace_count;
    results->trace_names = arena.push_zeroed<std::string_view>(trace_count);
    results->measurments = arena.push_zeroed<MeasurementsPerBenchmark>(trace_count);

    for (size_t trace_idx = 0; trace_idx < trace_count; ++trace_idx)
    {
        const auto path = paths[trace_idx];
        auto trace_name = get_file_version(arena, path);

        if (trace_name.empty())
        {
            const auto end = path + wcsnlen(path, SIZE_MAX);
            auto filename = end;
            for (; filename > path && filename[-1] != L'\\' && filename[-1] != L'/'; --filename)
            {
            }
            trace_name = u16u8(arena, filename, end - filename);
        }

        results->trace_names[trace_idx] = trace_name;
    }

    return results;
}

static void prepare_conhost(BenchmarkContext& ctx, HWND parent_hwnd)
{
    const auto scratch = mem::get_scratch_arena(ctx.arena);

    SetForegroundWindow(parent_hwnd);

    // Ensure conhost is in a consistent state with identical fonts and window sizes.
    SetConsoleCP(CP_UTF8);
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleMode(ctx.output, ENABLE_PROCESSED_OUTPUT | ENABLE_WRAP_AT_EOL_OUTPUT | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    // The ReadConsoleW test relies on ENABLE_LINE_INPUT not being set.
    SetConsoleMode(ctx.input, ENABLE_PROCESSED_INPUT | ENABLE_ECHO_INPUT);

    {
        CONSOLE_FONT_INFOEX info{
            .cbSize = sizeof(info),
            .dwFontSize = { 0, 16 },
            .FontFamily = 54,
            .FontWeight = 400,
            .FaceName = L"Consolas",
        };
        SetCurrentConsoleFontEx(ctx.output, FALSE, &info);
    }
    {
        SMALL_RECT info{
            .Left = 0,
            .Top = 0,
            .Right = s_viewport_size.X - 1,
            .Bottom = s_viewport_size.Y - 1,
        };
        SetConsoleScreenBufferSize(ctx.output, s_buffer_size);
        SetConsoleWindowInfo(ctx.output, TRUE, &info);
    }

    // Ensure conhost's backing TextBuffer is fully committed and initialized. There's currently no way
    // to un-commit it and so not committing it now would be unfair for the first test that runs.
    const auto buf = scratch.arena.push_uninitialized<char>(s_buffer_size.Y);
    memset(buf, '\n', s_buffer_size.Y);
    WriteFile(ctx.output, buf, s_buffer_size.Y, nullptr, nullptr);
}

static std::span<Measurements> run_benchmarks_for_path(mem::Arena& arena, const wchar_t* path)
{
    const auto scratch = mem::get_scratch_arena(arena);
    const auto parent_connection = get_active_connection();
    const auto parent_hwnd = GetConsoleWindow();
    const auto freq = query_perf_freq();

    auto handle = spawn_conhost(scratch.arena, path);
    set_active_connection(handle.connection.get());

    const auto print_with_parent_connection = [&](auto&&... args) {
        set_active_connection(parent_connection);
        mem::print_format(scratch.arena, args...);
        set_active_connection(handle.connection.get());
    };

    BenchmarkContext ctx{
        .hwnd = GetConsoleWindow(),
        .input = GetStdHandle(STD_INPUT_HANDLE),
        .output = GetStdHandle(STD_OUTPUT_HANDLE),

        .arena = scratch.arena,
        .utf8_4Ki = mem::repeat(scratch.arena, s_payload_utf8, 4 * 1024 / s_payload_utf8.size()),
        .utf8_128Ki = mem::repeat(scratch.arena, s_payload_utf8, 128 * 1024 / s_payload_utf8.size()),
        .utf16_4Ki = mem::repeat(scratch.arena, s_payload_utf16, 4 * 1024 / s_payload_utf16.size()),
        .utf16_128Ki = mem::repeat(scratch.arena, s_payload_utf16, 128 * 1024 / s_payload_utf16.size()),
        .attr_4Ki = mem::repeat(scratch.arena, s_payload_attr, 4 * 1024),
        .char_4Ki = mem::repeat(scratch.arena, s_payload_char, 4 * 1024),
        .input_4Ki = mem::repeat(scratch.arena, s_payload_record, 4 * 1024),

        .m_measurements = scratch.arena.push_uninitialized_span<int32_t>(4 * 1024 * 1024),
    };

    prepare_conhost(ctx, parent_hwnd);
    Sleep(1000);

    const auto results = arena.push_uninitialized_span<Measurements>(s_benchmarks_count);

    for (size_t bench_idx = 0; bench_idx < s_benchmarks_count; ++bench_idx)
    {
        const auto& bench = s_benchmarks[bench_idx];

        print_with_parent_connection("- %s", bench.title);

        // Warmup for 0.1s max.
        WriteConsoleW(ctx.output, L"\033c", 2, nullptr, nullptr);
        ctx.m_measurements_off = 0;
        ctx.m_time_limit = query_perf_counter() + freq / 10;
        bench.exec(ctx);

        // Actual run for 3s max.
        WriteConsoleW(ctx.output, L"\033c", 2, nullptr, nullptr);
        ctx.m_measurements_off = 0;
        ctx.m_time_limit = query_perf_counter() + freq * 3;
        bench.exec(ctx);

        const auto measurements = arena.push_uninitialized_span<int32_t>(std::min(ctx.m_measurements_off, s_samples_max));
        if (ctx.m_measurements_off <= s_samples_max)
        {
            mem::copy(measurements.data(), ctx.m_measurements.data(), ctx.m_measurements_off);
        }
        else
        {
            const auto total = ctx.m_measurements_off;
            for (size_t i = 0; i < s_samples_max; ++i)
            {
                measurements[i] = ctx.m_measurements[i * total / s_samples_max];
            }
        }
        results[bench_idx] = measurements;

        print_with_parent_connection(", done\r\n");
    }

    set_active_connection(parent_connection);
    return results;
}

static void generate_html(mem::Arena& arena, const AccumulatedResults* results)
{
    const auto scratch = mem::get_scratch_arena(arena);

    const wil::unique_hfile file{ THROW_LAST_ERROR_IF_NULL(CreateFileW(L"measurements.html", GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_DELETE, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr)) };
    const auto sec_per_tick = 1.0f / query_perf_freq();
    BufferedWriter writer{ file.get(), scratch.arena.push_uninitialized_span<char>(1024 * 1024) };

    writer.write(R"(<!DOCTYPE html>
<!DOCTYPE html>
<html lang="en-US">

<head>
    <meta charset="utf-8" />
    <meta name="viewport" content="width=device-width,initial-scale=1" />
    <style>
        html {
            overflow-x: hidden;
        }

        html, body {
            margin: 0;
            padding: 0;
        }

        body {
            display: flex;
            flex-direction: row;
            flex-wrap: wrap;
        }

        .view {
            width: 1024px;
            height: 600px;
        }
    </style>
</head>

<body>
    <script src="https://cdn.plot.ly/plotly-2.32.0.min.js" charset="utf-8"></script>
    <script>
)");

    {
        writer.write("        const results = [");

        for (size_t bench_idx = 0; bench_idx < s_benchmarks_count; ++bench_idx)
        {
            const auto& bench = s_benchmarks[bench_idx];

            writer.write("{title:'");
            writer.write(bench.title);
            writer.write("',results:[");

            for (size_t trace_idx = 0; trace_idx < results->trace_count; ++trace_idx)
            {
                writer.write("{basename:'");
                writer.write(results->trace_names[trace_idx]);
                writer.write("',measurements:[");

                const auto measurements = results->measurments[trace_idx][bench_idx];
                if (!measurements.empty())
                {
                    std::sort(measurements.begin(), measurements.end());

                    // Console calls have a high tail latency. Whatever the reason is (it's probably scheduling latency)
                    // it's not particularly interesting at the moment when the median latency is intolerable high anyway.
                    const auto p25 = measurements[(measurements.size() * 250 + 5) / 1000];
                    const auto p75 = measurements[(measurements.size() * 750 + 5) / 1000];
                    const auto iqr3 = (p75 - p25) * 3;
                    const auto outlier_max = p75 + iqr3;

                    const auto beg = measurements.data();
                    auto end = beg + measurements.size();

                    for (; end[-1] > outlier_max; --end)
                    {
                    }

                    for (auto it = beg; it != end; ++it)
                    {
                        char buffer[32];
                        const auto res = std::to_chars(&buffer[0], &buffer[64], *it * sec_per_tick, std::chars_format::scientific, 3);
                        writer.write({ &buffer[0], res.ptr });
                        writer.write(",");
                    }
                }

                writer.write("]},");
            }

            writer.write("]},");
        }

        writer.write("];\n");
    }

    writer.write(R"(
        for (const r of results) {
            const div = document.createElement('div');
            div.className = 'view';
            document.body.insertAdjacentElement('beforeend', div)

            Plotly.newPlot(div, r.results.map(tcr => ({
                type: 'violin',
                name: tcr.basename,
                y: tcr.measurements,
                meanline: { visible: true },
                points: false,
                spanmode : 'hard',
            })), {
                showlegend: false,
                title: r.title,
                yaxis: {
                    minexponent: 0,
                    showgrid: true,
                    showline: true,
                    ticksuffix: 's',
                },
            }, {
                responsive: true,
            });
        }
    </script>
</body>

</html>
)");
}

bool BenchmarkContext::wants_more() const
{
    return m_measurements_off < s_samples_min || (m_measurements_off < m_measurements.size() && m_time < m_time_limit);
}

void BenchmarkContext::mark_beg()
{
    m_time = query_perf_counter();
}

void BenchmarkContext::mark_end()
{
    const auto end = query_perf_counter();
    m_measurements[m_measurements_off++] = static_cast<int32_t>(end - m_time);
    m_time = end;
}

size_t BenchmarkContext::rand()
{
    // These constants are the same as used by the PCG family of random number generators.
    // The 32-Bit version is described in https://doi.org/10.1090/S0025-5718-99-00996-5, Table 5.
    // The 64-Bit version is the multiplier as used by Donald Knuth for MMIX and found by C. E. Haynes.
#ifdef _WIN64
    m_rng_state = m_rng_state * UINT64_C(6364136223846793005) + UINT64_C(1442695040888963407);
#else
    m_rng_state = m_rng_state * UINT32_C(747796405) + UINT32_C(2891336453);
#endif
    return m_rng_state;
}
