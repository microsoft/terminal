// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "XamlApplication.g.h"
#include <winrt/Windows.UI.Xaml.Hosting.h>
#include <winrt/Windows.UI.ViewManagement.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <Windows.h>

namespace winrt::Microsoft::UI::Xaml::Markup::implementation
{
    class XamlApplication : public XamlApplicationT<XamlApplication, Windows::UI::Xaml::Markup::IXamlMetadataProvider>
    {
    public:
        XamlApplication();
        XamlApplication(winrt::Windows::UI::Xaml::Markup::IXamlMetadataProvider parentProvider);
        ~XamlApplication();

        void Close();

        winrt::Windows::UI::Xaml::Markup::IXamlType GetXamlType(winrt::Windows::UI::Xaml::Interop::TypeName const& type);
        winrt::Windows::UI::Xaml::Markup::IXamlType GetXamlType(winrt::hstring const& fullName);
        winrt::com_array<winrt::Windows::UI::Xaml::Markup::XmlnsDefinition> GetXmlnsDefinitions();

        winrt::Windows::Foundation::Collections::IVector<winrt::Windows::UI::Xaml::Markup::IXamlMetadataProvider> Providers();

    private:
        winrt::Windows::Foundation::Collections::IVector<winrt::Windows::UI::Xaml::Markup::IXamlMetadataProvider> m_providers = winrt::single_threaded_vector<Windows::UI::Xaml::Markup::IXamlMetadataProvider>();
        bool m_bIsClosed = false;
    };
}

namespace winrt::Microsoft::UI::Xaml::Markup::factory_implementation
{
    class XamlApplication : public XamlApplicationT<XamlApplication, implementation::XamlApplication>
    {
    public:
        XamlApplication();
        ~XamlApplication();

    private:
        std::vector<HMODULE> m_preloadInstances;
    };
}
