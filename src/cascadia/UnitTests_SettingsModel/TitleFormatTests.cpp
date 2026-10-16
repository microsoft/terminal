#include "pch.h"
#include "../inc/TitleFormat.h"
#include "../TerminalSettingsModel/Profile.h"
#include "../TerminalSettingsModel/GlobalAppSettings.h"
#include "../TerminalSettingsModel/CascadiaSettings.h"
#include "JsonTestClass.h"

using namespace WEX::TestExecution;
using namespace winrt::Microsoft::Terminal::Settings::Model;
using Microsoft::Terminal::TitleFormat;

namespace SettingsModelUnitTests
{
    class TitleFormatTests : public JsonTestClass
    {
        TEST_CLASS(TitleFormatTests);

        TEST_METHOD(Separators)
        {
            const auto resolve = [](std::wstring_view name) -> std::wstring {
                return name == Microsoft::Terminal::TitleVariables::PaneTitle ? L"PowerShell" : L"";
            };
            for (const auto text : { L"${paneTitle}", L"${separator}${paneTitle}${separator}", L"${paneTitle}${separator}${workspaceName}${separator}${separator}" })
            {
                VERIFY_ARE_EQUAL(std::wstring{ L"PowerShell" }, TitleFormat(text, TitleFormat::Surface::Tab).Evaluate(resolve, L" - "));
            }
            const TitleFormat format{ L"${paneTitle}${separator}${workspaceName}${separator}Terminal", TitleFormat::Surface::Tab };
            VERIFY_ARE_EQUAL(std::wstring{ L"PowerShell - Terminal" }, format.Evaluate(resolve, L" - "));
            VERIFY_ARE_EQUAL(std::wstring{ L"PowerShellTerminal" }, format.Evaluate(resolve, L""));
            VERIFY_ARE_EQUAL(std::wstring{ L"(+)" }, TitleFormat(L"(+${numOtherPanes})", TitleFormat::Surface::Tab).Evaluate(resolve, L" - "));
        }

        TEST_METHOD(SyntaxAndSinglePass)
        {
            const auto resolve = [](std::wstring_view) { return std::wstring{ L"${workspaceName}" }; };
            VERIFY_ARE_EQUAL(std::wstring{ L"${workspaceName}" }, TitleFormat(L"${paneTitle}", TitleFormat::Surface::Tab).Evaluate(resolve, L" - "));
            VERIFY_ARE_EQUAL(std::wstring{ LR"(${workspaceName} $ \)" }, TitleFormat(LR"($${workspaceName} $ \)", TitleFormat::Surface::Tab).Evaluate(resolve, L" - "));
            VERIFY_ARE_EQUAL(std::wstring{ L"$${paneTitle}" }, TitleFormat(L"$$${paneTitle}", TitleFormat::Surface::Tab).Evaluate(resolve, L" - "));
            VERIFY_ARE_EQUAL(std::wstring{ L"$$" }, TitleFormat(L"$$", TitleFormat::Surface::Tab).Evaluate(resolve, L" - "));
            for (const auto text : { L"${", L"${foo${bar}}", L"${numPanes > 1}", L"${env:}", L"${_name}", L"${tabTitle}" })
            {
                const TitleFormat format{ text, TitleFormat::Surface::Tab };
                VERIFY_IS_FALSE(format.Valid());
                VERIFY_IS_TRUE(format.HasWarning());
                VERIFY_IS_TRUE(format.Evaluate(resolve, L" - ").empty());
            }
            const TitleFormat unknown{ L"${PaneTitle}${unknown}", TitleFormat::Surface::Window };
            VERIFY_IS_TRUE(unknown.Valid());
            VERIFY_IS_TRUE(unknown.HasWarning());
            VERIFY_IS_TRUE(unknown.Evaluate(resolve, L" - ").empty());
            VERIFY_IS_TRUE(TitleFormat(L"${env:Project Name}", TitleFormat::Surface::Tab).Valid());
            VERIFY_IS_TRUE(TitleFormat(L"${tabTitle}", TitleFormat::Surface::Window).Valid());
        }

        TEST_METHOD(VariablesAndNormalization)
        {
            for (const auto name : { L"paneTitle", L"tabTitle", L"profileName", L"workspaceName", L"windowName", L"appName", L"tabIndex", L"numPanes", L"numOtherPanes", L"otherPanes", L"elevated", L"elevatedShield", L"shellName", L"shellPid", L"cwd", L"cwdFolder", L"env:PROJECT" })
            {
                const TitleFormat format{ std::wstring{ L"${" } + name + L"}", TitleFormat::Surface::Window };
                VERIFY_IS_FALSE(format.HasWarning());
                VERIFY_ARE_EQUAL(std::wstring{ L"value" }, format.Evaluate([](auto) { return L"value"; }, L" - "));
                VERIFY_IS_TRUE(format.Evaluate([](auto) { return L""; }, L" - ").empty());
            }
            const TitleFormat format{ L" \t${paneTitle}\r\n ", TitleFormat::Surface::Tab };
            VERIFY_ARE_EQUAL(std::wstring{ L"a b  c \u65e5\u672c\U0001F6E1" }, format.Evaluate([](auto) { return L"a\nb  c\x1b\u65e5\u672c\U0001F6E1"; }, L" - "));
            VERIFY_IS_TRUE(TitleFormat(L"", TitleFormat::Surface::Tab).Evaluate([](auto) { return L""; }, L" - ").empty());
        }

        TEST_METHOD(DirectoryNames)
        {
            for (const auto root : { L"/", L"\\", L"C:", L"C:\\", L"C:/", L"\\\\server\\share", L"\\\\server\\share\\", LR"(\\?\UNC\server\share)", LR"(\\?\UNC\server\share\)", LR"(\\?\unc\server\share\)", LR"(\\?\C:\)" })
            {
                VERIFY_ARE_EQUAL(std::wstring{ root }, TitleFormat::DirectoryName(root));
            }
            VERIFY_ARE_EQUAL(std::wstring{ L"src" }, TitleFormat::DirectoryName(L"/home/user/src/"));
            VERIFY_ARE_EQUAL(std::wstring{ L"src" }, TitleFormat::DirectoryName(L"C:\\dev\\src\\"));
            VERIFY_ARE_EQUAL(std::wstring{ L"a\\b" }, TitleFormat::DirectoryName(L"/home/a\\b"));
            VERIFY_ARE_EQUAL(std::wstring{ L"a\\b" }, TitleFormat::DirectoryName(L"a\\b"));
            VERIFY_ARE_EQUAL(std::wstring{ L"bar" }, TitleFormat::DirectoryName(LR"(\foo\bar)"));
            VERIFY_ARE_EQUAL(std::wstring{ L"foo" }, TitleFormat::DirectoryName(L"C:foo"));
            VERIFY_ARE_EQUAL(std::wstring{ L"bar" }, TitleFormat::DirectoryName(LR"(C:foo\bar)"));
            VERIFY_ARE_EQUAL(std::wstring{ L"src" }, TitleFormat::DirectoryName(LR"(\\?\UNC\server\share\src\)"));
            VERIFY_ARE_EQUAL(std::wstring{ L"src" }, TitleFormat::DirectoryName(LR"(\\?\C:\dev\src\)"));
            VERIFY_IS_TRUE(TitleFormat::DirectoryName(L"").empty());
        }

        TEST_METHOD(NullableSettings)
        {
            const auto parent = implementation::Profile::FromJson(VerifyParseSucceeded(R"({"tabTitleFormat":"${paneTitle}","tabTitleSeparator":" "})"));
            const auto child = parent->CreateChild();
            VERIFY_ARE_EQUAL(winrt::hstring{ L"${paneTitle}" }, child->TabTitleFormat().GetString());
            child->LayerJson(VerifyParseSucceeded(R"({"tabTitleFormat":null})"));
            VERIFY_IS_NULL(child->TabTitleFormat());
            VERIFY_IS_TRUE(child->ToJson()["tabTitleFormat"].isNull());
            VERIFY_IS_NULL(implementation::Profile::FromJson(child->ToJson())->TabTitleFormat());
            child->ClearTabTitleFormat();
            VERIFY_ARE_EQUAL(winrt::hstring{ L"${paneTitle}" }, child->TabTitleFormat().GetString());
            child->LayerJson(VerifyParseSucceeded(R"({"tabTitleFormat":42,"tabTitleSeparator":null})"));
            VERIFY_IS_TRUE(child->InvalidTitleSettings());
            VERIFY_ARE_EQUAL(winrt::hstring{ L" " }, child->TabTitleSeparator());
            VERIFY_ARE_EQUAL(winrt::hstring{ L"${paneTitle}" }, child->TabTitleFormat().GetString());
            child->LayerJson(VerifyParseSucceeded(R"({"tabTitleFormat":"","tabTitleSeparator":""})"));
            VERIFY_IS_NOT_NULL(child->TabTitleFormat());
            VERIFY_IS_TRUE(child->TabTitleFormat().GetString().empty());
            VERIFY_IS_TRUE(child->CopySettings()->TabTitleFormat().GetString().empty());
            const auto serialized = child->ToJson();
            VERIFY_IS_TRUE(serialized["tabTitleFormat"].isString());
            VERIFY_IS_TRUE(serialized["tabTitleSeparator"].isString());
            const auto reloaded = implementation::Profile::FromJson(serialized);
            VERIFY_IS_NOT_NULL(reloaded->TabTitleFormat());
            VERIFY_IS_TRUE(reloaded->TabTitleFormat().GetString().empty());
            VERIFY_IS_TRUE(reloaded->TabTitleSeparator().empty());
        }

        TEST_METHOD(ProjectedNullableSettings)
        {
            const auto empty = winrt::box_value(winrt::hstring{}).as<winrt::Windows::Foundation::IPropertyValue>();
            const auto parent = implementation::Profile::FromJson(VerifyParseSucceeded(R"({"tabTitleFormat":"${paneTitle}"})"));
            const auto child = parent->CreateChild();
            const auto profile = child.as<Profile>();
            const auto wrongType = winrt::box_value(42).as<winrt::Windows::Foundation::IPropertyValue>();
            VERIFY_IS_FALSE(profile.HasTabTitleFormat());
            VERIFY_THROWS(profile.TabTitleFormat(wrongType), winrt::hresult_error);
            VERIFY_IS_FALSE(profile.HasTabTitleFormat());
            VERIFY_ARE_EQUAL(winrt::hstring{ L"${paneTitle}" }, profile.TabTitleFormat().GetString());
            profile.TabTitleFormat(empty);
            VERIFY_IS_TRUE(profile.HasTabTitleFormat());
            VERIFY_IS_NOT_NULL(profile.TabTitleFormat());
            VERIFY_IS_TRUE(profile.TabTitleFormat().GetString().empty());
            VERIFY_THROWS(profile.TabTitleFormat(wrongType), winrt::hresult_error);
            VERIFY_IS_TRUE(profile.TabTitleFormat().GetString().empty());
            VERIFY_IS_TRUE(child->ToJson()["tabTitleFormat"].isString());
            profile.TabTitleFormat(nullptr);
            VERIFY_IS_TRUE(profile.HasTabTitleFormat());
            VERIFY_IS_NULL(profile.TabTitleFormat());
            VERIFY_IS_NULL(child->CopySettings()->TabTitleFormat());
            VERIFY_IS_TRUE(child->ToJson().isMember("tabTitleFormat"));
            VERIFY_IS_TRUE(child->ToJson()["tabTitleFormat"].isNull());
            profile.ClearTabTitleFormat();
            VERIFY_IS_FALSE(profile.HasTabTitleFormat());
            VERIFY_ARE_EQUAL(winrt::hstring{ L"${paneTitle}" }, profile.TabTitleFormat().GetString());

            const auto settings = implementation::GlobalAppSettings::FromJson(VerifyParseSucceeded(R"({})"));
            const auto globals = settings.as<GlobalAppSettings>();
            VERIFY_IS_FALSE(globals.HasWindowTitleFormat());
            VERIFY_THROWS(globals.WindowTitleFormat(wrongType), winrt::hresult_error);
            VERIFY_IS_FALSE(globals.HasWindowTitleFormat());
            globals.WindowTitleFormat(empty);
            VERIFY_IS_TRUE(globals.HasWindowTitleFormat());
            VERIFY_IS_NOT_NULL(globals.WindowTitleFormat());
            VERIFY_IS_TRUE(globals.WindowTitleFormat().GetString().empty());
            VERIFY_THROWS(globals.WindowTitleFormat(wrongType), winrt::hresult_error);
            VERIFY_IS_TRUE(globals.WindowTitleFormat().GetString().empty());
            VERIFY_IS_TRUE(settings->ToJson()["windowTitleFormat"].isString());
            globals.WindowTitleFormat(nullptr);
            VERIFY_IS_TRUE(globals.HasWindowTitleFormat());
            VERIFY_IS_NULL(globals.WindowTitleFormat());
            VERIFY_IS_NULL(settings->Copy()->WindowTitleFormat());
            VERIFY_IS_TRUE(settings->ToJson().isMember("windowTitleFormat"));
            VERIFY_IS_TRUE(settings->ToJson()["windowTitleFormat"].isNull());
            globals.ClearWindowTitleFormat();
            VERIFY_IS_FALSE(globals.HasWindowTitleFormat());
            VERIFY_ARE_EQUAL(winrt::hstring{ L"${workspaceName}${separator}${tabTitle}" }, globals.WindowTitleFormat().GetString());
        }

        TEST_METHOD(FragmentTitleWarnings)
        {
            constexpr std::string_view user = R"({"profiles":[{"guid":"{6239a42c-1111-49a3-80bd-e8fdd045185c}","name":"base","commandline":"cmd.exe"}]})";
            for (const auto fragment : {
                     R"({"profiles":[{"guid":"{6239a42c-2222-49a3-80bd-e8fdd045185c}","name":"fragment","commandline":"cmd.exe","tabTitleFormat":42}]})",
                     R"({"profiles":[{"updates":"{6239a42c-1111-49a3-80bd-e8fdd045185c}","tabTitleSeparator":null}]})" })
            {
                implementation::SettingsLoader loader{ user, std::string_view{} };
                loader.MergeInboxIntoUserSettings();
                loader.MergeFragmentIntoUserSettings(L"TitleTest", {}, fragment);
                loader.FinalizeLayering();
                const auto settings = winrt::make_self<implementation::CascadiaSettings>(std::move(loader));
                size_t titleWarnings = 0;
                for (const auto warning : settings->Warnings())
                {
                    if (warning == SettingsLoadWarnings::InvalidTitleFormat)
                        ++titleWarnings;
                }
                VERIFY_ARE_EQUAL(size_t{ 1 }, titleWarnings);
            }
        }

        TEST_METHOD(TitleWarningsAreDeduplicated)
        {
            constexpr std::string_view json = R"({
                "windowTitleFormat":"${unknown}", "windowTitleSeparator":null,
                "profiles": {
                    "defaults":{"tabTitleFormat":"${unknown}","tabTitleSeparator":null},
                    "list":[
                        {"name":"first","commandline":"cmd.exe","tabTitleFormat":42},
                        {"name":"second","commandline":"cmd.exe","tabTitleSeparator":false}
                    ]
                }
            })";
            const auto settings = winrt::make_self<implementation::CascadiaSettings>(json);
            size_t titleWarnings = 0;
            for (const auto warning : settings->Warnings())
            {
                if (warning == SettingsLoadWarnings::InvalidTitleFormat)
                    ++titleWarnings;
            }
            VERIFY_ARE_EQUAL(size_t{ 1 }, titleWarnings);
            VERIFY_ARE_EQUAL(winrt::hstring{ L" - " }, settings->GlobalSettings().WindowTitleSeparator());
        }

        TEST_METHOD(WindowMigration)
        {
            const auto defaults = implementation::GlobalAppSettings::FromJson(VerifyParseSucceeded(R"({})"));
            VERIFY_ARE_EQUAL(winrt::hstring{ L"${workspaceName}${separator}${tabTitle}" }, defaults->WindowTitleFormat().GetString());
            const auto legacy = implementation::GlobalAppSettings::FromJson(VerifyParseSucceeded(R"({"showTerminalTitleInTitlebar":false})"));
            VERIFY_IS_NULL(legacy->WindowTitleFormat());
            VERIFY_IS_TRUE(legacy->FixupsAppliedDuringLoad());
            VERIFY_IS_TRUE(legacy->ToJson().isMember("windowTitleFormat"));
            VERIFY_IS_TRUE(legacy->ToJson()["windowTitleFormat"].isNull());
            VERIFY_IS_NULL(implementation::GlobalAppSettings::FromJson(legacy->ToJson())->WindowTitleFormat());
            auto reenabledJson = legacy->ToJson();
            reenabledJson["showTerminalTitleInTitlebar"] = true;
            const auto reenabled = implementation::GlobalAppSettings::FromJson(reenabledJson);
            VERIFY_IS_NULL(reenabled->WindowTitleFormat());
            VERIFY_IS_TRUE(reenabled->ShowTitleInTitlebar());
            const auto explicitFormat = implementation::GlobalAppSettings::FromJson(VerifyParseSucceeded(R"({"showTerminalTitleInTitlebar":false,"windowTitleFormat":"caption"})"));
            VERIFY_ARE_EQUAL(winrt::hstring{ L"caption" }, explicitFormat->WindowTitleFormat().GetString());
            VERIFY_ARE_EQUAL(winrt::hstring{ L"caption" }, explicitFormat->Copy()->WindowTitleFormat().GetString());
            explicitFormat->LayerJson(VerifyParseSucceeded(R"({"showTerminalTitleInTitlebar":false})"), OriginTag::User);
            VERIFY_ARE_EQUAL(winrt::hstring{ L"caption" }, explicitFormat->WindowTitleFormat().GetString());
            const auto empty = implementation::GlobalAppSettings::FromJson(VerifyParseSucceeded(R"({"windowTitleFormat":"","windowTitleSeparator":""})"));
            const auto serialized = empty->ToJson();
            VERIFY_IS_TRUE(serialized["windowTitleFormat"].isString());
            VERIFY_IS_TRUE(serialized["windowTitleSeparator"].isString());
            const auto reloaded = implementation::GlobalAppSettings::FromJson(serialized);
            VERIFY_IS_NOT_NULL(reloaded->WindowTitleFormat());
            VERIFY_IS_TRUE(reloaded->WindowTitleFormat().GetString().empty());
            VERIFY_IS_TRUE(reloaded->WindowTitleSeparator().empty());
        }
    };
}