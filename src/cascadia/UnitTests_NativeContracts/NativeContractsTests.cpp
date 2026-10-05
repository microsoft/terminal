// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "../TerminalSettingsModel/Native/MediaResource.h"
#include "../TerminalSettingsModel/Native/MediaResourceList.h"

#include <windows.h>
#include <WexTestClass.h>
#include <array>

#if defined(WINRT_Microsoft_Terminal_Settings_Model_H) || defined(WINRT_TerminalApp_H)
#error Native contract tests must not import product projections.
#endif

using namespace WEX::TestExecution;
namespace Native = Microsoft::Terminal::Settings::Model::Native;

namespace NativeContractsUnitTests
{
    class MediaResourceTests
    {
        TEST_CLASS(MediaResourceTests);

        TEST_CLASS_SETUP(Initialize)
        {
            winrt::init_apartment(winrt::apartment_type::multi_threaded);
            return true;
        }

        TEST_CLASS_CLEANUP(Uninitialize)
        {
            winrt::uninit_apartment();
            return true;
        }

        TEST_METHOD(ResolutionState)
        {
            const auto resource = Native::MediaResource::FromString(L"relative.png");
            VERIFY_IS_TRUE(resource->Path() == L"relative.png");
            VERIFY_IS_TRUE(resource->Resolved() == L"relative.png");
            VERIFY_IS_FALSE(resource->Ok());

            resource->Resolve(L"C:\\images\\relative.png");
            VERIFY_IS_TRUE(resource->Path() == L"relative.png");
            VERIFY_IS_TRUE(resource->Resolved() == L"C:\\images\\relative.png");
            VERIFY_IS_TRUE(resource->Ok());

            resource->Reject();
            VERIFY_IS_TRUE(resource->Path() == L"relative.png");
            VERIFY_IS_TRUE(resource->Resolved().empty());
            VERIFY_IS_FALSE(resource->Ok());

            resource->Resolve(L"recovered.png");
            VERIFY_IS_TRUE(resource->Resolved() == L"recovered.png");
            VERIFY_IS_TRUE(resource->Ok());
        }

        TEST_METHOD(StrongAndWeakAcrossDll)
        {
            auto resource = Native::MediaResource::FromString(L"sound.wav");
            const auto weak = resource->get_weak();
            auto strong = resource->get_strong();
            auto inspectable = resource.as<winrt::Windows::Foundation::IInspectable>();
            VERIFY_IS_TRUE(strong.get() == resource.get());

            resource = nullptr;
            strong = nullptr;
            VERIFY_IS_TRUE(weak.get() != nullptr);
            VERIFY_IS_TRUE(weak.get()->Path() == L"sound.wav");

            inspectable = nullptr;
            VERIFY_IS_TRUE(weak.get() == nullptr);
        }

        TEST_METHOD(EmptyResourceIdentity)
        {
            auto first = Native::MediaResource::Empty();
            auto second = Native::MediaResource::Empty();
            const auto weak = first->get_weak();
            VERIFY_IS_TRUE(first.get() == second.get());
            VERIFY_IS_TRUE(first->Path().empty());
            VERIFY_IS_TRUE(first->Resolved().empty());
            VERIFY_IS_FALSE(first->Ok());

            first = nullptr;
            VERIFY_IS_TRUE(weak.get() != nullptr);
            second = nullptr;
            VERIFY_IS_TRUE(weak.get() == nullptr);
        }

        TEST_METHOD(OwningModuleLifetime)
        {
            const auto module = GetModuleHandleW(L"Microsoft.Terminal.Settings.Model.dll");
            VERIFY_IS_NOT_NULL(module);
            const auto canUnload = reinterpret_cast<HRESULT(WINAPI*)()>(GetProcAddress(module, "DllCanUnloadNow"));
            VERIFY_IS_NOT_NULL(canUnload);
            VERIFY_ARE_EQUAL(S_OK, canUnload());

            {
                auto resource = Native::MediaResource::FromString(L"owned.wav");
                const auto weak = resource->get_weak();
                VERIFY_ARE_EQUAL(S_FALSE, canUnload());
                resource = nullptr;
                VERIFY_IS_TRUE(weak.get() == nullptr);
                VERIFY_ARE_EQUAL(S_FALSE, canUnload());
            }

            VERIFY_ARE_EQUAL(S_OK, canUnload());
        }

        TEST_METHOD(NativeCollectionIdentityAndIteration)
        {
            const auto first = Native::MediaResource::FromString(L"first.wav");
            const auto second = Native::MediaResource::FromString(L"second.wav");
            auto values = Native::MakeMediaResourceList({ first });
            const auto alias = values;
            const auto beforeMutation = values->First();
            alias->Append(second);
            VERIFY_ARE_EQUAL(2u, values->Size());
            VERIFY_IS_TRUE(values->GetAt(0).get() == first.get());
            VERIFY_THROWS(beforeMutation->HasCurrent(), winrt::hresult_changed_state);

            uint32_t index = 99;
            VERIFY_IS_TRUE(values->IndexOf(second, index));
            VERIFY_ARE_EQUAL(1u, index);
            std::array<winrt::com_ptr<Native::MediaResource>, 3> buffer;
            const auto iterator = values->First();
            VERIFY_ARE_EQUAL(2u, iterator->GetMany(buffer));
            VERIFY_IS_TRUE(buffer[0].get() == first.get());
            VERIFY_IS_TRUE(buffer[1].get() == second.get());
            VERIFY_IS_FALSE(iterator->HasCurrent());
            VERIFY_THROWS(iterator->Current(), winrt::hresult_out_of_bounds);
            VERIFY_THROWS(values->GetAt(2), winrt::hresult_out_of_bounds);
        }
    };
}
