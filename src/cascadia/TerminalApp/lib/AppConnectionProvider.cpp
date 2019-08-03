#include "pch.h"
#include "AppConnectionProvider.h"

#include "winrt/Windows.ApplicationModel.h"
#include "winrt/Windows.Storage.h"
#include "winrt/Windows.Storage.Streams.h"
#include "winrt/Windows.Foundation.Collections.h"
#include "winrt/Windows.Storage.Search.h"

using namespace winrt;
using namespace winrt::Microsoft::Terminal::TerminalConnection;

struct CompositeTerminalConnectionProvider : implements<CompositeTerminalConnectionProvider, ITerminalConnectionProvider>
{
    CompositeTerminalConnectionProvider(std::vector<ITerminalConnectionProvider> providers) :
        _providers(providers)
    {
    }

    ITerminalConnectionFactory GetFactory(guid id)
    {
        for (auto provider : _providers)
        {
            auto factory = provider.GetFactory(id);

            if (factory != nullptr)
            {
                return factory;
            }
        }

        return nullptr;
    }

    com_array<ITerminalConnectionFactory> GetFactories()
    {
        std::vector<ITerminalConnectionFactory> factories;

        for (auto provider : _providers)
        {
            for (auto factory : provider.GetFactories())
            {
                factories.push_back(factory);
            }
        }

        return com_array<ITerminalConnectionFactory>(factories);
    }

private:
    const std::vector<ITerminalConnectionProvider> _providers;
};

struct WrappedModuleProvider : implements<WrappedModuleProvider, ITerminalConnectionProvider>
{
    WrappedModuleProvider(ITerminalConnectionProvider other, HMODULE dll) :
        _other(other),
        _dll(dll)
    {
    }

    ~WrappedModuleProvider()
    {
        // Set stored provider to null before freeing the library so the IUnknown et al destructor implementations can be called
        _other = nullptr;
        FreeLibrary(_dll);
        _dll = 0;
    }

    ITerminalConnectionFactory GetFactory(guid id)
    {
        return _other.GetFactory(id);
    }

    com_array<ITerminalConnectionFactory> GetFactories()
    {
        return _other.GetFactories();
    }

private:
    ITerminalConnectionProvider _other;
    HMODULE _dll;
};

struct OptionalPackageTerminalConnectionProvider : CompositeTerminalConnectionProvider
{
    OptionalPackageTerminalConnectionProvider() :
        CompositeTerminalConnectionProvider(LoadProvidersFromOptionalPackages())
    {
    }

private:
    static bool endsWith(const std::wstring_view& str, const std::wstring_view& suffix)
    {
        return str.size() >= suffix.size() && 0 == str.compare(str.size() - suffix.size(), suffix.size(), suffix);
    }

    static bool startsWith(const std::wstring_view& str, const std::wstring_view& prefix)
    {
        return str.size() >= prefix.size() && 0 == str.compare(0, prefix.size(), prefix);
    }

    static bool IsPlugin(std::wstring_view path)
    {
        return startsWith(path, L"Terminal.Plugin.") && endsWith(path, L".dll");
    }

    typedef winrt::Microsoft::Terminal::TerminalConnection::ITerminalConnectionProvider(__stdcall* GetConnectionProvider)();

    static void DebugOut(const TCHAR* fmt, ...)
    {
        TCHAR s[1025];
        va_list args;
        va_start(args, fmt);
        wvsprintf(s, fmt, args);
        va_end(args);
        OutputDebugString(s);
    }

    static std::vector<ITerminalConnectionProvider> LoadProvidersFromOptionalPackages()
    {
        std::vector<ITerminalConnectionProvider> factories;

        auto currentAppPackage = winrt::Windows::ApplicationModel::Package::Current();

        for (auto package : currentAppPackage.Dependencies())
        {
            if (package.IsOptional())
            {
                auto query = package.InstalledLocation().CreateFileQuery();
                auto files = query.GetFilesAsync().get();

                for (auto file : files)
                {
                    if (!file.IsAvailable())
                    {
                        continue;
                    }

                    auto name = file.Name();

                    if (IsPlugin(name))
                    {
                        auto dll = LoadPackagedLibrary(name.data(), 0);

                        if (dll == nullptr)
                        {
                            auto hr = HRESULT_FROM_WIN32(GetLastError());

                            DebugOut(L"Could not load '%s' from '%s' [HRESULT 0x%IX]. Ensure the package is in a related set and was signed by a valid certificate.\n", name.data(), package.Id().FullName().data(), hr);
                        }
                        else
                        {
                            const char* ConnectionProviderName = "GetConnectionProvider";
                            auto procAddress = GetProcAddress(dll, ConnectionProviderName);

                            if (procAddress == nullptr)
                            {
                                DebugOut(L"Could not find connection provider via entrypoint '%S' in '%s' from '%s'. Must have an entrypoint named '%S'.\n", ConnectionProviderName, name.data(), package.Id().FullName().data());
                                FreeLibrary(dll);
                            }
                            else
                            {
                                DebugOut(L"Found connection provider in '%s' from '%s'.\n", name.data(), package.Id().FullName().data());
                                auto func = (GetConnectionProvider)procAddress;
                                auto raw = func();
                                auto wrapped = make<WrappedModuleProvider>(raw, dll);

                                factories.push_back(wrapped);
                            }
                        }
                    }
                }
            }
        }

        return factories;
    }
};

ITerminalConnectionProvider GetTerminalConnectionProvider()
{
    return make<OptionalPackageTerminalConnectionProvider>();
}
