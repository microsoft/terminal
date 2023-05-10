// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "MyPage.g.h"
#include "../../../src/cascadia/inc/cppwinrt_utils.h"

namespace winrt::SampleApp::implementation
{
    struct DynamicDependency
    {
        winrt::Windows::ApplicationModel::AppExtensions::AppExtension _extension{ nullptr };
        PWSTR _dependencyId{ nullptr };
        PACKAGEDEPENDENCY_CONTEXT _dependencyContext;
        winrt::hstring _implementationClassName;
        winrt::hstring _pfn;

        HRESULT Create(const winrt::Windows::ApplicationModel::AppExtensions::AppExtension& extn)
        {
            _extension = extn;
            _pfn = extn.Package().Id().FamilyName();
            RETURN_IF_FAILED(TryCreatePackageDependency(nullptr,
                                                        _pfn.c_str(),
                                                        PACKAGE_VERSION{},
                                                        PackageDependencyProcessorArchitectures_None,
                                                        PackageDependencyLifetimeKind_Process,
                                                        nullptr,
                                                        CreatePackageDependencyOptions_None,
                                                        &_dependencyId));
            RETURN_IF_FAILED(AddPackageDependency(_dependencyId,
                                                  1,
                                                  AddPackageDependencyOptions_None,
                                                  &_dependencyContext,
                                                  nullptr));
            return S_OK;
        }
        winrt::Windows::Foundation::IAsyncOperation<bool> ResolveProperties()
        {
            auto properties = co_await _extension.GetExtensionPropertiesAsync();
            if (properties)
            {
                if (const auto& s{ properties.TryLookup(L"Implementation") })
                {
                    // s is another property set, so look it up in that instead
                    if (const auto& asSet{ s.try_as<winrt::Windows::Foundation::Collections::IPropertySet>() })
                    {
                        _implementationClassName = asSet.TryLookup(L"#text").as<winrt::hstring>();
                        co_return true;
                    }
                }
            }
            co_return false;
        }
        ~DynamicDependency() noexcept
        {
            //if (_dependencyContext)
            //{
            //    RemovePackageDependency(_dependencyContext);
            //}
        }
    };

    struct MyPage : MyPageT<MyPage>
    {
    public:
        MyPage();

        void Create();

        hstring Title();

        void ClickHandler(Windows::Foundation::IInspectable const& sender, Windows::UI::Xaml::RoutedEventArgs const& args);
        winrt::fire_and_forget ActivateInstanceButtonHandler(Windows::Foundation::IInspectable const& sender, Windows::UI::Xaml::RoutedEventArgs const& args);

    private:
        friend struct MyPageT<MyPage>; // for Xaml to bind events
        winrt::Windows::Foundation::IAsyncAction _lookupCatalog() noexcept;

        winrt::Microsoft::Terminal::TerminalConnection::ITerminalConnection _connection{ nullptr };

        std::vector<DynamicDependency> _extensions;
    };
}

namespace winrt::SampleApp::factory_implementation
{
    BASIC_FACTORY(MyPage);
}
