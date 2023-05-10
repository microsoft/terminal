#include "pch.h"
#include "MainPage.h"
#include "MainPage.g.cpp"
#include "../../packages/Microsoft.Windows.ImplementationLibrary.1.0.220201.1/include/wil/win32_helpers.h"

#include <appmodel.h>
#include <Windows.ApplicationModel.AppExtensions.h>
#include <winrt/ExtensionHost.h>
#include "FooClass.h"
#include <winrt/ExtensionComponent.h>

using namespace winrt;
using namespace Windows::UI::Xaml;

namespace winrt::ExtensionHost::implementation
{
    int32_t MainPage::MyProperty()
    {
        throw hresult_not_implemented();
    }

    void MainPage::MyProperty(int32_t /* value */)
    {
        throw hresult_not_implemented();
    }

    static winrt::fire_and_forget _lookupCatalog() noexcept
    {
        co_await winrt::resume_background();
        try
        {
            auto cat = winrt::Windows::ApplicationModel::AppExtensions::AppExtensionCatalog::Open(winrt::hstring{ L"microsoft.terminal.scratch" });
            auto findOperation{ cat.FindAllAsync() };
            auto extnList = findOperation.get();
            for (const auto& extn : extnList)
            {
                extn;
            }
        }
        catch (...)
        {
            // LOG_CAUGHT_EXCEPTION();
            co_return;
        }

        /*ComPtr<IAppExtensionCatalogStatics> catalogStatics;
        RETURN_IF_FAILED(Windows::Foundation::GetActivationFactory(HStringReference(RuntimeClass_Windows_ApplicationModel_AppExtensions_AppExtensionCatalog).Get(), &catalogStatics));

        ComPtr<IAppExtensionCatalog> catalog;
        RETURN_IF_FAILED(catalogStatics->Open(HStringReference(extensionName).Get(), &catalog));

        ComPtr<IAsyncOperation<IVectorView<AppExtension*>*>> findOperation;
        RETURN_IF_FAILED(catalog->FindAllAsync(&findOperation));

        ComPtr<IVectorView<AppExtension*>> extensionList;
        RETURN_IF_FAILED(wil::wait_for_completion_nothrow(findOperation.Get(), &extensionList));

        UINT extensionCount;
        RETURN_IF_FAILED(extensionList->get_Size(&extensionCount));
        for (UINT index = 0; index < extensionCount; index++)
        {
        }*/

        co_return;
    }


    void MainPage::ClickHandler(IInspectable const&, RoutedEventArgs const&)
    {
        myButton().Content(box_value(L"Clicked"));

        auto hr = S_OK;

        _lookupCatalog();
        LOG_IF_FAILED(hr);
        if (FAILED(hr))
            return;

        PWSTR id;
        hr = TryCreatePackageDependency(nullptr, L"pfn", PACKAGE_VERSION{}, PackageDependencyProcessorArchitectures_None, PackageDependencyLifetimeKind_Process, nullptr, CreatePackageDependencyOptions_None, &id);
        LOG_IF_FAILED(hr);
        if (FAILED(hr))
            return;

        PACKAGEDEPENDENCY_CONTEXT ctx;
        PWSTR packageFullName;
        hr = AddPackageDependency(id, 1, AddPackageDependencyOptions_None, &ctx, &packageFullName);
        LOG_IF_FAILED(hr);
        if (FAILED(hr))
            return;

        winrt::ExtensionHost::FooClass myFoo{};
        winrt::ExtensionComponent::ExtensionClass bar;
        auto wat = bar.MyProperty();
        wat *= 2;
        myFoo.MyProperty(wat);
    }
}
