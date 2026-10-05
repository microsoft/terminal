// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "../TerminalSettingsModel/Native/Profile.h"
#include "../TerminalSettingsModel/Native/AppearanceConfig.h"
#include "../TerminalSettingsModel/Native/FontConfig.h"

#include <WexTestClass.h>
#include <json/json.h>

#if defined(WINRT_Microsoft_Terminal_Settings_Model_H) || defined(WINRT_TerminalApp_H)
#error Native profile contracts must not import product projections.
#endif

using namespace WEX::TestExecution;
namespace Native = Microsoft::Terminal::Settings::Model::Native;

namespace NativeContractsUnitTests
{
    class ProfileTests
    {
        TEST_CLASS(ProfileTests);

        TEST_METHOD(InheritanceAndNullableValues)
        {
            const auto parent = Native::Profile::Create();
            parent->HistorySize(1000);
            parent->TabColor(Native::Color{ 0x12, 0x34, 0x56, 0xff });
            const auto child = parent->CreateChild();
            const auto grandchild = child->CreateChild();
            VERIFY_ARE_EQUAL(1000, child->HistorySize());
            VERIFY_IS_FALSE(child->HasHistorySize());
            VERIFY_IS_TRUE(child->HistorySizeOverrideSource().get() == parent.get());
            VERIFY_IS_TRUE(child->TabColor().has_value());

            child->HistorySize(2000);
            child->TabColor(std::nullopt);
            VERIFY_ARE_EQUAL(2000, grandchild->HistorySize());
            VERIFY_IS_TRUE(child->HasTabColor());
            VERIFY_IS_FALSE(grandchild->TabColor().has_value());
            VERIFY_IS_TRUE(grandchild->TabColorOverrideSource().get() == child.get());
            child->ClearTabColor();
            VERIFY_IS_TRUE(grandchild->TabColor().has_value());
            VERIFY_IS_TRUE(grandchild->TabColorOverrideSource().get() == parent.get());
        }

        TEST_METHOD(ClonePreservesSharedParents)
        {
            const auto parent = Native::Profile::Create();
            const auto first = parent->CreateChild();
            const auto second = parent->CreateChild();
            std::unordered_map<const Native::Profile*, winrt::com_ptr<Native::Profile>> visited;
            const auto firstCopy = first->CopyInheritanceGraph(visited);
            const auto secondCopy = second->CopyInheritanceGraph(visited);
            VERIFY_ARE_EQUAL(size_t{ 1 }, firstCopy->Parents().size());
            VERIFY_IS_TRUE(firstCopy->Parents()[0].get() == secondCopy->Parents()[0].get());
            VERIFY_IS_TRUE(firstCopy->Parents()[0].get() != parent.get());
            VERIFY_IS_TRUE(firstCopy->FontInfo()->SourceProfile().get() == firstCopy.get());
            VERIFY_IS_TRUE(firstCopy->DefaultAppearance()->SourceProfile().get() == firstCopy.get());
        }

        TEST_METHOD(CopyAndSerializeNativeValues)
        {
            Json::Value json{ Json::objectValue };
            json["name"] = "native";
            json["historySize"] = 5000;
            json["tabColor"] = "#123456";
            json["bellSound"].append("one.wav");
            json["font"]["axes"]["wght"] = 400;
            json["environment"]["NATIVE_CONTRACT_TEST"] = "original";

            const auto profile = Native::Profile::FromJson(json);
            const auto copy = profile->CopySettings();
            VERIFY_ARE_EQUAL(5000, copy->HistorySize());
            VERIFY_IS_TRUE(copy->Name() == L"native");
            VERIFY_IS_TRUE(copy->BellSound().get() != profile->BellSound().get());
            VERIFY_IS_TRUE(copy->BellSound()->GetAt(0).get() == profile->BellSound()->GetAt(0).get());
            copy->BellSound()->Append(Native::MediaResource::FromString(L"two.wav"));
            VERIFY_ARE_EQUAL(1u, profile->BellSound()->Size());
            copy->FontInfo()->FontAxes().Insert(L"wght", 700.0f);
            VERIFY_ARE_EQUAL(400.0f, profile->FontInfo()->FontAxes().Lookup(L"wght"));
            copy->EnvironmentVariables().Insert(L"NATIVE_CONTRACT_TEST", L"changed");
            VERIFY_IS_TRUE(profile->EnvironmentVariables().Lookup(L"NATIVE_CONTRACT_TEST") == L"changed");
            VERIFY_IS_TRUE(copy->ToJson()["name"].asString() == "native");
            VERIFY_ARE_EQUAL(2u, copy->ToJson()["bellSound"].size());
        }

        TEST_METHOD(WeakSourceProfile)
        {
            auto profile = Native::Profile::Create();
            const auto appearance = profile->DefaultAppearance();
            const auto font = profile->FontInfo();
            const auto weak = profile->get_weak();
            profile = nullptr;
            VERIFY_IS_TRUE(weak.get() == nullptr);
            VERIFY_IS_TRUE(appearance->SourceProfile() == nullptr);
            VERIFY_IS_TRUE(font->SourceProfile() == nullptr);
        }
    };
}
