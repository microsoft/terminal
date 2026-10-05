// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include <windows.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Microsoft.Terminal.Settings.Model.h>
#include "Boundary.h"

#include <memory>
#include <stdexcept>
#include <type_traits>

namespace
{
    struct FreeModule
    {
        void operator()(HMODULE module) const noexcept
        {
            FreeLibrary(module);
        }
    };

    using Module = std::unique_ptr<std::remove_pointer_t<HMODULE>, FreeModule>;

    winrt::Microsoft::Terminal::Settings::Model::NewTerminalArgs Activate(const std::wstring& path, const std::optional<int32_t>& profileIndex)
    {
        static const auto modelPath = path;
        if (path != modelPath)
        {
            throw std::invalid_argument("A prototype process must use one fixed model DLL path");
        }

        // Pin the backend for projected objects that outlive this call. Do not FreeLibrary in DLL static destruction.
        static const auto module = [&] {
            const Module loaded{ LoadLibraryExW(path.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH) };
            if (!loaded)
            {
                winrt::throw_last_error();
            }
            HMODULE pinned{};
            if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN,
                                   reinterpret_cast<LPCWSTR>(loaded.get()), &pinned))
            {
                winrt::throw_last_error();
            }
            return pinned;
        }();

        using GetFactory = HRESULT(__stdcall*)(void*, void**);
        const auto getFactory = reinterpret_cast<GetFactory>(GetProcAddress(module, "DllGetActivationFactory"));
        if (!getFactory)
        {
            winrt::throw_last_error();
        }

        const winrt::hstring name{ L"Microsoft.Terminal.Settings.Model.NewTerminalArgs" };
        winrt::Windows::Foundation::IActivationFactory factory{ nullptr };
        winrt::check_hresult(getFactory(winrt::get_abi(name), winrt::put_abi(factory)));
        if (profileIndex)
        {
            return factory.as<winrt::Microsoft::Terminal::Settings::Model::INewTerminalArgsFactory>().CreateInstance(*profileIndex);
        }
        return factory.ActivateInstance<winrt::Microsoft::Terminal::Settings::Model::NewTerminalArgs>();
    }
}

boundary::Request boundary::Load(const LaunchValues& input, const std::wstring& modelDll)
{
    auto args = Activate(modelDll, input.profileIndex);
    args.Commandline(input.commandline);
    args.StartingDirectory(input.directory);
    args.TabTitle(input.title);
    args.Profile(input.profile);
    if (input.elevate)
    {
        args.Elevate(*input.elevate);
    }

#ifdef NATIVE_BOUNDARY
    LaunchValues values;
    values.commandline = args.Commandline();
    values.directory = args.StartingDirectory();
    values.title = args.TabTitle();
    values.profile = args.Profile();
    if (const auto index = args.ProfileIndex())
    {
        values.profileIndex = index.Value();
    }
    if (const auto elevate = args.Elevate())
    {
        values.elevate = elevate.Value();
    }
    return values;
#else
    return args;
#endif
}
