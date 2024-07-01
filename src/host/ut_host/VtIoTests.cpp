// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include <til/rand.h>

#include "CommonState.hpp"
#include "directio.h"
#include "../VtIo.hpp"
#include "../../interactivity/inc/ServiceLocator.hpp"
#include "../../renderer/base/Renderer.hpp"
#include "../../types/inc/Viewport.hpp"

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;

using namespace Microsoft::Console::Interactivity;
using namespace Microsoft::Console;
using namespace Microsoft::Console::VirtualTerminal;
using namespace Microsoft::Console::Render;
using namespace Microsoft::Console::Types;

constexpr CHAR_INFO red(wchar_t ch) noexcept
{
    return { ch, FOREGROUND_RED };
}

constexpr CHAR_INFO blu(wchar_t ch) noexcept
{
    return { ch, FOREGROUND_BLUE };
}

#pragma warning(disable : 4505)

static std::pair<wil::unique_hfile, wil::unique_hfile> createOverlappedPipe(DWORD bufferSize)
{
    const auto rnd = til::gen_random<uint64_t>();
    const auto name = fmt::format(FMT_COMPILE(LR"(\\.\pipe\{:016x})"), rnd);
    wil::unique_hfile rx{ CreateNamedPipeW(name.c_str(), PIPE_ACCESS_INBOUND | FILE_FLAG_OVERLAPPED, PIPE_TYPE_BYTE | PIPE_WAIT, 1, bufferSize, bufferSize, 0, nullptr) };
    THROW_LAST_ERROR_IF(!rx);
    wil::unique_hfile tx{ CreateFileW(name.c_str(), GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED, nullptr) };
    THROW_LAST_ERROR_IF(!tx);
    return { std::move(tx), std::move(rx) };
}

#define cup(y, x) "\x1b[" #y ";" #x "H"
#define decawm(h) "\x1b[?7" #h
#define lnm(h) "\x1b[20" #h

// The escape sequences that red() / blu() result in.
#define sgr_red(s) "\x1b[27;31;40m" s
#define sgr_blu(s) "\x1b[27;34;40m" s

#define sgr_rst() "\x1b[m"

static constexpr std::wstring_view s_initialContentVT{
    // clang-format off
    L""
    sgr_red("AB") sgr_blu("ab") sgr_red("CD") sgr_blu("cd") "\r\n"
    sgr_red("EF") sgr_blu("ef") sgr_red("GH") sgr_blu("gh") "\r\n"
    sgr_blu("ij") sgr_red("IJ") sgr_blu("kl") sgr_red("KL") "\r\n"
    sgr_blu("mn") sgr_red("MN") sgr_blu("op") sgr_red("OP")
    // clang-format on
};

class ::Microsoft::Console::VirtualTerminal::VtIoTests
{
    BEGIN_TEST_CLASS(VtIoTests)
        TEST_CLASS_PROPERTY(L"IsolationLevel", L"Class")
        //TEST_CLASS_PROPERTY(L"TestTimeout", L"0:1:0") // 1min
    END_TEST_CLASS()

    CommonState commonState;
    ApiRoutines routines;
    SCREEN_INFORMATION* screenInfo = nullptr;
    wil::unique_hfile rx;
    char rxBuf[4096];

    std::string_view readOutput() noexcept
    {
        DWORD read = 0;
        ReadFile(rx.get(), &rxBuf[0], sizeof(rxBuf), &read, nullptr);
        return { &rxBuf[0], read };
    }

    void setupInitialContents() const
    {
        auto& sm = screenInfo->GetStateMachine();
        sm.ProcessString(L"\033c");
        sm.ProcessString(s_initialContentVT);
        sm.ProcessString(L"\x1b[H");
    }

    void resetContents() const
    {
        auto& sm = screenInfo->GetStateMachine();
        sm.ProcessString(L"\033c");
    }

    TEST_CLASS_SETUP(ClassSetup)
    {
        wil::unique_hfile tx;
        //std::tie(tx, rx) = createOverlappedPipe(16 * 1024);
        THROW_IF_WIN32_BOOL_FALSE(CreatePipe(rx.addressof(), tx.addressof(), nullptr, 16 * 1024));

        DWORD mode = PIPE_READMODE_BYTE | PIPE_NOWAIT;
        THROW_IF_WIN32_BOOL_FALSE(SetNamedPipeHandleState(rx.get(), &mode, nullptr, nullptr));

        commonState.PrepareGlobalInputBuffer();
        commonState.PrepareGlobalScreenBuffer(8, 4, 8, 4);

        auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
        THROW_IF_FAILED(gci.GetVtIoNoCheck()->_Initialize(nullptr, tx.release(), nullptr));

        screenInfo = &gci.GetActiveOutputBuffer();
        return true;
    }

    TEST_METHOD(SetConsoleCursorPosition)
    {
        THROW_IF_FAILED(routines.SetConsoleCursorPositionImpl(*screenInfo, { 2, 3 }));
        THROW_IF_FAILED(routines.SetConsoleCursorPositionImpl(*screenInfo, { 0, 0 }));
        THROW_IF_FAILED(routines.SetConsoleCursorPositionImpl(*screenInfo, { 7, 3 }));
        THROW_IF_FAILED(routines.SetConsoleCursorPositionImpl(*screenInfo, { 3, 2 }));

        const auto expected = cup(4, 3) cup(1, 1) cup(4, 8) cup(3, 4);
        const auto actual = readOutput();
        VERIFY_ARE_EQUAL(expected, actual);
    }

    TEST_METHOD(SetConsoleOutputMode)
    {
        const auto initialMode = screenInfo->OutputMode;
        const auto cleanup = wil::scope_exit([=]() {
            screenInfo->OutputMode = initialMode;
        });

        screenInfo->OutputMode = 0;

        THROW_IF_FAILED(routines.SetConsoleOutputModeImpl(*screenInfo, ENABLE_PROCESSED_OUTPUT | ENABLE_WRAP_AT_EOL_OUTPUT | ENABLE_VIRTUAL_TERMINAL_PROCESSING | DISABLE_NEWLINE_AUTO_RETURN | ENABLE_LVB_GRID_WORLDWIDE)); // DECAWM ✔️ LNM ✔️
        THROW_IF_FAILED(routines.SetConsoleOutputModeImpl(*screenInfo, ENABLE_PROCESSED_OUTPUT | ENABLE_WRAP_AT_EOL_OUTPUT | ENABLE_VIRTUAL_TERMINAL_PROCESSING)); // DECAWM ✔️ LNM ✖️
        THROW_IF_FAILED(routines.SetConsoleOutputModeImpl(*screenInfo, ENABLE_PROCESSED_OUTPUT | DISABLE_NEWLINE_AUTO_RETURN | ENABLE_LVB_GRID_WORLDWIDE)); // DECAWM ✖️ LNM ✔️
        THROW_IF_FAILED(routines.SetConsoleOutputModeImpl(*screenInfo, 0)); // DECAWM ✖️ LNM ✖️
        THROW_IF_FAILED(routines.SetConsoleOutputModeImpl(*screenInfo, ENABLE_PROCESSED_OUTPUT | ENABLE_WRAP_AT_EOL_OUTPUT | DISABLE_NEWLINE_AUTO_RETURN | ENABLE_LVB_GRID_WORLDWIDE)); // DECAWM ✔️ LNM ✖️
        THROW_IF_FAILED(routines.SetConsoleOutputModeImpl(*screenInfo, ENABLE_PROCESSED_OUTPUT | ENABLE_WRAP_AT_EOL_OUTPUT | ENABLE_VIRTUAL_TERMINAL_PROCESSING | DISABLE_NEWLINE_AUTO_RETURN | ENABLE_LVB_GRID_WORLDWIDE)); // DECAWM ✔️ LNM ✔️

        const auto expected =
            decawm(h) lnm(l) // DECAWM ✔️ LNM ✔️
            lnm(h) // DECAWM ✔️ LNM ✖️
            decawm(l) lnm(l) // DECAWM ✖️ LNM ✔️
            lnm(h) // DECAWM ✖️ LNM ✖️
            decawm(h) // DECAWM ✔️ LNM ✖️
            lnm(l); // DECAWM ✔️ LNM ✔️
        const auto actual = readOutput();
        VERIFY_ARE_EQUAL(expected, actual);
    }

    TEST_METHOD(SetConsoleTextAttribute)
    {
        for (WORD i = 0; i < 16; i++)
        {
            THROW_IF_FAILED(routines.SetConsoleTextAttributeImpl(*screenInfo, i));
        }

        for (WORD i = 0; i < 16; i++)
        {
            THROW_IF_FAILED(routines.SetConsoleTextAttributeImpl(*screenInfo, i << 4));
        }

        THROW_IF_FAILED(routines.SetConsoleTextAttributeImpl(*screenInfo, FOREGROUND_BLUE | FOREGROUND_RED | FOREGROUND_INTENSITY | BACKGROUND_GREEN | COMMON_LVB_REVERSE_VIDEO));
        THROW_IF_FAILED(routines.SetConsoleTextAttributeImpl(*screenInfo, FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_RED | COMMON_LVB_REVERSE_VIDEO));

        const auto expected =
            // 16 foreground colors
            "\x1b[27;30;40m"
            "\x1b[27;34;40m"
            "\x1b[27;32;40m"
            "\x1b[27;36;40m"
            "\x1b[27;31;40m"
            "\x1b[27;35;40m"
            "\x1b[27;33;40m"
            "\x1b[27;39;49m" // <-- FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_RED gets translated to the default colors
            "\x1b[27;90;40m"
            "\x1b[27;94;40m"
            "\x1b[27;92;40m"
            "\x1b[27;96;40m"
            "\x1b[27;91;40m"
            "\x1b[27;95;40m"
            "\x1b[27;93;40m"
            "\x1b[27;97;40m"
            // 16 background colors
            "\x1b[27;30;40m"
            "\x1b[27;30;44m"
            "\x1b[27;30;42m"
            "\x1b[27;30;46m"
            "\x1b[27;30;41m"
            "\x1b[27;30;45m"
            "\x1b[27;30;43m"
            "\x1b[27;30;47m"
            "\x1b[27;30;100m"
            "\x1b[27;30;104m"
            "\x1b[27;30;102m"
            "\x1b[27;30;106m"
            "\x1b[27;30;101m"
            "\x1b[27;30;105m"
            "\x1b[27;30;103m"
            "\x1b[27;30;107m"
            // The remaining two calls
            "\x1b[7;95;42m"
            "\x1b[7;39;49m";
        const auto actual = readOutput();
        VERIFY_ARE_EQUAL(expected, actual);
    }

    TEST_METHOD(SetConsoleTitleW)
    {
        THROW_IF_FAILED(routines.SetConsoleTitleWImpl(L"foobar"));

        const auto expected = "\x1b]0;foobar\a";
        const auto actual = readOutput();
        VERIFY_ARE_EQUAL(expected, actual);
    }

    TEST_METHOD(SetConsoleActiveScreenBuffer)
    {
        //THROW_IF_FAILED(routines.SetConsoleActiveScreenBufferImpl());
        THROW_IF_FAILED(routines.SetConsoleTitleWImpl(L"foobar"));

        const auto expected = "\x1b]0;foobar\a";
        const auto actual = readOutput();
        VERIFY_ARE_EQUAL(expected, actual);
    }

    TEST_METHOD(ScrollConsoleScreenBufferW)
    {
        const char* expected = nullptr;
        std::string_view actual;

        setupInitialContents();

        // Scrolling from nowhere to somewhere are no-ops and should not emit anything.
        THROW_IF_FAILED(routines.ScrollConsoleScreenBufferWImpl(*screenInfo, { 0, 0, -1, -1 }, {}, std::nullopt, L' ', 0, false));
        THROW_IF_FAILED(routines.ScrollConsoleScreenBufferWImpl(*screenInfo, { -10, -10, -9, -9 }, {}, std::nullopt, L' ', 0, false));
        expected = "";
        actual = readOutput();
        VERIFY_ARE_EQUAL(expected, actual);

        // Scrolling from somewhere to nowhere should clear the area.
        THROW_IF_FAILED(routines.ScrollConsoleScreenBufferWImpl(*screenInfo, { 0, 0, 1, 1 }, { 10, 10 }, std::nullopt, L' ', FOREGROUND_RED, false));
        expected =
            cup(1, 1) sgr_red("  ") sgr_rst() //
            cup(2, 1) sgr_red("  ") sgr_rst() //
            cup(1, 1);
        actual = readOutput();
        VERIFY_ARE_EQUAL(expected, actual);

        // cmd uses ScrollConsoleScreenBuffer to clear the buffer contents and that gets translated to a clear screen sequence.
        THROW_IF_FAILED(routines.ScrollConsoleScreenBufferWImpl(*screenInfo, { 0, 0, 7, 3 }, { 0, -4 }, std::nullopt, 0, 0, true));
        expected = "\033c";
        actual = readOutput();
        VERIFY_ARE_EQUAL(expected, actual);

        //
        //   A   B   a   b   C   D   c   d
        //
        //   E   F   e   f   G   H   g   h
        //
        //   i   j   I   J   k   l   K   L
        //
        //   m   n   M   N   o   p   O   P
        //
        setupInitialContents();

        // Scrolling from somewhere to somewhere.
        //
        //     +-------+
        //   A | Z   Z | b   C   D   c   d
        //     |  src  |
        //   E | Z   Z | f   G   H   g   h
        //     +-------+       +-------+
        //   i   j   I   J   k | B   a | L
        //                     |  dst  |
        //   m   n   M   N   o | F   e | P
        //                     +-------+
        THROW_IF_FAILED(routines.ScrollConsoleScreenBufferWImpl(*screenInfo, { 1, 0, 2, 1 }, { 5, 2 }, std::nullopt, L'Z', FOREGROUND_RED, false));
        expected =
            cup(1, 2) sgr_red("ZZ") sgr_rst() //
            cup(2, 2) sgr_red("ZZ") sgr_rst() //
            cup(3, 6) sgr_red("B") sgr_blu("a") sgr_rst() //
            cup(4, 6) sgr_red("F") sgr_blu("e") sgr_rst() //
            cup(1, 1);
        actual = readOutput();
        VERIFY_ARE_EQUAL(expected, actual);

        // Same, but with a partially out-of-bounds target and clip rect. Clip rects affect both
        // the source area that gets filled and the target area that gets a copy of the source contents.
        //
        //   A   Z   Z   b   C   D   c   d
        // +---+~~~~~~~~~~~~~~~~~~~~~~~+
        // | E $ z   z | f   G   H   g $ h
        // |   $ src   |           +---$-------+
        // | i $ z   z | J   k   B | E $ L     |
        // +---$-------+           |   $ dst   |
        //   m $ n   M   N   o   F | i $ P     |
        //     +~~~~~~~~~~~~~~~~~~~~~~~+-------+
        //            clip rect
        THROW_IF_FAILED(routines.ScrollConsoleScreenBufferWImpl(*screenInfo, { 0, 1, 2, 2 }, { 6, 2 }, til::inclusive_rect{ 1, 1, 6, 3 }, L'z', FOREGROUND_BLUE, false));
        expected =
            cup(2, 2) sgr_blu("zz") sgr_rst() //
            cup(3, 2) sgr_blu("zz") sgr_rst() //
            cup(3, 7) sgr_red("E") sgr_rst() //
            cup(4, 7) sgr_blu("i") sgr_rst() //
            cup(1, 1);
        actual = readOutput();
        VERIFY_ARE_EQUAL(expected, actual);

        // Same, but with a partially out-of-bounds source.
        // The boundaries of the buffer act as a clip rect for reading and so only 2 cells get copied.
        //
        //                             +-------+
        //   A   Z   Z   b   C   D   c | Y     |
        //                             |  src  |
        //   E   z   z   f   G   H   g | Y     |
        //                 +---+       +-------+
        //   i   z   z   J | d | B   E   L
        //                 |dst|
        //   m   n   M   N | h | F   i   P
        //                 +---+
        THROW_IF_FAILED(routines.ScrollConsoleScreenBufferWImpl(*screenInfo, { 7, 0, 8, 1 }, { 4, 2 }, std::nullopt, L'Y', FOREGROUND_RED, false));
        expected =
            cup(1, 8) sgr_red("Y") sgr_rst() //
            cup(2, 8) sgr_red("Y") sgr_rst() //
            cup(3, 5) sgr_blu("d") sgr_rst() //
            cup(4, 5) sgr_blu("h") sgr_rst() //
            cup(1, 1);
        actual = readOutput();
        VERIFY_ARE_EQUAL(expected, actual);

        static constexpr std::array<CHAR_INFO, 8 * 4> expectedContents{ {
            // clang-format off
            red('A'), red('Z'), red('Z'), blu('b'), red('C'), red('D'), blu('c'), red('Y'),
            red('E'), blu('z'), blu('z'), blu('f'), red('G'), red('H'), blu('g'), red('Y'),
            blu('i'), blu('z'), blu('z'), red('J'), blu('d'), red('B'), red('E'), red('L'),
            blu('m'), blu('n'), red('M'), red('N'), blu('h'), red('F'), blu('i'), red('P'),
            // clang-format on
        } };
        std::array<CHAR_INFO, 8 * 4> actualContents{};
        Viewport actualContentsRead;
        THROW_IF_FAILED(routines.ReadConsoleOutputWImpl(*screenInfo, actualContents, Viewport::FromDimensions({ 8, 4 }), actualContentsRead));
        VERIFY_IS_TRUE(memcmp(expectedContents.data(), actualContents.data(), sizeof(actualContents)) == 0);
    }

    TEST_METHOD(FillConsoleOutputAttribute)
    {
        setupInitialContents();

        size_t cellsModified = 0;
        const char* expected = nullptr;
        std::string_view actual;

        // Writing nothing should produce nothing.
        THROW_IF_FAILED(routines.FillConsoleOutputAttributeImpl(*screenInfo, FOREGROUND_RED, 0, {}, cellsModified));
        expected = "";
        actual = readOutput();
        VERIFY_ARE_EQUAL(expected, actual);

        // Writing at the start of a line.
        THROW_IF_FAILED(routines.FillConsoleOutputAttributeImpl(*screenInfo, FOREGROUND_RED, 3, { 0, 0 }, cellsModified));
        expected = cup(1, 1) sgr_red("ABa") sgr_rst() //
            cup(1, 1);
        actual = readOutput();
        VERIFY_ARE_EQUAL(expected, actual);

        // Writing at the end of a line.
        THROW_IF_FAILED(routines.FillConsoleOutputAttributeImpl(*screenInfo, FOREGROUND_RED, 3, { 5, 0 }, cellsModified));
        expected = cup(1, 6) sgr_red("Dcd") sgr_rst() //
            cup(1, 1);
        actual = readOutput();
        VERIFY_ARE_EQUAL(expected, actual);

        // Writing across 2 lines.
        THROW_IF_FAILED(routines.FillConsoleOutputAttributeImpl(*screenInfo, FOREGROUND_BLUE, 8, { 4, 1 }, cellsModified));
        expected =
            cup(2, 5) sgr_blu("GHgh") sgr_rst() //
            cup(3, 1) sgr_blu("ijIJ") sgr_rst() //
            cup(1, 1);
        actual = readOutput();
        VERIFY_ARE_EQUAL(expected, actual);
    }

    TEST_METHOD(FillConsoleOutputCharacterW)
    {
        setupInitialContents();

        size_t cellsModified = 0;
        const char* expected = nullptr;
        std::string_view actual;

        // Writing nothing should produce nothing.
        THROW_IF_FAILED(routines.FillConsoleOutputCharacterWImpl(*screenInfo, L'a', 0, {}, cellsModified));
        expected = "";
        actual = readOutput();
        VERIFY_ARE_EQUAL(expected, actual);

        // Writing at the start of a line.
        THROW_IF_FAILED(routines.FillConsoleOutputCharacterWImpl(*screenInfo, L'a', 3, { 0, 0 }, cellsModified));
        expected = cup(1, 1) sgr_red("aa") sgr_blu("a") sgr_rst() //
            cup(1, 1);
        actual = readOutput();
        VERIFY_ARE_EQUAL(expected, actual);

        // Writing at the end of a line.
        THROW_IF_FAILED(routines.FillConsoleOutputCharacterWImpl(*screenInfo, L'b', 3, { 5, 0 }, cellsModified));
        expected = cup(1, 6) sgr_red("b") sgr_blu("bb") sgr_rst() //
            cup(1, 1);
        actual = readOutput();
        VERIFY_ARE_EQUAL(expected, actual);

        // Writing across 2 lines.
        THROW_IF_FAILED(routines.FillConsoleOutputCharacterWImpl(*screenInfo, L'c', 8, { 4, 1 }, cellsModified));
        expected =
            cup(2, 5) sgr_red("cc") sgr_blu("cc") sgr_rst() //
            cup(3, 1) sgr_blu("cc") sgr_red("cc") sgr_rst() //
            cup(1, 1);
        actual = readOutput();
        VERIFY_ARE_EQUAL(expected, actual);
    }

    TEST_METHOD(WriteConsoleW)
    {
        size_t written;
        std::unique_ptr<IWaitRoutine> waiter;
        const char* expected = nullptr;
        std::string_view actual;

        resetContents();

        THROW_IF_FAILED(routines.WriteConsoleWImpl(*screenInfo, L"", written, false, waiter));
        expected = "";
        actual = readOutput();
        VERIFY_ARE_EQUAL(expected, actual);

        THROW_IF_FAILED(routines.WriteConsoleWImpl(*screenInfo, L"a\t\r\nb", written, false, waiter));
        expected = "a\t \r\r\nb";
        actual = readOutput();
        VERIFY_ARE_EQUAL(expected, actual);
    }

    TEST_METHOD(WriteConsoleOutputW)
    {
        //THROW_IF_FAILED(routines.WriteConsoleOutputWImpl());
        THROW_IF_FAILED(routines.SetConsoleTitleWImpl(L"foobar"));

        const auto expected = "\x1b]0;foobar\a";
        const auto actual = readOutput();
        VERIFY_ARE_EQUAL(expected, actual);
    }

    TEST_METHOD(WriteConsoleOutputAttribute)
    {
        //THROW_IF_FAILED(routines.WriteConsoleOutputAttributeImpl());
        THROW_IF_FAILED(routines.SetConsoleTitleWImpl(L"foobar"));

        const auto expected = "\x1b]0;foobar\a";
        const auto actual = readOutput();
        VERIFY_ARE_EQUAL(expected, actual);
    }

    TEST_METHOD(WriteConsoleOutputCharacterW)
    {
        //THROW_IF_FAILED(routines.WriteConsoleOutputCharacterWImpl());
        THROW_IF_FAILED(routines.SetConsoleTitleWImpl(L"foobar"));

        const auto expected = "\x1b]0;foobar\a";
        const auto actual = readOutput();
        VERIFY_ARE_EQUAL(expected, actual);
    }
};
