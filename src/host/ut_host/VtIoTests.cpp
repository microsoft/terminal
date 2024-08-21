// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "CommonState.hpp"
#include "../../terminal/parser/InputStateMachineEngine.hpp"

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;

using namespace Microsoft::Console::Interactivity;
using namespace Microsoft::Console;
using namespace Microsoft::Console::VirtualTerminal;
using namespace Microsoft::Console::Render;
using namespace Microsoft::Console::Types;

static constexpr WORD red = FOREGROUND_RED | BACKGROUND_GREEN;
static constexpr WORD blu = FOREGROUND_BLUE | BACKGROUND_GREEN;

constexpr CHAR_INFO ci_red(wchar_t ch) noexcept
{
    return { ch, red };
}

constexpr CHAR_INFO ci_blu(wchar_t ch) noexcept
{
    return { ch, blu };
}

#define cup(y, x) "\x1b[" #y ";" #x "H" // CUP: Cursor Position
#define deccra(t, l, b, r, y, x) "\x1b[" #t ";" #l ";" #b ";" #r ";;" #y ";" #x "$v" // DECCRA: Copy Rectangular Area
#define decfra(ch, t, l, b, r) "\x1b[" #ch ";" #t ";" #l ";" #b ";" #r "$x"
#define decawm(h) "\x1b[?7" #h // DECAWM: Autowrap Mode
#define decsc() "\x1b\x37" // DECSC: DEC Save Cursor (+ attributes)
#define decrc() "\x1b\x38" // DECRC: DEC Restore Cursor (+ attributes)

// The escape sequences that ci_red() / ci_blu() result in.
#define sgr_red(s) "\x1b[0;31;42m" s
#define sgr_blu(s) "\x1b[0;34;42m" s
// What the default attributes `FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_RED` result in.
#define sgr_rst() "\x1b[0m"

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
        sm.ProcessString(L"\x1b[H" sgr_rst());
    }

    void resetContents() const
    {
        auto& sm = screenInfo->GetStateMachine();
        sm.ProcessString(L"\033c");
    }

    TEST_CLASS_SETUP(ClassSetup)
    {
        wil::unique_hfile tx;
        THROW_IF_WIN32_BOOL_FALSE(CreatePipe(rx.addressof(), tx.addressof(), nullptr, 16 * 1024));

        DWORD mode = PIPE_READMODE_BYTE | PIPE_NOWAIT;
        THROW_IF_WIN32_BOOL_FALSE(SetNamedPipeHandleState(rx.get(), &mode, nullptr, nullptr));

        commonState.PrepareGlobalInputBuffer();
        commonState.PrepareGlobalScreenBuffer(8, 4, 8, 4);

        auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
        THROW_IF_FAILED(gci.GetVtIo()->_Initialize(nullptr, tx.release(), nullptr));

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

        THROW_IF_FAILED(routines.SetConsoleOutputModeImpl(*screenInfo, ENABLE_PROCESSED_OUTPUT | ENABLE_WRAP_AT_EOL_OUTPUT | ENABLE_VIRTUAL_TERMINAL_PROCESSING | DISABLE_NEWLINE_AUTO_RETURN | ENABLE_LVB_GRID_WORLDWIDE)); // DECAWM ✔️
        THROW_IF_FAILED(routines.SetConsoleOutputModeImpl(*screenInfo, ENABLE_PROCESSED_OUTPUT | DISABLE_NEWLINE_AUTO_RETURN | ENABLE_LVB_GRID_WORLDWIDE)); // DECAWM ✖️
        THROW_IF_FAILED(routines.SetConsoleOutputModeImpl(*screenInfo, 0)); // DECAWM ✖️
        THROW_IF_FAILED(routines.SetConsoleOutputModeImpl(*screenInfo, ENABLE_PROCESSED_OUTPUT | ENABLE_WRAP_AT_EOL_OUTPUT | DISABLE_NEWLINE_AUTO_RETURN | ENABLE_LVB_GRID_WORLDWIDE)); // DECAWM ✔️

        const auto expected =
            decawm(h) // DECAWM ✔️
            decawm(l) // DECAWM ✖️
            decawm(h); // DECAWM ✔️
        const auto actual = readOutput();
        VERIFY_ARE_EQUAL(expected, actual);
    }

    TEST_METHOD(SetConsoleTitleW)
    {
        std::string_view expected;
        std::string_view actual;

        THROW_IF_FAILED(routines.SetConsoleTitleWImpl(
            L"foobar"));
        expected = "\x1b]0;foobar\x1b\\";
        actual = readOutput();
        VERIFY_ARE_EQUAL(expected, actual);

        THROW_IF_FAILED(routines.SetConsoleTitleWImpl(
            L"foo"
            "\u0001\u001f"
            "bar"));
        expected = "\x1b]0;foo☺▼bar\x1b\\";
        actual = readOutput();
        VERIFY_ARE_EQUAL(expected, actual);

        THROW_IF_FAILED(routines.SetConsoleTitleWImpl(
            L"foo"
            "\u0001\u001f"
            "bar"
            "\u007f\u009f"));
        expected = "\x1b]0;foo☺▼bar⌂?\x1b\\";
        actual = readOutput();
        VERIFY_ARE_EQUAL(expected, actual);
    }

    TEST_METHOD(SetConsoleCursorInfo)
    {
        THROW_IF_FAILED(routines.SetConsoleCursorInfoImpl(*screenInfo, 25, false));
        THROW_IF_FAILED(routines.SetConsoleCursorInfoImpl(*screenInfo, 25, true));

        const auto expected = "\x1b[?25l"
                              "\x1b[?25h";
        const auto actual = readOutput();
        VERIFY_ARE_EQUAL(expected, actual);
    }

    TEST_METHOD(SetConsoleTextAttribute)
    {
        for (WORD i = 0; i < 16; i++)
        {
            THROW_IF_FAILED(routines.SetConsoleTextAttributeImpl(*screenInfo, i | BACKGROUND_RED));
        }

        for (WORD i = 0; i < 16; i++)
        {
            THROW_IF_FAILED(routines.SetConsoleTextAttributeImpl(*screenInfo, i << 4 | FOREGROUND_RED));
        }

        THROW_IF_FAILED(routines.SetConsoleTextAttributeImpl(*screenInfo, FOREGROUND_BLUE | FOREGROUND_RED | FOREGROUND_INTENSITY | BACKGROUND_GREEN | COMMON_LVB_REVERSE_VIDEO));
        THROW_IF_FAILED(routines.SetConsoleTextAttributeImpl(*screenInfo, FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_RED | COMMON_LVB_REVERSE_VIDEO));

        const auto expected =
            // 16 foreground colors
            "\x1b[0;30;41m"
            "\x1b[0;34;41m"
            "\x1b[0;32;41m"
            "\x1b[0;36;41m"
            "\x1b[0;31;41m"
            "\x1b[0;35;41m"
            "\x1b[0;33;41m"
            "\x1b[0;41m" // <-- default foreground (FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_RED)
            "\x1b[0;90;41m"
            "\x1b[0;94;41m"
            "\x1b[0;92;41m"
            "\x1b[0;96;41m"
            "\x1b[0;91;41m"
            "\x1b[0;95;41m"
            "\x1b[0;93;41m"
            "\x1b[0;97;41m"
            // 16 background colors
            "\x1b[0;31m" // <-- default background (0)
            "\x1b[0;31;44m"
            "\x1b[0;31;42m"
            "\x1b[0;31;46m"
            "\x1b[0;31;41m"
            "\x1b[0;31;45m"
            "\x1b[0;31;43m"
            "\x1b[0;31;47m"
            "\x1b[0;31;100m"
            "\x1b[0;31;104m"
            "\x1b[0;31;102m"
            "\x1b[0;31;106m"
            "\x1b[0;31;101m"
            "\x1b[0;31;105m"
            "\x1b[0;31;103m"
            "\x1b[0;31;107m"
            // The remaining two calls
            "\x1b[0;7;95;42m"
            "\x1b[0;7m";
        const auto actual = readOutput();
        VERIFY_ARE_EQUAL(expected, actual);
    }

    TEST_METHOD(WriteConsoleW)
    {
        resetContents();

        size_t written;
        std::unique_ptr<IWaitRoutine> waiter;
        std::string_view expected;
        std::string_view actual;

        THROW_IF_FAILED(routines.WriteConsoleWImpl(*screenInfo, L"", written, waiter));
        expected = "";
        actual = readOutput();
        VERIFY_ARE_EQUAL(expected, actual);

        // Force-wrap because we write up to the last column.
        THROW_IF_FAILED(routines.WriteConsoleWImpl(*screenInfo, L"aaaaaaaa", written, waiter));
        expected = "aaaaaaaa\r\n";
        actual = readOutput();
        VERIFY_ARE_EQUAL(expected, actual);

        // Force-wrap because we write up to the last column, but this time with a tab.
        THROW_IF_FAILED(routines.WriteConsoleWImpl(*screenInfo, L"a\t\r\nb", written, waiter));
        expected = "a\t\r\n\r\nb";
        actual = readOutput();
        VERIFY_ARE_EQUAL(expected, actual);
    }

    TEST_METHOD(WriteConsoleOutputW)
    {
        resetContents();

        std::array payload{ ci_red('a'), ci_red('b'), ci_blu('A'), ci_blu('B') };
        const auto target = Viewport::FromDimensions({ 1, 1 }, { 4, 1 });
        Viewport written;
        THROW_IF_FAILED(routines.WriteConsoleOutputWImpl(*screenInfo, payload, target, written));

        const auto expected = decsc() cup(2, 2) sgr_red("ab") sgr_blu("AB") decrc();
        const auto actual = readOutput();
        VERIFY_ARE_EQUAL(expected, actual);
    }

    TEST_METHOD(WriteConsoleOutputAttribute)
    {
        setupInitialContents();

        static constexpr std::array payload{ red, blu, red, blu };
        static constexpr til::point target{ 6, 1 };
        size_t written;
        THROW_IF_FAILED(routines.WriteConsoleOutputAttributeImpl(*screenInfo, payload, target, written));

        const auto expected =
            decsc() //
            cup(2, 7) sgr_red("g") sgr_blu("h") //
            cup(3, 1) sgr_red("i") sgr_blu("j") //
            decrc();
        const auto actual = readOutput();
        VERIFY_ARE_EQUAL(expected, actual);
    }

    TEST_METHOD(WriteConsoleOutputCharacterW)
    {
        setupInitialContents();

        size_t written = 0;
        std::string_view expected;
        std::string_view actual;

        THROW_IF_FAILED(routines.WriteConsoleOutputCharacterWImpl(*screenInfo, L"foobar", { 5, 1 }, written));
        expected =
            decsc() //
            cup(2, 6) sgr_red("f") sgr_blu("oo") //
            cup(3, 1) sgr_blu("ba") sgr_red("r") //
            decrc();
        actual = readOutput();
        VERIFY_ARE_EQUAL(6u, written);
        VERIFY_ARE_EQUAL(expected, actual);

        // Writing past the end of the buffer.
        THROW_IF_FAILED(routines.WriteConsoleOutputCharacterWImpl(*screenInfo, L"foobar", { 5, 3 }, written));
        expected =
            decsc() //
            cup(4, 6) sgr_blu("f") sgr_red("oo") //
            decrc();
        actual = readOutput();
        VERIFY_ARE_EQUAL(3u, written);
        VERIFY_ARE_EQUAL(expected, actual);

        // Writing 3 wide chars while intersecting the last column.
        THROW_IF_FAILED(routines.WriteConsoleOutputCharacterWImpl(*screenInfo, L"✨✅❌", { 5, 1 }, written));
        expected =
            decsc() //
            cup(2, 6) sgr_red("✨") sgr_blu(" ") //
            cup(3, 1) sgr_blu("✅") sgr_red("❌") //
            decrc();
        actual = readOutput();
        VERIFY_ARE_EQUAL(3u, written);
        VERIFY_ARE_EQUAL(expected, actual);
    }

    TEST_METHOD(FillConsoleOutputAttribute)
    {
        size_t cellsModified = 0;
        std::string_view expected;
        std::string_view actual;

        // Writing nothing should produce nothing.
        THROW_IF_FAILED(routines.FillConsoleOutputAttributeImpl(*screenInfo, red, 0, {}, cellsModified, false));
        expected = "";
        actual = readOutput();
        VERIFY_ARE_EQUAL(0u, cellsModified);
        VERIFY_ARE_EQUAL(expected, actual);

        // PowerShell uses ScrollConsoleScreenBufferW + FillConsoleOutputCharacterW to
        // clear the buffer contents and that gets translated to a clear screen sequence.
        // We ignore FillConsoleOutputCharacterW in favor of ScrollConsoleScreenBufferW.
        THROW_IF_FAILED(routines.FillConsoleOutputAttributeImpl(*screenInfo, FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_RED, 8 * 4, {}, cellsModified, true));
        expected = "";
        actual = readOutput();
        VERIFY_ARE_EQUAL(expected, actual);

        setupInitialContents();

        // Writing at the start of a line.
        THROW_IF_FAILED(routines.FillConsoleOutputAttributeImpl(*screenInfo, red, 3, { 0, 0 }, cellsModified, false));
        expected =
            decsc() //
            cup(1, 1) sgr_red("ABa") //
            decrc();
        actual = readOutput();
        VERIFY_ARE_EQUAL(3u, cellsModified);
        VERIFY_ARE_EQUAL(expected, actual);

        // Writing at the end of a line.
        THROW_IF_FAILED(routines.FillConsoleOutputAttributeImpl(*screenInfo, red, 3, { 5, 0 }, cellsModified, false));
        expected =
            decsc() //
            cup(1, 6) sgr_red("Dcd") //
            decrc();
        actual = readOutput();
        VERIFY_ARE_EQUAL(3u, cellsModified);
        VERIFY_ARE_EQUAL(expected, actual);

        // Writing across 2 lines.
        THROW_IF_FAILED(routines.FillConsoleOutputAttributeImpl(*screenInfo, blu, 8, { 4, 1 }, cellsModified, false));
        expected =
            decsc() //
            cup(2, 5) sgr_blu("GHgh") //
            cup(3, 1) sgr_blu("ijIJ") //
            decrc();
        actual = readOutput();
        VERIFY_ARE_EQUAL(8u, cellsModified);
        VERIFY_ARE_EQUAL(expected, actual);
    }

    TEST_METHOD(FillConsoleOutputCharacterW)
    {
        size_t cellsModified = 0;
        std::string_view expected;
        std::string_view actual;

        // Writing nothing should produce nothing.
        THROW_IF_FAILED(routines.FillConsoleOutputCharacterWImpl(*screenInfo, L'a', 0, {}, cellsModified, false));
        expected = "";
        actual = readOutput();
        VERIFY_ARE_EQUAL(expected, actual);

        // PowerShell uses ScrollConsoleScreenBufferW + FillConsoleOutputCharacterW to
        // clear the buffer contents and that gets translated to a clear screen sequence.
        THROW_IF_FAILED(routines.FillConsoleOutputCharacterWImpl(*screenInfo, L' ', 8 * 4, {}, cellsModified, true));
        expected = "\x1b[H\x1b[2J\x1b[3J";
        actual = readOutput();
        VERIFY_ARE_EQUAL(expected, actual);

        setupInitialContents();

        // Writing at the start of a line.
        THROW_IF_FAILED(routines.FillConsoleOutputCharacterWImpl(*screenInfo, L'a', 3, { 0, 0 }, cellsModified, false));
        expected =
            decsc() //
            cup(1, 1) sgr_red("aa") sgr_blu("a") //
            decrc();
        actual = readOutput();
        VERIFY_ARE_EQUAL(expected, actual);

        // Writing at the end of a line.
        THROW_IF_FAILED(routines.FillConsoleOutputCharacterWImpl(*screenInfo, L'b', 3, { 5, 0 }, cellsModified, false));
        expected =
            decsc() //
            cup(1, 6) sgr_red("b") sgr_blu("bb") //
            decrc();
        actual = readOutput();
        VERIFY_ARE_EQUAL(expected, actual);

        // Writing across 2 lines.
        THROW_IF_FAILED(routines.FillConsoleOutputCharacterWImpl(*screenInfo, L'c', 8, { 4, 1 }, cellsModified, false));
        expected =
            decsc() //
            cup(2, 5) sgr_red("cc") sgr_blu("cc") //
            cup(3, 1) sgr_blu("cc") sgr_red("cc") //
            decrc();
        actual = readOutput();
        VERIFY_ARE_EQUAL(expected, actual);

        // Writing 3 wide chars while intersecting the last column.
        THROW_IF_FAILED(routines.FillConsoleOutputCharacterWImpl(*screenInfo, L'✨', 3, { 5, 1 }, cellsModified, false));
        expected =
            decsc() //
            cup(2, 6) sgr_red("✨") sgr_blu(" ") //
            cup(3, 1) sgr_blu("✨") sgr_red("✨") //
            decrc();
        actual = readOutput();
        VERIFY_ARE_EQUAL(expected, actual);
    }

    TEST_METHOD(ScrollConsoleScreenBufferW)
    {
        std::string_view expected;
        std::string_view actual;

        setupInitialContents();

        // Scrolling from nowhere to somewhere are no-ops and should not emit anything.
        THROW_IF_FAILED(routines.ScrollConsoleScreenBufferWImpl(*screenInfo, { 0, 0, -1, -1 }, {}, std::nullopt, L' ', 0, false));
        THROW_IF_FAILED(routines.ScrollConsoleScreenBufferWImpl(*screenInfo, { -10, -10, -9, -9 }, {}, std::nullopt, L' ', 0, false));
        expected = "";
        actual = readOutput();
        VERIFY_ARE_EQUAL(expected, actual);

        // Scrolling from somewhere to nowhere should clear the area.
        THROW_IF_FAILED(routines.ScrollConsoleScreenBufferWImpl(*screenInfo, { 0, 0, 1, 1 }, { 10, 10 }, std::nullopt, L' ', red, false));
        expected =
            decsc() //
            cup(1, 1) sgr_red("  ") //
            cup(2, 1) sgr_red("  ") //
            decrc();
        actual = readOutput();
        VERIFY_ARE_EQUAL(expected, actual);

        // cmd uses ScrollConsoleScreenBuffer to clear the buffer contents and that gets translated to a clear screen sequence.
        THROW_IF_FAILED(routines.ScrollConsoleScreenBufferWImpl(*screenInfo, { 0, 0, 7, 3 }, { 0, -4 }, std::nullopt, 0, 0, true));
        expected = "\x1b[H\x1b[2J\x1b[3J";
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
        THROW_IF_FAILED(routines.ScrollConsoleScreenBufferWImpl(*screenInfo, { 1, 0, 2, 1 }, { 5, 2 }, std::nullopt, L'Z', red, false));
        expected =
            decsc() //
            cup(1, 2) sgr_red("ZZ") //
            cup(2, 2) sgr_red("ZZ") //
            cup(3, 6) sgr_red("B") sgr_blu("a") //
            cup(4, 6) sgr_red("F") sgr_blu("e") //
            decrc();
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
        THROW_IF_FAILED(routines.ScrollConsoleScreenBufferWImpl(*screenInfo, { 0, 1, 2, 2 }, { 6, 2 }, til::inclusive_rect{ 1, 1, 6, 3 }, L'z', blu, false));
        expected =
            decsc() //
            cup(2, 2) sgr_blu("zz") //
            cup(3, 2) sgr_blu("zz") //
            cup(3, 7) sgr_red("E") //
            cup(4, 7) sgr_blu("i") //
            decrc();
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
        THROW_IF_FAILED(routines.ScrollConsoleScreenBufferWImpl(*screenInfo, { 7, 0, 8, 1 }, { 4, 2 }, std::nullopt, L'Y', red, false));
        expected =
            decsc() //
            cup(1, 8) sgr_red("Y") //
            cup(2, 8) sgr_red("Y") //
            cup(3, 5) sgr_blu("d") //
            cup(4, 5) sgr_blu("h") //
            decrc();
        actual = readOutput();
        VERIFY_ARE_EQUAL(expected, actual);

        // Copying from a partially out-of-bounds source to a partially out-of-bounds target,
        // while source and target overlap and there's a partially out-of-bounds clip rect.
        //
        // Before:
        //                       clip rect
        //                +~~~~~~~~~~~~~~~~~~~~~+
        // +--------------$--------+            $
        // |     A   Z   Z$  b   C | D   c   Y  $
        // |              $+-------+------------$--+
        // |     E   z   z$| f   G | H   g   Y  $  |
        // |          src $|       |            $  |
        // |     i   z   z$| J   d | B   E   L  $  |
        // |              $|       |  dst       $  |
        // |     m   n   M$| N   h | F   i   P  $  |
        // +--------------$+-------+            $  |
        //                +~e~~~~~~~~~~~~~~~~~~~+  |
        //                 +-----------------------+
        //
        // After:
        //
        // +-----------------------+
        // |     A   Z   Z   y   y | D   c   Y
        // |               +-------+---------------+
        // |     E   z   z | y   A | Z   Z   b     |
        // |               |       |               |
        // |     i   z   z | y   E | z   z   f     |
        // |               |       |               |
        // |     m   n   M | y   i | z   z   J     |
        // +---------------+-------+               |
        //                 |                       |
        //                 +-----------------------+
        THROW_IF_FAILED(routines.ScrollConsoleScreenBufferWImpl(*screenInfo, { -1, 0, 4, 3 }, { 3, 1 }, til::inclusive_rect{ 3, -1, 7, 9 }, L'y', blu, false));
        expected =
            decsc() //
            cup(1, 4) sgr_blu("yy") //
            cup(2, 4) sgr_blu("yy") //
            cup(3, 4) sgr_blu("yy") //
            cup(4, 4) sgr_blu("yy") //
            cup(2, 4) sgr_blu("y") sgr_red("AZZ") sgr_blu("b") //
            cup(3, 4) sgr_blu("y") sgr_red("E") sgr_blu("zzf") //
            cup(4, 4) sgr_blu("yizz") sgr_red("J") //
            decrc();
        actual = readOutput();
        VERIFY_ARE_EQUAL(expected, actual);

        static constexpr std::array<CHAR_INFO, 8 * 4> expectedContents{ {
            // clang-format off
            ci_red('A'), ci_red('Z'), ci_red('Z'), ci_blu('y'), ci_blu('y'), ci_red('D'), ci_blu('c'), ci_red('Y'),
            ci_red('E'), ci_blu('z'), ci_blu('z'), ci_blu('y'), ci_red('A'), ci_red('Z'), ci_red('Z'), ci_blu('b'),
            ci_blu('i'), ci_blu('z'), ci_blu('z'), ci_blu('y'), ci_red('E'), ci_blu('z'), ci_blu('z'), ci_blu('f'),
            ci_blu('m'), ci_blu('n'), ci_red('M'), ci_blu('y'), ci_blu('i'), ci_blu('z'), ci_blu('z'), ci_red('J'),
            // clang-format on
        } };
        std::array<CHAR_INFO, 8 * 4> actualContents{};
        Viewport actualContentsRead;
        THROW_IF_FAILED(routines.ReadConsoleOutputWImpl(*screenInfo, actualContents, Viewport::FromDimensions({}, { 8, 4 }), actualContentsRead));
        VERIFY_IS_TRUE(memcmp(expectedContents.data(), actualContents.data(), sizeof(actualContents)) == 0);
    }

    TEST_METHOD(ScrollConsoleScreenBufferW_DECCRA)
    {
        ServiceLocator::LocateGlobals().getConsoleInformation().GetVtIo()->SetDeviceAttributes({ DeviceAttribute::RectangularAreaOperations });
        const auto cleanup = wil::scope_exit([]() {
            ServiceLocator::LocateGlobals().getConsoleInformation().GetVtIo()->SetDeviceAttributes({});
        });

        std::string_view expected;
        std::string_view actual;

        setupInitialContents();

        // Scrolling from nowhere to somewhere are no-ops and should not emit anything.
        THROW_IF_FAILED(routines.ScrollConsoleScreenBufferWImpl(*screenInfo, { 0, 0, -1, -1 }, {}, std::nullopt, L' ', 0, false));
        THROW_IF_FAILED(routines.ScrollConsoleScreenBufferWImpl(*screenInfo, { -10, -10, -9, -9 }, {}, std::nullopt, L' ', 0, false));
        expected = "";
        actual = readOutput();
        VERIFY_ARE_EQUAL(expected, actual);

        // Scrolling from somewhere to nowhere should clear the area.
        THROW_IF_FAILED(routines.ScrollConsoleScreenBufferWImpl(*screenInfo, { 0, 0, 1, 1 }, { 10, 10 }, std::nullopt, L' ', red, false));
        expected =
            decsc() //
            sgr_red() //
            decfra(32, 1, 1, 2, 2) // ' ' = 32
            decrc();
        actual = readOutput();
        VERIFY_ARE_EQUAL(expected, actual);

        // cmd uses ScrollConsoleScreenBuffer to clear the buffer contents and that gets translated to a clear screen sequence.
        THROW_IF_FAILED(routines.ScrollConsoleScreenBufferWImpl(*screenInfo, { 0, 0, 7, 3 }, { 0, -4 }, std::nullopt, 0, 0, true));
        expected = "\x1b[H\x1b[2J\x1b[3J";
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
        THROW_IF_FAILED(routines.ScrollConsoleScreenBufferWImpl(*screenInfo, { 1, 0, 2, 1 }, { 5, 2 }, std::nullopt, L'Z', red, false));
        expected =
            decsc() //
            sgr_red() //
            deccra(1, 2, 2, 3, 3, 6) //
            decfra(90, 1, 2, 2, 3) // 'Z' = 90
            decrc();
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
        THROW_IF_FAILED(routines.ScrollConsoleScreenBufferWImpl(*screenInfo, { 0, 1, 2, 2 }, { 6, 2 }, til::inclusive_rect{ 1, 1, 6, 3 }, L'z', blu, false));
        expected =
            decsc() //
            sgr_blu() //
            deccra(2, 1, 3, 1, 3, 7) //
            decfra(122, 2, 2, 3, 3) // 'z' = 122
            decrc();
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
        THROW_IF_FAILED(routines.ScrollConsoleScreenBufferWImpl(*screenInfo, { 7, 0, 8, 1 }, { 4, 2 }, std::nullopt, L'Y', red, false));
        expected =
            decsc() //
            sgr_red() //
            deccra(1, 8, 2, 8, 3, 5) //
            decfra(89, 1, 8, 2, 8) // 'Y' = 89
            decrc();
        actual = readOutput();
        VERIFY_ARE_EQUAL(expected, actual);

        // Copying from a partially out-of-bounds source to a partially out-of-bounds target,
        // while source and target overlap and there's a partially out-of-bounds clip rect.
        //
        // Before:
        //                       clip rect
        //                +~~~~~~~~~~~~~~~~~~~~~+
        // +--------------$--------+            $
        // |     A   Z   Z$  b   C | D   c   Y  $
        // |              $+-------+------------$--+
        // |     E   z   z$| f   G | H   g   Y  $  |
        // |          src $|       |            $  |
        // |     i   z   z$| J   d | B   E   L  $  |
        // |              $|       |  dst       $  |
        // |     m   n   M$| N   h | F   i   P  $  |
        // +--------------$+-------+            $  |
        //                +~e~~~~~~~~~~~~~~~~~~~+  |
        //                 +-----------------------+
        //
        // After:
        //
        // +-----------------------+
        // |     A   Z   Z   y   y | D   c   Y
        // |               +-------+---------------+
        // |     E   z   z | y   A | Z   Z   b     |
        // |               |       |               |
        // |     i   z   z | y   E | z   z   f     |
        // |               |       |               |
        // |     m   n   M | y   i | z   z   J     |
        // +---------------+-------+               |
        //                 |                       |
        //                 +-----------------------+
        THROW_IF_FAILED(routines.ScrollConsoleScreenBufferWImpl(*screenInfo, { -1, 0, 4, 3 }, { 3, 1 }, til::inclusive_rect{ 3, -1, 7, 9 }, L'y', blu, false));
        expected =
            decsc() //
            sgr_blu() //
            deccra(1, 1, 3, 4, 2, 5) //
            decfra(121, 1, 4, 1, 5) // 'y' = 121
            decfra(121, 2, 4, 4, 4) //
            decrc();
        actual = readOutput();
        VERIFY_ARE_EQUAL(expected, actual);

        static constexpr std::array<CHAR_INFO, 8 * 4> expectedContents{ {
            // clang-format off
            ci_red('A'), ci_red('Z'), ci_red('Z'), ci_blu('y'), ci_blu('y'), ci_red('D'), ci_blu('c'), ci_red('Y'),
            ci_red('E'), ci_blu('z'), ci_blu('z'), ci_blu('y'), ci_red('A'), ci_red('Z'), ci_red('Z'), ci_blu('b'),
            ci_blu('i'), ci_blu('z'), ci_blu('z'), ci_blu('y'), ci_red('E'), ci_blu('z'), ci_blu('z'), ci_blu('f'),
            ci_blu('m'), ci_blu('n'), ci_red('M'), ci_blu('y'), ci_blu('i'), ci_blu('z'), ci_blu('z'), ci_red('J'),
            // clang-format on
        } };
        std::array<CHAR_INFO, 8 * 4> actualContents{};
        Viewport actualContentsRead;
        THROW_IF_FAILED(routines.ReadConsoleOutputWImpl(*screenInfo, actualContents, Viewport::FromDimensions({}, { 8, 4 }), actualContentsRead));
        VERIFY_IS_TRUE(memcmp(expectedContents.data(), actualContents.data(), sizeof(actualContents)) == 0);
    }

    TEST_METHOD(SetConsoleActiveScreenBuffer)
    {
        SCREEN_INFORMATION* screenInfoAlt;

        VERIFY_NT_SUCCESS(SCREEN_INFORMATION::CreateInstance(
            screenInfo->GetViewport().Dimensions(),
            screenInfo->GetCurrentFont(),
            screenInfo->GetBufferSize().Dimensions(),
            screenInfo->GetAttributes(),
            screenInfo->GetPopupAttributes(),
            screenInfo->GetTextBuffer().GetCursor().GetSize(),
            &screenInfoAlt));

        routines.SetConsoleActiveScreenBufferImpl(*screenInfoAlt);
        setupInitialContents();
        THROW_IF_FAILED(routines.SetConsoleOutputModeImpl(*screenInfoAlt, ENABLE_PROCESSED_OUTPUT | ENABLE_WRAP_AT_EOL_OUTPUT | ENABLE_VIRTUAL_TERMINAL_PROCESSING));
        readOutput();

        routines.SetConsoleActiveScreenBufferImpl(*screenInfo);

        const auto expected =
            "\x1b[?1049l" // ASB (Alternate Screen Buffer)
            cup(1, 1) sgr_red("AB") sgr_blu("ab") sgr_red("CD") sgr_blu("cd") //
            cup(2, 1) sgr_red("EF") sgr_blu("ef") sgr_red("GH") sgr_blu("gh") //
            cup(3, 1) sgr_blu("ij") sgr_red("IJ") sgr_blu("kl") sgr_red("KL") //
            cup(4, 1) sgr_blu("mn") sgr_red("MN") sgr_blu("op") sgr_red("OP") //
            cup(1, 1) sgr_rst() //
            "\x1b[?25h" // DECTCEM (Text Cursor Enable)
            "\x1b[?7h"; // DECAWM (Autowrap Mode)
        const auto actual = readOutput();
        VERIFY_ARE_EQUAL(expected, actual);
    }
};
