// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "winrt/Microsoft.Terminal.TerminalConnection.h"
#include "ConsoleInputReader.h"

using namespace winrt;
using namespace winrt::Windows::Foundation;
using namespace winrt::Microsoft::Terminal::TerminalConnection;

static til::size GetConsoleScreenSize(HANDLE outputHandle)
{
    CONSOLE_SCREEN_BUFFER_INFOEX csbiex{};
    csbiex.cbSize = sizeof(csbiex);
    GetConsoleScreenBufferInfoEx(outputHandle, &csbiex);
    return {
        (csbiex.srWindow.Right - csbiex.srWindow.Left) + 1,
        (csbiex.srWindow.Bottom - csbiex.srWindow.Top) + 1
    };
}

static ConnectionState RunConnectionToCompletion(const ITerminalConnection& connection, HANDLE outputHandle, HANDLE inputHandle)
{
    connection.TerminalOutput([outputHandle](const winrt::hstring& output) {
        WriteConsoleW(outputHandle, output.data(), output.size(), nullptr, nullptr);
    });

    // Detach a thread to spin the console read indefinitely.
    // This application exits when the connection is closed, so
    // the connection's lifetime will outlast this thread.
    std::thread([connection, outputHandle, inputHandle] {
        ConsoleInputReader reader{ inputHandle };
        reader.SetWindowSizeChangedCallback([&]() {
            const auto size = GetConsoleScreenSize(outputHandle);

            connection.Resize(size.height, size.width);
        });

        while (true)
        {
            auto input = reader.Read();
            if (input)
            {
                connection.WriteInput(*input);
            }
        }
    }).detach();

    std::condition_variable stateChangeVar;
    std::optional<ConnectionState> state;
    std::mutex stateMutex;

    connection.StateChanged([&](auto&& /*s*/, auto&& /*e*/) {
        std::unique_lock<std::mutex> lg{ stateMutex };
        state = connection.State();
        stateChangeVar.notify_all();
    });

    connection.Start();

    std::unique_lock<std::mutex> lg{ stateMutex };
    stateChangeVar.wait(lg, [&]() {
        if (!state.has_value())
        {
            return false;
        }
        return state.value() == ConnectionState::Closed || state.value() == ConnectionState::Failed;
    });

    return state.value();
}

int wmain(int /*argc*/, wchar_t** /*argv*/)
{
    winrt::init_apartment(winrt::apartment_type::single_threaded);

    DWORD inputMode{}, outputMode{};
    auto conIn{ GetStdHandle(STD_INPUT_HANDLE) }, conOut{ GetStdHandle(STD_OUTPUT_HANDLE) };
    auto codepage{ GetConsoleCP() }, outputCodepage{ GetConsoleOutputCP() };

    RETURN_IF_WIN32_BOOL_FALSE(GetConsoleMode(conIn, &inputMode));
    RETURN_IF_WIN32_BOOL_FALSE(GetConsoleMode(conOut, &outputMode));

    RETURN_IF_WIN32_BOOL_FALSE(SetConsoleMode(conIn, ENABLE_WINDOW_INPUT | ENABLE_VIRTUAL_TERMINAL_INPUT));
    RETURN_IF_WIN32_BOOL_FALSE(SetConsoleMode(conOut, ENABLE_PROCESSED_OUTPUT | ENABLE_VIRTUAL_TERMINAL_PROCESSING | ENABLE_WRAP_AT_EOL_OUTPUT | DISABLE_NEWLINE_AUTO_RETURN));
    RETURN_IF_WIN32_BOOL_FALSE(SetConsoleCP(CP_UTF8));
    RETURN_IF_WIN32_BOOL_FALSE(SetConsoleOutputCP(CP_UTF8));

    auto restoreConsoleModes = wil::scope_exit([&]() {
        SetConsoleMode(conIn, inputMode);
        SetConsoleMode(conOut, outputMode);
        SetConsoleCP(codepage);
        SetConsoleOutputCP(outputCodepage);
    });

    const auto size = GetConsoleScreenSize(conOut);

    AzureConnection azureConn{};
    winrt::Windows::Foundation::Collections::ValueSet vs{};
    vs.Insert(L"initialRows", winrt::Windows::Foundation::PropertyValue::CreateUInt32(gsl::narrow_cast<uint32_t>(size.height)));
    vs.Insert(L"initialCols", winrt::Windows::Foundation::PropertyValue::CreateUInt32(gsl::narrow_cast<uint32_t>(size.width)));
    azureConn.Initialize(vs);

    const auto state = RunConnectionToCompletion(azureConn, conOut, conIn);

    return state == ConnectionState::Closed ? 0 : 1;
}
