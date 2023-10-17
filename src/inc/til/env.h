// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include <wil/token_helpers.h>
#include <wil/registry.h>
#include <winternl.h>
#include <til/string.h>

#pragma warning(push)
#pragma warning(disable : 26429) // Symbol '...' is never tested for nullness, it can be marked as not_null (f.23).
#pragma warning(disable : 26472) // Don't use a static_cast for arithmetic conversions. Use brace initialization, gsl::narrow_cast or gsl::narrow (type.1).
#pragma warning(disable : 26481) // Don't use pointer arithmetic. Use span instead (bounds.1).
#pragma warning(disable : 26490) // Don't use reinterpret_cast (type.1).

#ifdef UNIT_TESTING
class EnvTests;
#endif

namespace til // Terminal Implementation Library. Also: "Today I Learned"
{
    struct env
    {
#ifdef UNIT_TESTING
        friend class ::EnvTests;
#endif

        env() = default;

        env(const wchar_t* block)
        {
            for (auto lastCh = block; *lastCh; ++lastCh)
            {
                // Copy current entry into temporary map.
                const size_t cchEntry{ ::wcslen(lastCh) };
                const std::wstring_view entry{ lastCh, cchEntry };

                // Every entry is of the form "name=value\0".
                const auto pos = entry.find(L'=');
                THROW_HR_IF(E_UNEXPECTED, pos == decltype(entry)::npos);

                std::wstring name{ entry.substr(0, pos) }; // portion before '='
                std::wstring value{ entry.substr(pos + 1) }; // portion after '='

                // Don't replace entries that already exist.
                _envMap.try_emplace(std::move(name), std::move(value));
                lastCh += cchEntry;
            }
        }

        // Function Description:
        // - Creates a new environment with the current process's unicode environment
        //   variables.
        // Return Value:
        // - A new environment
        static env from_current_environment()
        {
            const auto currentEnvVars = ::GetEnvironmentStringsW();
            THROW_HR_IF_NULL(E_OUTOFMEMORY, currentEnvVars);

            const auto cleanup = wil::scope_exit([&] {
                FreeEnvironmentStringsW(currentEnvVars);
            });

            return env{ currentEnvVars };
        }

        void set_user_environment_var(std::wstring_view var, std::wstring_view value)
        {
            save_to_map(std::wstring{ var }, check_for_temp(var, expand_environment_strings(value)));
        }

        // Generally replicates the behavior of shell32!RegenerateUserEnvironment, excluding the autoexec.bat handling.
        void regenerate()
        {
            static constexpr const wchar_t* environmentKeys[]{
                L"ALLUSERSPROFILE",
                L"APPDATA",
                L"HOMEDRIVE",
                L"HOMEPATH",
                L"HOMESHARE",
                L"LOCALAPPDATA",
                L"ProgramData",
                L"PUBLIC",
                L"SystemDrive",
                L"SystemRoot",
                L"USERDNSDOMAIN",
                L"USERPROFILE",
            };

            for (const auto key : environmentKeys)
            {
                get(key);
            }

            get_user_name_and_domain();
            get_program_files();

            get_vars_from_registry(HKEY_LOCAL_MACHINE, LR"(SYSTEM\CurrentControlSet\Control\Session Manager\Environment)");
            get_vars_from_registry(HKEY_CURRENT_USER, LR"(Environment)");
            get_vars_from_registry(HKEY_CURRENT_USER, LR"(Volatile Environment)");
            {
                wchar_t key[32];
                swprintf_s(key, LR"(Volatile Environment\%lu)", NtCurrentTeb()->ProcessEnvironmentBlock->SessionId);
                get_vars_from_registry(HKEY_CURRENT_USER, &key[0]);
            }

            for (auto& [k, v] : _envMap)
            {
                if (v.postProcessingRequired)
                {
                    v.value = expand_environment_strings(v.value);
                    v.postProcessingRequired = false;
                }
                v.value = check_for_temp(k, std::move(v.value));
            }
        }

        std::wstring to_string() const
        {
            std::wstring result;
            for (const auto& [k, v] : _envMap)
            {
                result.append(k);
                result.push_back(L'=');
                result.append(v.value);
                result.push_back(L'\0');
            }
            return result;
        }

        auto& as_map() noexcept
        {
            return _envMap;
        }

    private:
        void get(const wchar_t* key)
        {
            wchar_t buf[maxLargeValueLen];
            const auto len = GetEnvironmentVariableW(key, &buf[0], maxLargeValueLen);

            if (len > 0 && len < maxLargeValueLen)
            {
                save_to_map(std::wstring{ key }, std::wstring{ &buf[0], len });
            }
        }

        void get_computer_name()
        {
            wchar_t name[MAX_COMPUTERNAME_LENGTH + 1];
            DWORD size = _countof(name);
            if (GetComputerNameW(&name[0], &size))
            {
                save_to_map(L"COMPUTERNAME", std::wstring{ &name[0], size });
            }
        }

        void get_user_name_and_domain()
        try
        {
            const auto token = wil::open_current_access_token();
            const auto user = wil::get_token_information<TOKEN_USER>(token.get());

            DWORD accountNameSize = 0;
            DWORD userDomainSize = 0;
            SID_NAME_USE sidNameUse;
            if (LookupAccountSidW(nullptr, user.get()->User.Sid, nullptr, &accountNameSize, nullptr, &userDomainSize, &sidNameUse) || GetLastError() != ERROR_INSUFFICIENT_BUFFER)
            {
                return;
            }

            std::wstring accountName;
            std::wstring userDomain;
            accountName.resize(accountNameSize);
            userDomain.resize(userDomainSize);

            if (!LookupAccountSidW(nullptr, user.get()->User.Sid, accountName.data(), &accountNameSize, userDomain.data(), &userDomainSize, &sidNameUse))
            {
                return;
            }

            accountName.resize(accountNameSize);
            userDomain.resize(userDomainSize);

            save_to_map(L"USERNAME", std::move(accountName));
            save_to_map(L"USERDOMAIN", std::move(userDomain));
        }
        CATCH_LOG()

        void get_program_files()
        {
            static constexpr std::array program_files_map{
                std::pair{ L"ProgramFilesDir", L"ProgramFiles" },
                std::pair{ L"CommonFilesDir", L"CommonProgramFiles" },
#ifdef _WIN64
#ifdef _M_ARM64
                std::pair{ L"ProgramFilesDir (Arm)", L"ProgramFiles(Arm)" },
                std::pair{ L"CommonFilesDir (Arm)", L"CommonProgramFiles(Arm)" },
#endif
                std::pair{ L"ProgramFilesDir (x86)", L"ProgramFiles(x86)" },
                std::pair{ L"CommonFilesDir (x86)", L"CommonProgramFiles(x86)" },
                std::pair{ L"ProgramW6432Dir", L"ProgramW6432" },
                std::pair{ L"CommonW6432Dir", L"CommonProgramW6432" },
#endif
            };

            wil::unique_hkey key;
            if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, LR"(Software\Microsoft\Windows\CurrentVersion)", 0, KEY_READ, key.addressof()) != ERROR_SUCCESS)
            {
                return;
            }

            wchar_t valueBuf[maxLargeValueLen];

            for (auto& [keyName, varName] : program_files_map)
            {
                DWORD valueLenBytes = sizeof(valueBuf);
                DWORD type = 0;
                if (RegQueryValueExW(
                        /* hKey        */ key.get(),
                        /* lpValueName */ keyName,
                        /* lpReserved  */ nullptr,
                        /* lpType      */ &type,
                        /* lpData      */ reinterpret_cast<BYTE*>(&valueBuf[0]),
                        /* lpcbData    */ &valueLenBytes) != ERROR_SUCCESS)
                {
                    continue;
                }

                if (type != REG_SZ && type != REG_EXPAND_SZ)
                {
                    continue;
                }

                const auto valueLen = wcsnlen(&valueBuf[0], valueLenBytes / sizeof(wchar_t));
                if (!valueLen)
                {
                    continue;
                }

                set_user_environment_var(varName, { &valueBuf[0], valueLen });
            }
        }

        void get_vars_from_registry(HKEY rootKey, const wchar_t* subkey)
        {
            wil::unique_hkey key;
            if (RegOpenKeyExW(rootKey, subkey, 0, KEY_READ, key.addressof()) != ERROR_SUCCESS)
            {
                return;
            }

            wchar_t nameBuf[MAX_PATH];
            wchar_t valueBuf[maxLargeValueLen];

            for (DWORD i = 0;; ++i)
            {
                DWORD nameLen = _countof(nameBuf);
                DWORD valueLenBytes = sizeof(valueBuf);
                DWORD type = 0;
                const auto status = RegEnumValueW(key.get(), i, &nameBuf[0], &nameLen, nullptr, &type, reinterpret_cast<BYTE*>(&valueBuf[0]), &valueLenBytes);
                if (status == ERROR_NO_MORE_ITEMS)
                {
                    break;
                }
                if (status != ERROR_SUCCESS || (type != REG_SZ && type != REG_EXPAND_SZ) || !nameLen || !valueLenBytes)
                {
                    continue;
                }

                const auto valueLen = wcsnlen(&valueBuf[0], valueLenBytes / sizeof(wchar_t));
                if (!valueLen)
                {
                    continue;
                }

                const std::wstring_view name{ &nameBuf[0], nameLen };
                const std::wstring_view value{ &valueBuf[0], valueLen };
                const auto it = _envMap.emplace(std::piecewise_construct, std::forward_as_tuple(name), std::forward_as_tuple()).first;

                if (til::compare_string_ordinal(name, L"Path") == CSTR_EQUAL ||
                    til::compare_string_ordinal(name, L"LibPath") == CSTR_EQUAL ||
                    til::compare_string_ordinal(name, L"Os2LibPath") == CSTR_EQUAL)
                {
                    if (!it->second.value.ends_with(L';'))
                    {
                        it->second.value.push_back(L';');
                    }
                    it->second.value.append(value);
                    // On some systems we've seen path variables that are REG_SZ instead
                    // of REG_EXPAND_SZ. We should always treat them as REG_EXPAND_SZ.
                    it->second.postProcessingRequired = true;
                }
                else
                {
                    it->second.value.assign(value);
                    it->second.postProcessingRequired = type == REG_EXPAND_SZ;
                }
            }
        }

        static std::wstring check_for_temp(std::wstring_view var, std::wstring value)
        {
            if (til::compare_string_ordinal(var, L"temp") == CSTR_EQUAL ||
                til::compare_string_ordinal(var, L"tmp") == CSTR_EQUAL)
            {
                const auto inLen = gsl::narrow<DWORD>(value.size());
                // GetShortPathNameW() allows the source and destination parameters to overlap.
                // It'll allocate an internal buffer for you to avoid scribbling over the given string in case of an error.
                const auto outLen = GetShortPathNameW(value.c_str(), value.data(), inLen);
                if (inLen > 0 && outLen < inLen)
                {
                    value.resize(outLen);
                }
            }

            return value;
        }

        std::wstring expand_environment_strings(std::wstring_view input)
        {
            std::wstring expanded;
            size_t lastPercentPosition = 0;
            bool isInEnvVarName = false;

            expanded.reserve(input.size());

            for (;;)
            {
                const auto idx = input.find(L'%', lastPercentPosition + 1);
                if (idx == decltype(input)::npos)
                {
                    break;
                }

                if (isInEnvVarName)
                {
                    const auto nameAndQuotes = input.substr(lastPercentPosition, idx - lastPercentPosition);
                    const auto name = input.substr(lastPercentPosition + 1, idx - lastPercentPosition - 2);
                    const auto it = _envMap.find(name);
                    const auto replacementAvailable = it != _envMap.end() && !it->second.postProcessingRequired;
                    const auto replacement = replacementAvailable ? it->second.value : nameAndQuotes;
                    expanded.append(replacement);
                }

                lastPercentPosition = idx;
                isInEnvVarName = !isInEnvVarName;
            }

            if (isInEnvVarName)
            {
                expanded.append(input.substr(lastPercentPosition));
            }

            return expanded;
        }

        void save_to_map(std::wstring_view var, std::wstring value)
        {
            if (!var.empty() && !value.empty())
            {
                const auto it = _envMap.emplace(std::piecewise_construct, std::forward_as_tuple(var), std::forward_as_tuple()).first;
                it->second.value = std::move(value);
            }
        }

        // The original shell32!RegenerateUserEnvironment makes a fairly conscious effort to
        // restrict the length of environment variables to 4096 characters. It's unknown why
        // but it probably doesn't hurt keeping this restriction until we know why it exists.
        static constexpr DWORD maxLargeValueLen = 4096;

        struct EnvMapEntry
        {
            std::wstring value;
            bool postProcessingRequired = false;
        };

        std::map<std::wstring, EnvMapEntry, til::wstring_case_insensitive_compare> _envMap;
    };
};

#pragma warning(pop)
