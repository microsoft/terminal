// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN

#include <Windows.h>
#include <Shlwapi.h>
#include <shellapi.h>
#include <icu.h>

#include <cstdint>

#include "crt.cpp"

// This warning is broken on if/else chains with init statements.
#pragma warning(disable : 4456) // declaration of '...' hides previous local declaration

namespace pcg_engines
{
    /*
     * PCG Random Number Generation for C++
     *
     * Copyright 2014-2017 Melissa O'Neill <oneill@pcg-random.org>,
     *                     and the PCG Project contributors.
     *
     * SPDX-License-Identifier: (Apache-2.0 OR MIT)
     *
     * Licensed under the Apache License, Version 2.0 (provided in
     * LICENSE-APACHE.txt and at http://www.apache.org/licenses/LICENSE-2.0)
     * or under the MIT license (provided in LICENSE-MIT.txt and at
     * http://opensource.org/licenses/MIT), at your option. This file may not
     * be copied, modified, or distributed except according to those terms.
     *
     * Distributed on an "AS IS" BASIS, WITHOUT WARRANTY OF ANY KIND, either
     * express or implied.  See your chosen license for details.
     *
     * For additional information about the PCG random number generation scheme,
     * visit http://www.pcg-random.org/.
     */

    class oneseq_dxsm_64_32
    {
        using xtype = uint32_t;
        using itype = uint64_t;

        itype state_;

        static constexpr uint64_t multiplier() noexcept
        {
            return 6364136223846793005ULL;
        }

        static constexpr uint64_t increment() noexcept
        {
            return 1442695040888963407ULL;
        }

        static constexpr itype bump(itype state) noexcept
        {
            return state * multiplier() + increment();
        }

        constexpr itype base_generate0() noexcept
        {
            itype old_state = state_;
            state_ = bump(state_);
            return old_state;
        }

    public:
        explicit constexpr oneseq_dxsm_64_32(itype state = 0xcafef00dd15ea5e5ULL) noexcept :
            state_(bump(state + increment()))
        {
        }

        constexpr xtype operator()() noexcept
        {
            constexpr auto xtypebits = uint8_t(sizeof(xtype) * 8);
            constexpr auto itypebits = uint8_t(sizeof(itype) * 8);
            static_assert(xtypebits <= itypebits / 2, "Output type must be half the size of the state type.");

            auto internal = base_generate0();
            auto hi = xtype(internal >> (itypebits - xtypebits));
            auto lo = xtype(internal);

            lo |= 1;
            hi ^= hi >> (xtypebits / 2);
            hi *= xtype(multiplier());
            hi ^= hi >> (3 * (xtypebits / 4));
            hi *= lo;
            return hi;
        }

        constexpr xtype operator()(xtype upper_bound) noexcept
        {
            uint32_t threshold = (UINT64_MAX + uint32_t(1) - upper_bound) % upper_bound;
            for (;;)
            {
                auto r = operator()();
                if (r >= threshold)
                    return r % upper_bound;
            }
        }
    };
} // namespace pcg_engines

template<typename T>
constexpr T min(T a, T b)
{
    return a < b ? a : b;
}

template<typename T>
constexpr T max(T a, T b)
{
    return a > b ? a : b;
}

template<typename T>
constexpr T clamp(T val, T min, T max)
{
    return val < min ? min : (val > max ? max : val);
}

static uint32_t parse_number(const wchar_t* str, const wchar_t** end) noexcept
{
    static constexpr uint32_t max = 0x0fffffff;
    uint32_t accumulator = 0;

    for (;; ++str)
    {
        if (*str == '\0' || *str < '0' || *str > '9')
        {
            break;
        }

        accumulator = accumulator * 10 + *str - '0';
        if (accumulator >= max)
        {
            accumulator = max;
            break;
        }
    }

    if (end)
    {
        *end = str;
    }

    return accumulator;
}

static uint32_t parse_number_with_suffix(const wchar_t* str) noexcept
{
    const wchar_t* str_end = nullptr;
    auto value = parse_number(str, &str_end);

    if (str_end[0])
    {
        uint32_t mul = 1000;

        if (str_end[1])
        {
            mul = 1024;

            if (str_end[1] != L'i')
            {
                value = 0;
            }
        }

        switch (*str_end)
        {
        case L'g':
        case L'G':
            value *= mul;
            [[fallthrough]];
        case L'm':
        case L'M':
            value *= mul;
            [[fallthrough]];
        case L'k':
        case L'K':
            value *= mul;
            break;
        default:
            value = 0;
            break;
        }
    }

    return value;
}

static bool has_suffix(const wchar_t* str, const wchar_t* suffix)
{
    return wcscmp(str, suffix) == 0;
}

static const wchar_t* split_prefix(const wchar_t* str, const wchar_t* prefix) noexcept
{
    for (; *prefix; ++prefix, ++str)
    {
        if (*str != *prefix)
        {
            return nullptr;
        }
    }
    return str;
}

static char* buffer_append_long(char* dst, const void* src, size_t size)
{
    memcpy(dst, src, size);
    return dst + size;
}

static char* buffer_append(char* dst, const char* src, size_t size)
{
    for (size_t i = 0; i < size; ++i, ++src, ++dst)
    {
        *dst = *src;
    }
    return dst;
}

static char* buffer_append_string(char* dst, const char* src)
{
    return buffer_append(dst, src, strlen(src));
}

char* buffer_append_number(char* dst, uint8_t val)
{
    if (val >= 10)
    {
        if (val >= 100)
        {
            const uint8_t d = val / 100;
            *dst++ = '0' + d;
            val -= d * 100;
        }

        const uint8_t d = val / 10;
        *dst++ = '0' + d;
        val -= d * 10;
    }

    *dst++ = '0' + val;
    return dst;
}

struct FormatResult
{
    LONGLONG integral;
    LONGLONG fractional;
    const char* suffix;
};

#define FORMAT_RESULT_FMT "%lld.%03lld%s"
#define FORMAT_RESULT_ARGS(r) r.integral, r.fractional, r.suffix

static FormatResult format_size(LONGLONG value)
{
    FormatResult result;
    if (value >= 1'000'000'000)
    {
        result.integral = value / 1'000'000'000;
        result.fractional = ((value + 500'000) / 1'000'000) % 1000;
        result.suffix = "G";
    }
    else if (value >= 1'000'000)
    {
        result.integral = value / 1'000'000;
        result.fractional = ((value + 500) / 1'000) % 1000;
        result.suffix = "M";
    }
    else if (value >= 1'000)
    {
        result.integral = value / 1'000;
        result.fractional = value % 1'000;
        result.suffix = "k";
    }
    else
    {
        result.integral = value;
        result.fractional = 0;
        result.suffix = "";
    }
    return result;
}

static FormatResult format_duration(LONGLONG microseconds)
{
    FormatResult result;
    if (microseconds >= 1'000'000)
    {
        result.integral = microseconds / 1'000'000;
        result.fractional = ((microseconds + 500) / 1'000) % 1000;
        result.suffix = "";
    }
    else
    {
        result.integral = microseconds / 1'000;
        result.fractional = microseconds % 1'000;
        result.suffix = "m";
    }
    return result;
}

static int format(char* buffer, int size, const char* format, ...) noexcept
{
    va_list vl;
    va_start(vl, format);
    const auto length = wvnsprintfA(buffer, size, format, vl);
    va_end(vl);
    return length;
}

enum class VtMode
{
    Off,
    On,
    Italic,
    Color
};

static HANDLE g_stdout;
static HANDLE g_stderr;
static UINT g_console_cp_old;
static DWORD g_console_mode_old;
static size_t g_large_page_minimum;

[[noreturn]] static void clean_exit(UINT code)
{
    if (g_console_cp_old)
    {
        SetConsoleCP(g_console_cp_old);
    }
    if (g_console_mode_old)
    {
        SetConsoleMode(g_stdout, g_console_mode_old);
    }
    ExitProcess(code);
}

[[noreturn]] static void eprintf(const char* format, ...) noexcept
{
    char buffer[1024];

    va_list vl;
    va_start(vl, format);
    const auto length = wvnsprintfA(buffer, sizeof(buffer), format, vl);
    va_end(vl);

    if (length > 0)
    {
        WriteFile(g_stderr, buffer, length, nullptr, nullptr);
    }

    clean_exit(1);
}

[[noreturn]] static void print_last_error(const char* what) noexcept
{
    eprintf("\r\nfailed to %s with 0x%08lx\r\n", what, GetLastError());
}

static void acquire_lock_memory_privilege() noexcept
{
    HANDLE token;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES, &token))
    {
        return;
    }

    TOKEN_PRIVILEGES privileges{};
    privileges.PrivilegeCount = 1;
    privileges.Privileges[0].Luid = { 4, 0 }; // SE_LOCK_MEMORY_PRIVILEGE is a well known LUID and always {4, 0}
    privileges.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

    // AdjustTokenPrivileges can return true and still set the last error to ERROR_NOT_ALL_ASSIGNED. This API is nuts...
    const bool success = AdjustTokenPrivileges(token, FALSE, &privileges, 0, nullptr, nullptr);
    if (success && GetLastError() == S_OK)
    {
        g_large_page_minimum = GetLargePageMinimum();
    }

    CloseHandle(token);
}

static char* allocate(size_t size)
{
    if (g_large_page_minimum)
    {
        const auto large_size = (size + g_large_page_minimum - 1) & ~(g_large_page_minimum - 1);
        if (const auto address = static_cast<char*>(VirtualAlloc(nullptr, large_size, MEM_COMMIT | MEM_RESERVE | MEM_LARGE_PAGES, PAGE_READWRITE)))
        {
            return address;
        }
    }

    if (const auto address = static_cast<char*>(VirtualAlloc(nullptr, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE)))
    {
        return address;
    }

    print_last_error("allocate memory");
}

static BOOL WINAPI consoleCtrlHandler(DWORD)
{
    CancelIoEx(g_stdout, nullptr);
    return TRUE;
}

int __stdcall main() noexcept
{
    g_stdout = GetStdHandle(STD_OUTPUT_HANDLE);
    g_stderr = GetStdHandle(STD_OUTPUT_HANDLE);
    g_console_cp_old = GetConsoleOutputCP();

    SetConsoleCtrlHandler(consoleCtrlHandler, TRUE);
    SetConsoleOutputCP(CP_UTF8);

    const wchar_t* path = nullptr;
    uint32_t chunk_size = 128 * 1024;
    uint32_t repeat = 1;
    VtMode vt = VtMode::Off;
    uint64_t seed = 0;
    bool has_seed = false;

    {
        int argc;
        const auto argv = CommandLineToArgvW(GetCommandLineW(), &argc);

        for (int i = 1; i < argc; ++i)
        {
            if (const auto suffix = split_prefix(argv[i], L"-c"))
            {
                // 1GiB is the maximum buffer size WriteFile seems to accept.
                chunk_size = min<uint32_t>(parse_number_with_suffix(suffix), 1024 * 1024 * 1024);
            }
            else if (const auto suffix = split_prefix(argv[i], L"-r"))
            {
                repeat = parse_number_with_suffix(suffix);
            }
            else if (const auto suffix = split_prefix(argv[i], L"-v"))
            {
                vt = VtMode::On;
                if (has_suffix(suffix, L"i"))
                {
                    vt = VtMode::Italic;
                }
                else if (has_suffix(suffix, L"c"))
                {
                    vt = VtMode::Color;
                }
                else if (*suffix)
                {
                    break;
                }
            }
            else if (has_suffix(argv[i], L"-s"))
            {
                seed = parse_number_with_suffix(suffix);
                has_seed = true;
            }
            else
            {
                if (argc - i == 1)
                {
                    path = argv[i];
                }
                break;
            }
        }
    }

    if (!path || !chunk_size || !repeat)
    {
        eprintf(
            "bc [options] <filename>\r\n"
            "  -v        enable VT\r\n"
            "  -vi       print as italic\r\n"
            "  -vc       print colorized\r\n"
            "  -c{d}{u}  chunk size, defaults to 128Ki\r\n"
            "  -r{d}{u}  repeats, defaults to 1\r\n"
            "  -s{d}     RNG seed\r\n"
            "{d} are base-10 digits\r\n"
            "{u} are suffix units k, Ki, M, Mi, G, Gi\r\n");
    }

    if (!has_seed && vt == VtMode::Color)
    {
        const auto cryptbase = LoadLibraryExW(L"cryptbase.dll", nullptr, 0);
        if (!cryptbase)
        {
            print_last_error("get handle to cryptbase.dll");
        }

        const auto RtlGenRandom = reinterpret_cast<BOOLEAN(APIENTRY*)(PVOID, ULONG)>(GetProcAddress(cryptbase, "SystemFunction036"));
        if (!RtlGenRandom)
        {
            print_last_error("get handle to RtlGenRandom");
        }

        RtlGenRandom(&seed, sizeof(seed));
    }

    pcg_engines::oneseq_dxsm_64_32 rng{ seed };

    const auto stdout = GetStdHandle(STD_OUTPUT_HANDLE);
    const auto file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE)
    {
        print_last_error("open file");
    }

    size_t file_size = 0;
    {
#ifdef _WIN64
        LARGE_INTEGER i;
        if (!GetFileSizeEx(file, &i))
        {
            print_last_error("open file");
        }

        file_size = static_cast<size_t>(i.QuadPart);

#else
        file_size = GetFileSize(file, nullptr);
        if (file_size == INVALID_FILE_SIZE)
        {
            print_last_error("open file");
        }
#endif
    }

    acquire_lock_memory_privilege();

    const auto file_data = allocate(file_size);
    auto stdout_size = file_size;
    auto stdout_data = file_data;

    {
        auto read_data = file_data;
        DWORD read = 0;

        for (auto remaining = file_size; remaining > 0; remaining -= read, read_data += read)
        {
            read = static_cast<DWORD>(min<size_t>(0xffffffff, remaining));
            if (!ReadFile(file, read_data, read, &read, nullptr))
            {
                print_last_error("read");
            }
        }
    }

    switch (vt)
    {
    case VtMode::Italic:
    {
        stdout_data = allocate(file_size + 16);
        auto p = stdout_data;
        p = buffer_append_string(p, "\x1b[3m");
        p = buffer_append_long(p, file_data, file_size);
        p = buffer_append_string(p, "\x1b[0m");
        stdout_size = static_cast<DWORD>(p - stdout_data);
        break;
    }
    case VtMode::Color:
    {
        if (const auto icu = LoadLibraryExW(L"icuuc.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32))
        {
            stdout_data = allocate(file_size * 20 + 8);
            auto p = stdout_data;

            const auto p_utext_openUTF8 = reinterpret_cast<decltype(&utext_openUTF8)>(GetProcAddress(icu, "utext_openUTF8"));
            const auto p_ubrk_open = reinterpret_cast<decltype(&ubrk_open)>(GetProcAddress(icu, "ubrk_open"));
            const auto p_ubrk_setUText = reinterpret_cast<decltype(&ubrk_setUText)>(GetProcAddress(icu, "ubrk_setUText"));
            const auto p_ubrk_next = reinterpret_cast<decltype(&ubrk_next)>(GetProcAddress(icu, "ubrk_next"));

            auto error = U_ZERO_ERROR;
            UText text = UTEXT_INITIALIZER;
            p_utext_openUTF8(&text, file_data, file_size, &error);

            const auto it = p_ubrk_open(UBRK_CHARACTER, "", nullptr, 0, &error);
            p_ubrk_setUText(it, &text, &error);

            for (int32_t ubrk0 = 0, ubrk1; (ubrk1 = p_ubrk_next(it)) != UBRK_DONE; ubrk0 = ubrk1)
            {
                p = buffer_append_string(p, "\x1b[38;2");
                for (int i = 0; i < 3; i++)
                {
                    *p++ = ';';
                    p = buffer_append_number(p, static_cast<uint8_t>(rng()));
                }
                p = buffer_append_string(p, "m");
                p = buffer_append(p, file_data + ubrk0, ubrk1 - ubrk0);
            }

            p = buffer_append_string(p, "\x1b[39;49m");
            stdout_size = static_cast<DWORD>(p - stdout_data);
        }
        break;
    }
    default:
        break;
    }

    {
        DWORD mode = 0;
        if (!GetConsoleMode(g_stdout, &mode))
        {
            print_last_error("get console mode");
        }

        g_console_mode_old = mode;

        mode |= ENABLE_PROCESSED_OUTPUT | ENABLE_WRAP_AT_EOL_OUTPUT;
        mode &= ~(ENABLE_VIRTUAL_TERMINAL_PROCESSING | DISABLE_NEWLINE_AUTO_RETURN);
        if (vt != VtMode::Off)
        {
            mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
        }

        if (!SetConsoleMode(g_stdout, mode))
        {
            print_last_error("set console mode");
        }
    }

    LARGE_INTEGER frequency, beg, end;
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&beg);

    for (size_t iteration = 0; iteration < repeat; ++iteration)
    {
        auto write_data = stdout_data;
        DWORD written = 0;

        for (auto remaining = stdout_size; remaining != 0; remaining -= written, write_data += written)
        {
            written = static_cast<DWORD>(min<size_t>(remaining, chunk_size));
            if (!WriteFile(stdout, write_data, written, &written, nullptr))
            {
                print_last_error("write");
            }
        }
    }

    QueryPerformanceCounter(&end);

    const auto elapsed_ticks = end.QuadPart - beg.QuadPart;
    const auto elapsed_us = (elapsed_ticks * 1'000'000) / frequency.QuadPart;
    LONGLONG total_size = static_cast<LONGLONG>(stdout_size) * repeat;
    const auto bytes_per_second = (total_size * frequency.QuadPart) / elapsed_ticks;

    const auto written = format_size(total_size);
    const auto duration = format_duration(elapsed_us);
    const auto throughput = format_size(bytes_per_second);

    char status[128];
    const auto status_length = format(
        &status[0],
        1024,
        FORMAT_RESULT_FMT "B, " FORMAT_RESULT_FMT "s, " FORMAT_RESULT_FMT "B/s",
        FORMAT_RESULT_ARGS(written),
        FORMAT_RESULT_ARGS(duration),
        FORMAT_RESULT_ARGS(throughput));

    if (status_length <= 0)
    {
        clean_exit(1);
    }

    char buffer[256];
    char* buffer_end = &buffer[0];

    buffer_end = buffer_append_string(buffer_end, "\r\n");
    for (int i = 0; i < status_length; ++i)
    {
        *buffer_end++ = '-';
    }
    buffer_end = buffer_append_string(buffer_end, "\r\n");
    buffer_end = buffer_append_long(buffer_end, &status[0], static_cast<size_t>(status_length));
    buffer_end = buffer_append_string(buffer_end, "\r\n");

    WriteFile(g_stderr, &buffer[0], static_cast<DWORD>(buffer_end - &buffer[0]), nullptr, nullptr);
    clean_exit(0);
}
