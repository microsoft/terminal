// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"

#include "../TerminalSettingsModel/ApplicationState.h"

using namespace Microsoft::Console;
using namespace WEX::Logging;
using namespace WEX::TestExecution;
using namespace WEX::Common;
using namespace winrt::Microsoft::Terminal::Settings::Model;

namespace SettingsModelUnitTests
{
    // All tests operate on a throw-away ApplicationState instance pointed at
    // a temp directory, so they don't touch the real user state.
    class ApplicationStateTests
    {
        TEST_CLASS(ApplicationStateTests);

        TEST_METHOD(SaveAndLookupWorkspace);
        TEST_METHOD(RemoveWorkspaceReturnsFalseWhenMissing);
        TEST_METHOD(RenameWorkspaceMigratesEntry);
        TEST_METHOD(RenameWorkspaceNoOpForEmptyOrEqualNames);
        TEST_METHOD(RenameWorkspaceNoOpForMissingEntry);
        TEST_METHOD(TakeWorkspaceRemovesAndReturns);
        TEST_METHOD(TakeWorkspaceReturnsNullWhenMissing);
        TEST_METHOD(IgnoreUnknownDismissedMessages);

    private:
        static std::filesystem::path _tempRoot()
        {
            auto root = std::filesystem::temp_directory_path() / L"WT_ApplicationStateTests";
            std::error_code ec;
            std::filesystem::create_directories(root, ec);
            // Best-effort clean of any leftover state.json from a prior run so
            // tests see an empty starting point.
            std::filesystem::remove(root / L"state.json", ec);
            std::filesystem::remove(root / L"elevated-state.json", ec);
            return root;
        }

        static winrt::com_ptr<implementation::ApplicationState> _make()
        {
            return winrt::make_self<implementation::ApplicationState>(_tempRoot());
        }

        static WindowLayout _makeLayout()
        {
            WindowLayout layout;
            layout.TabLayout(winrt::single_threaded_vector<ActionAndArgs>());
            return layout;
        }
    };

    void ApplicationStateTests::IgnoreUnknownDismissedMessages()
    {
        Log::Comment(L"GH #16874 - Unknown dismissed message names must not terminate state loading");

        Json::Value json{ Json::objectValue };
        json["settingsHash"] = "test-hash";
        json["dismissedMessages"] = Json::Value{ Json::arrayValue };
        for (const auto name : { "futureMessage", "closeOnExitInfo", "removedMessage", "keyboardServiceWarning", "setAsDefault", "" })
        {
            json["dismissedMessages"].append(name);
        }
        json["allowedCommandlines"] = Json::Value{ Json::arrayValue };
        json["allowedCommandlines"].append("test-command");

        auto state = _make();
        state->FromJson(json, implementation::FileSource::Shared | implementation::FileSource::Local);

        const auto messages = state->DismissedMessages();
        VERIFY_IS_NOT_NULL(messages);
        VERIFY_ARE_EQUAL(6u, messages.Size());
        VERIFY_ARE_EQUAL(InfoBarMessage::IgnoredDeprecatedEntry, messages.GetAt(0));
        VERIFY_ARE_EQUAL(InfoBarMessage::CloseOnExitInfo, messages.GetAt(1));
        for (uint32_t i = 2; i < messages.Size(); ++i)
        {
            VERIFY_ARE_EQUAL(InfoBarMessage::IgnoredDeprecatedEntry, messages.GetAt(i));
        }

        // Invalid names must not prevent fields before or after this one from loading.
        VERIFY_ARE_EQUAL(L"test-hash", state->SettingsHash());
        VERIFY_ARE_EQUAL(1u, state->AllowedCommandlines().Size());
        VERIFY_ARE_EQUAL(L"test-command", state->AllowedCommandlines().GetAt(0));

        const auto serialized = state->ToJson(implementation::FileSource::Shared);
        VERIFY_ARE_EQUAL(6u, serialized["dismissedMessages"].size());
        VERIFY_ARE_EQUAL("closeOnExitInfo", serialized["dismissedMessages"][1].asString());
        for (const auto i : { 0, 2, 3, 4, 5 })
        {
            VERIFY_ARE_EQUAL("deprecated", serialized["dismissedMessages"][i].asString());
        }
    }

    void ApplicationStateTests::SaveAndLookupWorkspace()
    {
        auto state = _make();
        const auto layout = _makeLayout();
        state->SaveWorkspace(L"win1", layout);

        const auto all = state->AllPersistedWorkspaces();
        VERIFY_IS_NOT_NULL(all);
        VERIFY_IS_TRUE(all.HasKey(L"win1"));
    }

    void ApplicationStateTests::RemoveWorkspaceReturnsFalseWhenMissing()
    {
        auto state = _make();
        VERIFY_IS_FALSE(state->RemoveWorkspace(L"does-not-exist"));

        state->SaveWorkspace(L"win1", _makeLayout());
        VERIFY_IS_TRUE(state->RemoveWorkspace(L"win1"));
        VERIFY_IS_FALSE(state->RemoveWorkspace(L"win1"));
    }

    void ApplicationStateTests::RenameWorkspaceMigratesEntry()
    {
        auto state = _make();
        state->SaveWorkspace(L"oldName", _makeLayout());

        VERIFY_IS_TRUE(state->RenameWorkspace(L"oldName", L"newName"));

        const auto all = state->AllPersistedWorkspaces();
        VERIFY_IS_NOT_NULL(all);
        VERIFY_IS_FALSE(all.HasKey(L"oldName"));
        VERIFY_IS_TRUE(all.HasKey(L"newName"));
    }

    void ApplicationStateTests::RenameWorkspaceNoOpForEmptyOrEqualNames()
    {
        auto state = _make();
        state->SaveWorkspace(L"win1", _makeLayout());

        VERIFY_IS_FALSE(state->RenameWorkspace(L"win1", L"win1"));
        VERIFY_IS_FALSE(state->RenameWorkspace(L"", L"win2"));

        // Renaming to an empty name removes the stale entry under the old name.
        VERIFY_IS_TRUE(state->RenameWorkspace(L"win1", L""));
        const auto all = state->AllPersistedWorkspaces();
        if (all)
        {
            VERIFY_IS_FALSE(all.HasKey(L"win1"));
            VERIFY_IS_FALSE(all.HasKey(L""));
        }

        // Calling again is now a no-op because the entry is gone.
        VERIFY_IS_FALSE(state->RenameWorkspace(L"win1", L""));
    }

    void ApplicationStateTests::RenameWorkspaceNoOpForMissingEntry()
    {
        auto state = _make();
        VERIFY_IS_FALSE(state->RenameWorkspace(L"missing", L"newName"));
    }

    void ApplicationStateTests::TakeWorkspaceRemovesAndReturns()
    {
        auto state = _make();
        state->SaveWorkspace(L"win1", _makeLayout());

        const auto taken = state->TakeWorkspace(L"win1");
        VERIFY_IS_NOT_NULL(taken);

        // Subsequent Take for the same name must return null.
        // This is the atomicity guarantee the startup path relies on.
        VERIFY_IS_NULL(state->TakeWorkspace(L"win1"));
    }

    void ApplicationStateTests::TakeWorkspaceReturnsNullWhenMissing()
    {
        auto state = _make();
        VERIFY_IS_NULL(state->TakeWorkspace(L"missing"));
    }
}
