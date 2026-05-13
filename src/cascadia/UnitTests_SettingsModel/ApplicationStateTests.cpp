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
    // Covers the workspace-persistence APIs added to ApplicationState:
    //   SaveWorkspace / RemoveWorkspace / RenameWorkspace / TakeWorkspace /
    //   AllPersistedWorkspaces.
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

        // Subsequent Take for the same name must return null — this is the
        // atomicity guarantee the startup path relies on.
        VERIFY_IS_NULL(state->TakeWorkspace(L"win1"));
    }

    void ApplicationStateTests::TakeWorkspaceReturnsNullWhenMissing()
    {
        auto state = _make();
        VERIFY_IS_NULL(state->TakeWorkspace(L"missing"));
    }
}
