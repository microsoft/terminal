// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include <wil/token_helpers.h>
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
    // A case-insensitive wide-character map is used to store environment variables
    // due to documented requirements:
    //
    //      "All strings in the environment block must be sorted alphabetically by name.
    //      The sort is case-insensitive, Unicode order, without regard to locale.
    //      Because the equal sign is a separator, it must not be used in the name of
    //      an environment variable."
    //      https://docs.microsoft.com/en-us/windows/desktop/ProcThread/changing-environment-variables
    struct env_key_sorter
    {
        [[nodiscard]] bool operator()(const std::wstring& lhs, const std::wstring& rhs) const noexcept
        {
            return compare_ordinal_insensitive(lhs, rhs) < 0;
        }
    };

    namespace details
    {

        namespace vars
        {
            inline constexpr wil::zwstring_view system_root{ L"SystemRoot" };
            inline constexpr wil::zwstring_view system_drive{ L"SystemDrive" };
            inline constexpr wil::zwstring_view all_users_profile{ L"ALLUSERSPROFILE" };
            inline constexpr wil::zwstring_view public_var{ L"PUBLIC" };
            inline constexpr wil::zwstring_view program_data{ L"ProgramData" };
            inline constexpr wil::zwstring_view computer_name{ L"COMPUTERNAME" };
            inline constexpr wil::zwstring_view user_name{ L"USERNAME" };
            inline constexpr wil::zwstring_view user_domain{ L"USERDOMAIN" };
            inline constexpr wil::zwstring_view user_dns_domain{ L"USERDNSDOMAIN" };
            inline constexpr wil::zwstring_view home_drive{ L"HOMEDRIVE" };
            inline constexpr wil::zwstring_view home_share{ L"HOMESHARE" };
            inline constexpr wil::zwstring_view home_path{ L"HOMEPATH" };
            inline constexpr wil::zwstring_view user_profile{ L"USERPROFILE" };
            inline constexpr wil::zwstring_view app_data{ L"APPDATA" };
            inline constexpr wil::zwstring_view local_app_data{ L"LOCALAPPDATA" };

            inline constexpr wil::zwstring_view program_files{ L"ProgramFiles" };
            inline constexpr wil::zwstring_view program_files_x86{ L"ProgramFiles(x86)" };
            inline constexpr wil::zwstring_view program_files_arm64{ L"ProgramFiles(Arm)" };
            inline constexpr wil::zwstring_view program_w6432{ L"ProgramW6432" };
            inline constexpr wil::zwstring_view common_program_files{ L"CommonProgramFiles" };
            inline constexpr wil::zwstring_view common_program_files_x86{ L"CommonProgramFiles(x86)" };
            inline constexpr wil::zwstring_view common_program_files_arm64{ L"CommonProgramFiles(Arm)" };
            inline constexpr wil::zwstring_view common_program_w6432{ L"CommonProgramW6432" };

            const std::array program_files_map{
                std::pair{ L"ProgramFilesDir", program_files },
                std::pair{ L"CommonFilesDir", common_program_files },
#ifdef _WIN64
#ifdef _M_ARM64
                std::pair{ L"ProgramFilesDir (Arm)", program_files_arm64 },
                std::pair{ L"CommonFilesDir (Arm)", common_program_files_arm64 },
#endif
                std::pair{ L"ProgramFilesDir (x86)", program_files_x86 },
                std::pair{ L"CommonFilesDir (x86)", common_program_files_x86 },
                std::pair{ L"ProgramW6432Dir", program_w6432 },
                std::pair{ L"CommonW6432Dir", common_program_w6432 },
#endif
            };

            namespace reg
            {
                inline constexpr wil::zwstring_view program_files_root{ LR"(Software\Microsoft\Windows\CurrentVersion)" };
                inline constexpr wil::zwstring_view system_env_var_root{ LR"(SYSTEM\CurrentControlSet\Control\Session Manager\Environment)" };
                inline constexpr wil::zwstring_view user_env_var_root{ LR"(Environment)" };
                inline constexpr wil::zwstring_view user_volatile_env_var_root{ LR"(Volatile Environment)" };
                inline constexpr wil::zwstring_view user_volatile_session_env_var_root_pattern{ LR"(Volatile Environment\{0:d})" };
            };
        };

        namespace wil_env
        {
            /** Looks up a registry value from 'key' and fails if it is not found. */
            template<typename string_type, size_t initialBufferLength = 256>
            HRESULT RegQueryValueExW(HKEY key, PCWSTR valueName, string_type& result) WI_NOEXCEPT
            {
                return wil::AdaptFixedSizeToAllocatedResult<string_type, initialBufferLength>(result, [&](_Out_writes_(valueLength) PWSTR value, size_t valueLength, _Out_ size_t* valueLengthNeededWithNul) -> HRESULT {
                    auto length = gsl::narrow<DWORD>(valueLength * sizeof(wchar_t));
                    const auto status = ::RegQueryValueExW(key, valueName, nullptr, nullptr, reinterpret_cast<BYTE*>(value), &length);
                    // length will receive the number of bytes including trailing null byte. Convert to a number of wchar_t's.
                    // AdaptFixedSizeToAllocatedResult will then resize buffer to valueLengthNeededWithNull.
                    // We're rounding up to prevent infinite loops if the data isn't a REG_SZ and length isn't divisible by 2.
                    *valueLengthNeededWithNul = (length + sizeof(wchar_t) - 1) / sizeof(wchar_t);
                    return status == ERROR_MORE_DATA ? S_OK : HRESULT_FROM_WIN32(status);
                });
            }

            /** Looks up a registry value from 'key' and returns null if it is not found. */
            template<typename string_type, size_t initialBufferLength = 256>
            HRESULT TryRegQueryValueExW(HKEY key, PCWSTR valueName, string_type& result) WI_NOEXCEPT
            {
                const auto hr = wil_env::TryRegQueryValueExW<string_type, initialBufferLength>(key, valueName, result);
                RETURN_HR_IF(hr, FAILED(hr) && (hr != HRESULT_FROM_WIN32(ERROR_ENVVAR_NOT_FOUND)));
                return S_OK;
            }

#ifdef WIL_ENABLE_EXCEPTIONS
            /** Looks up a registry value from 'key' and fails if it is not found. */
            template<typename string_type = wil::unique_cotaskmem_string, size_t initialBufferLength = 256>
            string_type RegQueryValueExW(HKEY key, PCWSTR valueName)
            {
                string_type result;
                THROW_IF_FAILED((wil_env::RegQueryValueExW<string_type, initialBufferLength>(key, valueName, result)));
                return result;
            }

            /** Looks up a registry value from 'key' and returns null if it is not found. */
            template<typename string_type = wil::unique_cotaskmem_string, size_t initialBufferLength = 256>
            string_type TryRegQueryValueExW(HKEY key, PCWSTR valueName)
            {
                string_type result;
                THROW_IF_FAILED((wil_env::TryRegQueryValueExW<string_type, initialBufferLength>(key, valueName, result)));
                return result;
            }
#endif

            //! A strongly typed version of the Win32 API GetShortPathNameW.
            //! Return a path in an allocated buffer for handling long paths.
            template<typename string_type, size_t stackBufferLength = 256>
            HRESULT GetShortPathNameW(PCWSTR file, string_type& path)
            {
                const auto hr = wil::AdaptFixedSizeToAllocatedResult<string_type, stackBufferLength>(path, [&](_Out_writes_(valueLength) PWSTR value, size_t valueLength, _Out_ size_t* valueLengthNeededWithNull) -> HRESULT {
                    // Note that GetShortPathNameW() is not limited to MAX_PATH
                    // but it does take a fixed size buffer.
                    *valueLengthNeededWithNull = ::GetShortPathNameW(file, value, static_cast<DWORD>(valueLength));
                    RETURN_LAST_ERROR_IF(*valueLengthNeededWithNull == 0);
                    WI_ASSERT((*value != L'\0') == (*valueLengthNeededWithNull < valueLength));
                    if (*valueLengthNeededWithNull < valueLength)
                    {
                        (*valueLengthNeededWithNull)++; // it fit, account for the null
                    }
                    return S_OK;
                });
                return hr;
            }

#ifdef WIL_ENABLE_EXCEPTIONS
            //! A strongly typed version of the Win32 API GetShortPathNameW.
            //! Return a path in an allocated buffer for handling long paths.
            template<typename string_type = wil::unique_cotaskmem_string, size_t stackBufferLength = 256>
            string_type GetShortPathNameW(PCWSTR file)
            {
                string_type result;
                THROW_IF_FAILED((GetShortPathNameW<string_type, stackBufferLength>(file, result)));
                return result;
            }
#endif
        };
    };

    class env
    {
    private:
#ifdef UNIT_TESTING
        friend class ::EnvTests;
#endif

        std::map<std::wstring, std::wstring, til::env_key_sorter> _envMap{};

        // We make copies of the environment variable names to ensure they are null terminated.
        void get(wil::zwstring_view variable)
        {
            if (auto value = wil::TryGetEnvironmentVariableW<std::wstring>(variable.c_str()); !value.empty())
            {
                save_to_map(std::wstring{ variable }, std::move(value));
            }
        }

        void get_program_files()
        {
            wil::unique_hkey key;
            if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, til::details::vars::reg::program_files_root.data(), 0, KEY_READ, &key) == ERROR_SUCCESS)
            {
                for (auto& [keyName, varName] : til::details::vars::program_files_map)
                {
                    auto value = til::details::wil_env::RegQueryValueExW<std::wstring, 256>(key.get(), keyName);
                    set_user_environment_var(varName, value);
                }
            }
        }

        void get_vars_from_registry(HKEY rootKey, wil::zwstring_view subkey)
        {
            wil::unique_hkey key;
            if (RegOpenKeyExW(rootKey, subkey.c_str(), 0, KEY_READ, &key) == ERROR_SUCCESS)
            {
                DWORD maxValueNameSize = 0, maxValueDataSize = 0;
                if (RegQueryInfoKeyW(key.get(), nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, &maxValueNameSize, &maxValueDataSize, nullptr, nullptr) == ERROR_SUCCESS)
                {
                    maxValueNameSize++; // the query does not include the terminating null, but needs that space to enum
                    std::wstring valueName;
                    std::basic_string<BYTE> valueData;
                    valueName.resize(maxValueNameSize);
                    valueData.resize(maxValueDataSize);

                    for (DWORD pass = 0; pass < 2; ++pass)
                    {
                        DWORD valueNameSize = maxValueNameSize;
                        DWORD valueDataSize = maxValueDataSize;

                        DWORD index = 0;
                        DWORD type = 0;

                        while (true)
                        {
                            const DWORD result = RegEnumValueW(key.get(), index, valueName.data(), &valueNameSize, nullptr, &type, valueData.data(), &valueDataSize);
                            if (result != ERROR_SUCCESS)
                            {
                                break;
                            }
                            valueName.resize(valueNameSize);
                            valueData.resize(valueDataSize);

                            if (valueNameSize)
                            {
                                const bool isPathVar = is_path_var(valueName);

                                // On some systems we've seen path variables that are REG_SZ instead
                                // of REG_EXPAND_SZ. We should always treat them as REG_EXPAND_SZ.
                                if (isPathVar && type == REG_SZ)
                                {
                                    type = REG_EXPAND_SZ;
                                }

                                std::wstring data;
                                if (pass == 0 && (type == REG_SZ) && valueDataSize >= sizeof(wchar_t))
                                {
                                    const auto p = reinterpret_cast<const wchar_t*>(valueData.data());
                                    const auto l = wcsnlen(p, valueData.size() / sizeof(wchar_t));
                                    data.assign(p, l);
                                }
                                else if (pass == 1 && (type == REG_EXPAND_SZ) && valueDataSize >= sizeof(wchar_t))
                                {
                                    const auto p = reinterpret_cast<const wchar_t*>(valueData.data());
                                    const auto l = wcsnlen(p, valueData.size() / sizeof(wchar_t));
                                    data.assign(p, l);
                                    data = expand_environment_strings(data.data());
                                }

                                if (!data.empty())
                                {
                                    if (isPathVar)
                                    {
                                        concat_var(valueName, std::move(data));
                                    }
                                    else
                                    {
                                        set_user_environment_var(valueName, std::move(data));
                                    }
                                }
                            }

                            valueName.resize(maxValueNameSize);
                            valueData.resize(maxValueDataSize);
                            valueNameSize = maxValueNameSize;
                            valueDataSize = maxValueDataSize;
                            index++;
                        }
                        index = 0;
                    }
                }
            }
        }

        std::wstring expand_environment_strings(std::wstring_view input)
        {
            std::wstring expanded;
            expanded.reserve(input.size());
            bool isInEnvVarName = false;
            std::wstring currentEnvVarName;
            for (const auto character : input)
            {
                if (character == L'%')
                {
                    if (isInEnvVarName)
                    {
                        if (const auto envVarValue = _envMap.find(currentEnvVarName); envVarValue != _envMap.end())
                        {
                            expanded.append(envVarValue->second);
                        }
                        else
                        {
                            expanded.push_back(L'%');
                            expanded.append(currentEnvVarName);
                            expanded.push_back(L'%');
                        }
                        isInEnvVarName = false;
                        currentEnvVarName.clear();
                    }
                    else
                    {
                        isInEnvVarName = true;
                    }
                }
                else
                {
                    if (isInEnvVarName)
                    {
                        currentEnvVarName.push_back(character);
                    }
                    else
                    {
                        expanded.push_back(character);
                    }
                }
            }
            if (isInEnvVarName)
            {
                expanded.push_back('%');
                expanded.append(currentEnvVarName);
            }
            return expanded;
        }

        void concat_var(std::wstring var, std::wstring value)
        {
            if (const auto existing = _envMap.find(var); existing != _envMap.end())
            {
                // If it doesn't already trail with a ;, add one.
                // Otherwise, just take advantage of the one it has.
                if (existing->second.back() != L';')
                {
                    existing->second.push_back(L';');
                }
                existing->second.append(value);
            }
            else
            {
                save_to_map(std::move(var), std::move(value));
            }
        }

        void save_to_map(std::wstring var, std::wstring value)
        {
            if (!var.empty() && !value.empty())
            {
                _envMap.insert_or_assign(std::move(var), std::move(value));
            }
        }

        std::wstring check_for_temp(std::wstring_view var, std::wstring_view value)
        {
            static constexpr std::wstring_view temp{ L"temp" };
            static constexpr std::wstring_view tmp{ L"tmp" };
            if (til::compare_ordinal_insensitive(var, temp) == 0 ||
                til::compare_ordinal_insensitive(var, tmp) == 0)
            {
                return til::details::wil_env::GetShortPathNameW<std::wstring, 256>(value.data());
            }
            else
            {
                return std::wstring{ value };
            }
        }

        static bool is_path_var(std::wstring_view input) noexcept
        {
            static constexpr std::wstring_view path{ L"Path" };
            static constexpr std::wstring_view libPath{ L"LibPath" };
            static constexpr std::wstring_view os2LibPath{ L"Os2LibPath" };
            return til::compare_ordinal_insensitive(input, path) == 0 ||
                   til::compare_ordinal_insensitive(input, libPath) == 0 ||
                   til::compare_ordinal_insensitive(input, os2LibPath) == 0;
        }

        void parse(const wchar_t* lastCh)
        {
            for (; *lastCh != '\0'; ++lastCh)
            {
                // Copy current entry into temporary map.
                const size_t cchEntry{ ::wcslen(lastCh) };
                const std::wstring_view entry{ lastCh, cchEntry };

                // Every entry is of the form "name=value\0".
                const auto pos = entry.find_first_of(L"=", 0, 1);
                THROW_HR_IF(E_UNEXPECTED, pos == std::wstring::npos);

                std::wstring name{ entry.substr(0, pos) }; // portion before '='
                std::wstring value{ entry.substr(pos + 1) }; // portion after '='

                // Don't replace entries that already exist.
                _envMap.try_emplace(std::move(name), std::move(value));
                lastCh += cchEntry;
            }
        }

    public:
        env() = default;

        env(const wchar_t* block)
        {
            parse(block);
        }

        // Function Description:
        // - Creates a new environment with the current process's unicode environment
        //   variables.
        // Return Value:
        // - A new environment
        static til::env from_current_environment()
        {
            LPWCH currentEnvVars{};
            auto freeCurrentEnv = wil::scope_exit([&] {
                if (currentEnvVars)
                {
                    FreeEnvironmentStringsW(currentEnvVars);
                    currentEnvVars = nullptr;
                }
            });

            currentEnvVars = ::GetEnvironmentStringsW();
            THROW_HR_IF_NULL(E_OUTOFMEMORY, currentEnvVars);

            return til::env{ currentEnvVars };
        }

        void set_user_environment_var(std::wstring_view var, std::wstring_view value)
        {
            auto valueString = expand_environment_strings(value);
            valueString = check_for_temp(var, valueString);
            save_to_map(std::wstring{ var }, std::move(valueString));
        }

        void regenerate()
        {
            // Generally replicates the behavior of shell32!RegenerateUserEnvironment
            // The difference is that we don't
            // * handle autoexec.bat (intentionally unsupported).
            // * call LookupAccountSidW to get the USERNAME/USERDOMAIN variables.
            //   Windows sets these as values of the "Volatile Environment" key at each login.
            //   We don't expect our process to impersonate another user so we can get them from the PEB.
            // * call GetComputerNameW to get the COMPUTERNAME variable, for the same reason.
            get(til::details::vars::system_root);
            get(til::details::vars::system_drive);
            get(til::details::vars::all_users_profile);
            get(til::details::vars::public_var);
            get(til::details::vars::program_data);
            get(til::details::vars::computer_name);
            get(til::details::vars::user_name);
            get(til::details::vars::user_domain);
            get(til::details::vars::user_dns_domain);
            get(til::details::vars::home_drive);
            get(til::details::vars::home_share);
            get(til::details::vars::home_path);
            get(til::details::vars::user_profile);
            get(til::details::vars::app_data);
            get(til::details::vars::local_app_data);
            get_program_files();
            get_vars_from_registry(HKEY_LOCAL_MACHINE, til::details::vars::reg::system_env_var_root);
            // not processing autoexec.bat
            get_vars_from_registry(HKEY_CURRENT_USER, til::details::vars::reg::user_env_var_root);
            get_vars_from_registry(HKEY_CURRENT_USER, til::details::vars::reg::user_volatile_env_var_root);
            get_vars_from_registry(HKEY_CURRENT_USER, fmt::format(til::details::vars::reg::user_volatile_session_env_var_root_pattern, NtCurrentTeb()->ProcessEnvironmentBlock->SessionId));
        }

        std::wstring to_string() const
        {
            std::wstring result;
            for (const auto& [k, v] : _envMap)
            {
                result.append(k);
                result.push_back(L'=');
                result.append(v);
                result.push_back(L'\0');
            }
            result.push_back(L'\0');
            return result;
        }

        auto& as_map() noexcept
        {
            return _envMap;
        }
    };
};

#pragma warning(pop)
