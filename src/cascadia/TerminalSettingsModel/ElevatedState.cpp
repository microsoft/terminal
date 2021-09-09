// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "ElevatedState.h"
#include "CascadiaSettings.h"
#include "ElevatedState.g.cpp"

#include "JsonUtils.h"
#include "FileUtils.h"

#include <aclapi.h>

constexpr std::wstring_view stateFileName{ L"elevated-state.json" };

using namespace ::Microsoft::Terminal::Settings::Model;

namespace winrt::Microsoft::Terminal::Settings::Model::implementation
{
    ElevatedState::ElevatedState(std::filesystem::path path) noexcept :
        BaseApplicationState{ path } {}

    // Returns the application-global ElevatedState object.
    Microsoft::Terminal::Settings::Model::ElevatedState ElevatedState::SharedInstance()
    {
        // TODO! place in a totally different file! and path!
        static auto state = winrt::make_self<ElevatedState>(GetBaseSettingsPath() / stateFileName);
        state->Reload();

        // const auto testPath{ GetBaseSettingsPath() / L"test.json" };

        // PSID pEveryoneSID = NULL;
        // SID_IDENTIFIER_AUTHORITY SIDAuthWorld = SECURITY_NT_AUTHORITY;
        // BOOL success = AllocateAndInitializeSid(&SIDAuthWorld, 1, SECURITY_LOCAL_SYSTEM_RID, 0, 0, 0, 0, 0, 0, 0, &pEveryoneSID);

        // EXPLICIT_ACCESS ea[1];
        // ZeroMemory(&ea, 1 * sizeof(EXPLICIT_ACCESS));
        // ea[0].grfAccessPermissions = KEY_READ;
        // ea[0].grfAccessMode = SET_ACCESS;
        // ea[0].grfInheritance = NO_INHERITANCE;
        // ea[0].Trustee.TrusteeForm = TRUSTEE_IS_SID;
        // ea[0].Trustee.TrusteeType = TRUSTEE_IS_WELL_KNOWN_GROUP;
        // ea[0].Trustee.ptstrName = (LPTSTR)pEveryoneSID;

        // ACL acl;
        // PACL pAcl = &acl;
        // DWORD dwRes = SetEntriesInAcl(1, ea, NULL, &pAcl);
        // dwRes;

        // SECURITY_DESCRIPTOR sd;
        // success = InitializeSecurityDescriptor(&sd, SECURITY_DESCRIPTOR_REVISION);
        // success = SetSecurityDescriptorDacl(&sd,
        //                                     TRUE, // bDaclPresent flag
        //                                     pAcl,
        //                                     FALSE);

        // SECURITY_ATTRIBUTES sa;
        // // Initialize a security attributes structure.
        // sa.nLength = sizeof(SECURITY_ATTRIBUTES);
        // sa.lpSecurityDescriptor = &sd;
        // sa.bInheritHandle = FALSE;
        // success;

        // wil::unique_hfile file{ CreateFileW(testPath.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr) };
        // THROW_LAST_ERROR_IF(!file);

        return *state;
    }

    void ElevatedState::FromJson(const Json::Value& root) const noexcept
    {
        auto state = _state.lock();
        // GetValueForKey() comes in two variants:
        // * take a std::optional<T> reference
        // * return std::optional<T> by value
        // At the time of writing the former version skips missing fields in the json,
        // but we want to explicitly clear state fields that were removed from state.json.
#define MTSM_ELEVATED_STATE_GEN(type, name, key, ...) state->name = JsonUtils::GetValueForKey<std::optional<type>>(root, key);
        MTSM_ELEVATED_STATE_FIELDS(MTSM_ELEVATED_STATE_GEN)
#undef MTSM_ELEVATED_STATE_GEN
    }
    Json::Value ElevatedState::ToJson() const noexcept
    {
        Json::Value root{ Json::objectValue };

        {
            auto state = _state.lock_shared();
#define MTSM_ELEVATED_STATE_GEN(type, name, key, ...) JsonUtils::SetValueForKey(root, key, state->name);
            MTSM_ELEVATED_STATE_FIELDS(MTSM_ELEVATED_STATE_GEN)
#undef MTSM_ELEVATED_STATE_GEN
        }
        return root;
    }

    // Generate all getter/setters
#define MTSM_ELEVATED_STATE_GEN(type, name, key, ...)    \
    type ElevatedState::name() const noexcept            \
    {                                                    \
        const auto state = _state.lock_shared();         \
        const auto& value = state->name;                 \
        return value ? *value : type{ __VA_ARGS__ };     \
    }                                                    \
                                                         \
    void ElevatedState::name(const type& value) noexcept \
    {                                                    \
        {                                                \
            auto state = _state.lock();                  \
            state->name.emplace(value);                  \
        }                                                \
                                                         \
        _throttler();                                    \
    }
    MTSM_ELEVATED_STATE_FIELDS(MTSM_ELEVATED_STATE_GEN)
#undef MTSM_ELEVATED_STATE_GEN

    // Serialized this ApplicationState (in `context`) into the state.json at _path.
    // * Errors are only logged.
    // * _state->_writeScheduled is set to false, signaling our
    //   setters that _synchronize() needs to be called again.
    void ElevatedState::_write() const noexcept
    try
    {
        Json::Value root{ this->ToJson() };

        Json::StreamWriterBuilder wbuilder;
        const auto content = Json::writeString(wbuilder, root);
        WriteUTF8FileAtomic(_path, content, true);
        // WriteUTF8File(_path, content, true);
    }
    CATCH_LOG()

}
