#include "pch.h"
#include <filesystem>
#include <mutex>
#include <til/env.h>
#include "../inc/TitleVariables.h"

using namespace WEX::TestExecution;
using namespace winrt::Microsoft::Terminal::TerminalConnection;

namespace TerminalAppLocalTests
{
    class ConptyTitleTests
    {
        BEGIN_TEST_CLASS(ConptyTitleTests)
            TEST_CLASS_PROPERTY(L"ThreadingModel", L"MTA")
        END_TEST_CLASS()

        TEST_METHOD_SETUP(Setup)
        {
            winrt::init_apartment();
            const auto testModule = GetModuleHandleW(L"TerminalApp.LocalTests.dll");
            VERIFY_IS_NOT_NULL(testModule);
            const auto directory = std::filesystem::path{ wil::GetModuleFileNameW<std::wstring>(testModule) }.parent_path();
            _module.reset(LoadLibraryExW((directory / L"TerminalConnection.dll").c_str(), nullptr, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS));
            VERIFY_IS_NOT_NULL(_module.get());
            HMODULE pinnedModule{};
            VERIFY_IS_TRUE(GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_PIN | GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, reinterpret_cast<LPCWSTR>(_module.get()), &pinnedModule));
            using GetFactory = HRESULT(WINAPI*)(HSTRING, void**);
            const auto getFactory = reinterpret_cast<GetFactory>(GetProcAddress(_module.get(), "DllGetActivationFactory"));
            VERIFY_IS_NOT_NULL(getFactory);
            const winrt::hstring className{ L"Microsoft.Terminal.TerminalConnection.ConptyConnection" };
            winrt::check_hresult(getFactory(static_cast<HSTRING>(winrt::get_abi(className)), winrt::put_abi(_factory)));
            return true;
        }

        TEST_METHOD_CLEANUP(Cleanup)
        {
            _factory = nullptr;
            _module.reset();
            winrt::uninit_apartment();
            return true;
        }

        TEST_METHOD(LaunchMetadataSnapshot)
        {
            namespace Variables = ::Microsoft::Terminal::TitleVariables;
            const auto connection = MakeConnection(L"override", L" /v:on /k \"set TITLE_TEST_SNAPSHOT=changed&echo TITLE_TEST_!TITLE_TEST_SNAPSHOT!\"");
            auto close = wil::scope_exit([&] { connection.Close(); });
            VERIFY_IS_TRUE(connection.TitleVariable(Variables::ShellName).empty());
            VERIFY_IS_TRUE(connection.TitleVariable(Variables::ShellPid).empty());
            VERIFY_IS_TRUE(connection.TitleVariable(L"env:TITLE_TEST_SNAPSHOT").empty());

            struct OutputState
            {
                std::mutex mutex;
                std::wstring text;
                wil::unique_event ready{ wil::EventOptions::ManualReset };
            };
            const auto output = std::make_shared<OutputState>();
            const auto token = connection.TerminalOutput([output](winrt::array_view<const char16_t> text) {
                const std::scoped_lock lock{ output->mutex };
                output->text.append(reinterpret_cast<const wchar_t*>(text.data()), text.size());
                if (output->text.find(L"TITLE_TEST_changed") != std::wstring::npos)
                {
                    output->ready.SetEvent();
                }
                if (output->text.size() > 8192)
                {
                    output->text.erase(0, output->text.size() - 4096);
                }
            });
            const auto revoke = wil::scope_exit([&] { connection.TerminalOutput(token); });
            connection.Start();
            VERIFY_ARE_EQUAL(ConnectionState::Connected, connection.State());
            VERIFY_ARE_EQUAL(winrt::hstring{ L"cmd.exe" }, connection.TitleVariable(Variables::ShellName));
            const auto pid = GetProcessId(reinterpret_cast<HANDLE>(connection.RootProcessHandle()));
            VERIFY_ARE_NOT_EQUAL(DWORD{ 0 }, pid);
            VERIFY_ARE_EQUAL(winrt::to_hstring(static_cast<uint32_t>(pid)), connection.TitleVariable(Variables::ShellPid));
            VERIFY_ARE_EQUAL(winrt::hstring{ L"override" }, connection.TitleVariable(L"env:title_test_snapshot"));
            VERIFY_ARE_EQUAL(winrt::hstring{ L"override" }, connection.TitleVariable(L"env:TITLE_TEST_SNAPSHOT"));
            VERIFY_ARE_EQUAL(winrt::hstring{ L"base-only" }, connection.TitleVariable(L"env:TITLE_TEST_BASE"));
            VERIFY_IS_TRUE(connection.TitleVariable(L"env:TITLE_TEST_MISSING").empty());
            VERIFY_ARE_EQUAL(winrt::hstring{ ::Microsoft::Console::Utils::GuidToPlainString(connection.SessionId()) }, connection.TitleVariable(L"env:WT_SESSION"));
            VERIFY_ARE_EQUAL(winrt::hstring{ L"{6239a42c-1111-49a3-80bd-e8fdd045185c}" }, connection.TitleVariable(L"env:WT_PROFILE_ID"));
            const std::wstring wslEnv{ connection.TitleVariable(L"env:WSLENV") };
            VERIFY_IS_TRUE(wslEnv.find(L"WT_SESSION") != std::wstring_view::npos);
            VERIFY_IS_TRUE(wslEnv.find(L"WT_PROFILE_ID") != std::wstring_view::npos);
            VERIFY_IS_TRUE(wslEnv.find(L"TITLE_TEST_SNAPSHOT") != std::wstring_view::npos);
            VERIFY_IS_TRUE(wslEnv.find(L"EXISTING/p") != std::wstring_view::npos);
            const auto receivedMarker = output->ready.wait(10000);
            if (!receivedMarker)
            {
                const std::scoped_lock lock{ output->mutex };
                auto diagnostic = output->text;
                for (auto& character : diagnostic)
                {
                    if (character < L' ')
                    {
                        character = L' ';
                    }
                }
                WEX::Logging::Log::Comment(diagnostic.c_str());
            }
            VERIFY_IS_TRUE(receivedMarker);
            VERIFY_ARE_EQUAL(winrt::hstring{ L"override" }, connection.TitleVariable(L"env:TITLE_TEST_SNAPSHOT"));
            connection.Close();
            close.release();
            VERIFY_ARE_EQUAL(winrt::to_hstring(static_cast<uint32_t>(pid)), connection.TitleVariable(Variables::ShellPid));
            VERIFY_ARE_EQUAL(winrt::hstring{ L"cmd.exe" }, connection.TitleVariable(Variables::ShellName));
            VERIFY_ARE_EQUAL(winrt::hstring{ L"override" }, connection.TitleVariable(L"env:TITLE_TEST_SNAPSHOT"));
        }

        TEST_METHOD(IndependentConnectionSnapshots)
        {
            const auto first = MakeConnection(L"first");
            auto closeFirst = wil::scope_exit([&] { first.Close(); });
            first.Start();
            VERIFY_ARE_EQUAL(ConnectionState::Connected, first.State());
            const auto second = MakeConnection(L"second");
            const auto closeSecond = wil::scope_exit([&] { second.Close(); });
            VERIFY_IS_TRUE(second.TitleVariable(L"shellPid").empty());
            VERIFY_IS_TRUE(second.TitleVariable(L"env:TITLE_TEST_SNAPSHOT").empty());
            second.Start();
            VERIFY_ARE_EQUAL(ConnectionState::Connected, second.State());
            VERIFY_ARE_NOT_EQUAL(first.TitleVariable(L"shellPid"), second.TitleVariable(L"shellPid"));
            VERIFY_ARE_NOT_EQUAL(first.TitleVariable(L"env:WT_SESSION"), second.TitleVariable(L"env:WT_SESSION"));
            VERIFY_ARE_EQUAL(winrt::hstring{ L"first" }, first.TitleVariable(L"env:TITLE_TEST_SNAPSHOT"));
            VERIFY_ARE_EQUAL(winrt::hstring{ L"second" }, second.TitleVariable(L"env:TITLE_TEST_SNAPSHOT"));
            first.Close();
            closeFirst.release();
            VERIFY_ARE_EQUAL(ConnectionState::Connected, second.State());
            VERIFY_ARE_EQUAL(winrt::hstring{ L"second" }, second.TitleVariable(L"env:TITLE_TEST_SNAPSHOT"));
        }

    private:
        ConptyConnection MakeConnection(const winrt::hstring& value, const std::wstring_view shellArguments = L"")
        {
            auto environment = til::env::from_current_environment();
            environment.set_user_environment_var(L"TITLE_TEST_SNAPSHOT", L"base");
            environment.set_user_environment_var(L"TITLE_TEST_BASE", L"base-only");
            environment.set_user_environment_var(L"TITLE_TEST_MISSING", L"");
            environment.set_user_environment_var(L"WSLENV", L"EXISTING/p");
            const auto block = environment.to_string();
            const auto overrides = winrt::single_threaded_map<winrt::hstring, winrt::hstring>();
            overrides.Insert(L"TITLE_TEST_SNAPSHOT", value);
            wchar_t systemDirectory[MAX_PATH]{};
            VERIFY_IS_TRUE(GetSystemDirectoryW(systemDirectory, ARRAYSIZE(systemDirectory)) != 0);
            const winrt::hstring command{ L"\"" + std::wstring{ systemDirectory } + L"\\cmd.exe\" /d /q" + std::wstring{ shellArguments } };
            const auto settings = _factory.as<IConptyConnectionStatics>().CreateSettings(
                command, systemDirectory, L"Title metadata test", false, winrt::hstring{ block.data(), static_cast<uint32_t>(block.size()) }, overrides.GetView(), 30, 120, {}, winrt::guid{ L"{6239a42c-1111-49a3-80bd-e8fdd045185c}" });
            const auto connection = _factory.ActivateInstance<ConptyConnection>();
            connection.Initialize(settings);
            return connection;
        }

        wil::unique_hmodule _module;
        winrt::Windows::Foundation::IActivationFactory _factory{ nullptr };
    };
}
