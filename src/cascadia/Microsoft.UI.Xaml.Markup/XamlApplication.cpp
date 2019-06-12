// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"

#include "XamlApplication.h"

#include "XamlApplication.g.cpp"

namespace xaml = ::winrt::Windows::UI::Xaml;

extern "C" {
WINBASEAPI HMODULE WINAPI LoadLibraryExW(_In_ LPCWSTR lpLibFileName, _Reserved_ HANDLE hFile, _In_ DWORD dwFlags);
WINBASEAPI HMODULE WINAPI GetModuleHandleW(_In_opt_ LPCWSTR lpModuleName);
WINUSERAPI BOOL WINAPI PeekMessageW(_Out_ LPMSG lpMsg, _In_opt_ HWND hWnd, _In_ UINT wMsgFilterMin, _In_ UINT wMsgFilterMax, _In_ UINT wRemoveMsg);
WINUSERAPI LRESULT WINAPI DispatchMessageW(_In_ CONST MSG* lpMsg);
}

namespace winrt::Microsoft::UI::Xaml::Markup::implementation
{
    XamlApplication::XamlApplication(winrt::Windows::UI::Xaml::Markup::IXamlMetadataProvider parentProvider)
    {
        m_providers.Append(parentProvider);
    }

    XamlApplication::XamlApplication()
    {
    }

    void XamlApplication::Close()
    {
        if (m_bIsClosed)
        {
            return;
        }

        m_bIsClosed = true;

        m_providers.Clear();

        Exit();
        {
            MSG msg = {};
            while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE))
            {
                ::DispatchMessageW(&msg);
            }
        }
    }

    XamlApplication::~XamlApplication()
    {
        Close();
    }

    xaml::Markup::IXamlType XamlApplication::GetXamlType(xaml::Interop::TypeName const& type)
    {
        for (const auto& provider : m_providers)
        {
            const auto xamlType = provider.GetXamlType(type);
            if (xamlType != nullptr)
            {
                return xamlType;
            }
        }

        return nullptr;
    }

    xaml::Markup::IXamlType XamlApplication::GetXamlType(winrt::hstring const& fullName)
    {
        for (const auto& provider : m_providers)
        {
            const auto xamlType = provider.GetXamlType(fullName);
            if (xamlType != nullptr)
            {
                return xamlType;
            }
        }

        return nullptr;
    }

    winrt::com_array<xaml::Markup::XmlnsDefinition> XamlApplication::GetXmlnsDefinitions()
    {
        std::list<xaml::Markup::XmlnsDefinition> definitions;
        for (const auto& provider : m_providers)
        {
            auto defs = provider.GetXmlnsDefinitions();
            for (const auto& def : defs)
            {
                definitions.insert(definitions.begin(), def);
            }
        }

        return winrt::com_array<xaml::Markup::XmlnsDefinition>(definitions.begin(), definitions.end());
    }

    winrt::Windows::Foundation::Collections::IVector<xaml::Markup::IXamlMetadataProvider> XamlApplication::Providers()
    {
        return m_providers;
    }
}

namespace winrt::Microsoft::UI::Xaml::Markup::factory_implementation
{
    XamlApplication::XamlApplication()
    {
        // Workaround a bug where twinapi.appcore.dll and threadpoolwinrt.dll gets loaded after it has been unloaded
        // because of a call to GetActivationFactory
        const wchar_t* preloadDlls[] = {
            L"twinapi.appcore.dll",
            L"threadpoolwinrt.dll",
        };
        for (auto dllName : preloadDlls)
        {
            const auto module = ::LoadLibraryExW(dllName, nullptr, 0);
            m_preloadInstances.push_back(module);
        }
    }

    XamlApplication::~XamlApplication()
    {
        for (auto module : m_preloadInstances)
        {
            ::FreeLibrary(module);
        }
        m_preloadInstances.clear();
    }
}
