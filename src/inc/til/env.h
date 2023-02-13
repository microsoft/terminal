// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include <wil/token_helpers.h>
#include <winternl.h>

namespace til // Terminal Implementation Library. Also: "Today I Learned"
{
    namespace details
    {
        //
        // A case-insensitive wide-character map is used to store environment variables
        // due to documented requirements:
        //
        //      "All strings in the environment block must be sorted alphabetically by name.
        //      The sort is case-insensitive, Unicode order, without regard to locale.
        //      Because the equal sign is a separator, it must not be used in the name of
        //      an environment variable."
        //      https://docs.microsoft.com/en-us/windows/desktop/ProcThread/changing-environment-variables
        //
        struct wstring_case_insensitive_compare
        {
            [[nodiscard]] bool operator()(const std::wstring& lhs, const std::wstring& rhs) const noexcept
            {
                return (::_wcsicmp(lhs.c_str(), rhs.c_str()) < 0);
            }
        };

        namespace vars
        {
            static constexpr std::wstring_view system_root{ L"SystemRoot" };
            static constexpr std::wstring_view system_drive{ L"SystemDrive" };
            static constexpr std::wstring_view all_users_profile{ L"ALLUSERSPROFILE" };
            static constexpr std::wstring_view public_var{ L"PUBLIC" };
            static constexpr std::wstring_view program_data{ L"ProgramData" };
            static constexpr std::wstring_view computer_name{ L"COMPUTERNAME" };
            static constexpr std::wstring_view user_name{ L"USERNAME" };
            static constexpr std::wstring_view user_domain{ L"USERDOMAIN" };
            static constexpr std::wstring_view user_dns_domain{ L"USERDNSDOMAIN" };
            static constexpr std::wstring_view home_drive{ L"HOMEDRIVE" };
            static constexpr std::wstring_view home_share{ L"HOMESHARE" };
            static constexpr std::wstring_view home_path{ L"HOMEPATH" };
            static constexpr std::wstring_view user_profile{ L"USERPROFILE" };
            static constexpr std::wstring_view app_data{ L"APPDATA" };
            static constexpr std::wstring_view local_app_data{ L"LOCALAPPDATA" };

            static constexpr std::wstring_view program_files{ L"ProgramFiles" };
            static constexpr std::wstring_view program_files_x86{ L"ProgramFiles(x86)" };
            static constexpr std::wstring_view program_files_arm64{ L"ProgramFiles(Arm)" };
            static constexpr std::wstring_view program_w6432{ L"ProgramW6432" };
            static constexpr std::wstring_view common_program_files{ L"CommonProgramFiles" };
            static constexpr std::wstring_view common_program_files_x86{ L"CommonProgramFiles(x86)" };
            static constexpr std::wstring_view common_program_files_arm64{ L"CommonProgramFiles(Arm)" };
            static constexpr std::wstring_view common_program_w6432{ L"CommonProgramW6432" };

            const std::map<std::wstring, std::wstring_view> program_files_map{
                { L"ProgramFilesDir", program_files },
                { L"CommonFilesDir", common_program_files },
#ifdef _WIN64
#ifdef _M_ARM64
                { L"ProgramFilesDir (Arm)", program_files_arm64 },
                { L"CommonFilesDir (Arm)", common_program_files_arm64 },
#endif
                { L"ProgramFilesDir (x86)", program_files_x86 },
                { L"CommonFilesDir (x86)", common_program_files_x86 },
                { L"ProgramW6432Dir", program_w6432 },
                { L"CommonW6432Dir", common_program_w6432 },
#endif
            };

            namespace reg
            {
                static constexpr std::wstring_view program_files_root{ LR"(Software\Microsoft\Windows\CurrentVersion)" };
                static constexpr std::wstring_view system_env_var_root{ LR"(SYSTEM\CurrentControlSet\Control\Session Manager\Environment)" };
                static constexpr std::wstring_view user_env_var_root{ LR"(Environment)" };
                static constexpr std::wstring_view user_volatile_env_var_root{ LR"(Volatile Environment)" };
                static constexpr std::wstring_view user_volatile_session_env_var_root_pattern{ LR"(Volatile Environment\{0:d})" };
            };
        };

        namespace wil_env
        {
            /** Looks up the computer name and fails if it is not found. */
            template<typename string_type, size_t initialBufferLength = MAX_COMPUTERNAME_LENGTH + 1>
            inline HRESULT GetComputerNameW(string_type& result) WI_NOEXCEPT
            {
                return wil::AdaptFixedSizeToAllocatedResult<string_type, initialBufferLength>(result,
                                                                                              [&](_Out_writes_(valueLength) PWSTR value, size_t valueLength, _Out_ size_t* valueLengthNeededWithNul) -> HRESULT {
                                                                                                  // If the function succeeds, the return value is the number of characters stored in the buffer
                                                                                                  // pointed to by lpBuffer, not including the terminating null character.
                                                                                                  //
                                                                                                  // If lpBuffer is not large enough to hold the data, the return value is the buffer size, in
                                                                                                  // characters, required to hold the string and its terminating null character and the contents of
                                                                                                  // lpBuffer are undefined.
                                                                                                  //
                                                                                                  // If the function fails, the return value is zero. If the specified environment variable was not
                                                                                                  // found in the environment block, GetLastError returns ERROR_ENVVAR_NOT_FOUND.

                                                                                                  ::SetLastError(ERROR_SUCCESS);

                                                                                                  DWORD length = static_cast<DWORD>(valueLength);

                                                                                                  auto result = ::GetComputerNameW(value, &length);
                                                                                                  *valueLengthNeededWithNul = length;
                                                                                                  RETURN_IF_WIN32_BOOL_FALSE_EXPECTED(result);
                                                                                                  if (*valueLengthNeededWithNul < valueLength)
                                                                                                  {
                                                                                                      (*valueLengthNeededWithNul)++; // It fit, account for the null.
                                                                                                  }
                                                                                                  return S_OK;
                                                                                              });
            }

            /** Looks up the computer name and returns null if it is not found. */
            template<typename string_type, size_t initialBufferLength = MAX_COMPUTERNAME_LENGTH + 1>
            HRESULT TryGetComputerNameW(string_type& result) WI_NOEXCEPT
            {
                const auto hr = wil_env::GetComputerNameW<string_type, initialBufferLength>(result);
                RETURN_HR_IF(hr, FAILED(hr) && (hr != HRESULT_FROM_WIN32(ERROR_ENVVAR_NOT_FOUND)));
                return S_OK;
            }

#ifdef WIL_ENABLE_EXCEPTIONS
            /** Looks up the computer name and fails if it is not found. */
            template<typename string_type = wil::unique_cotaskmem_string, size_t initialBufferLength = MAX_COMPUTERNAME_LENGTH + 1>
            string_type GetComputerNameW()
            {
                string_type result;
                THROW_IF_FAILED((wil_env::GetComputerNameW<string_type, initialBufferLength>(result)));
                return result;
            }

            /** Looks up the computer name and returns null if it is not found. */
            template<typename string_type = wil::unique_cotaskmem_string, size_t initialBufferLength = MAX_COMPUTERNAME_LENGTH + 1>
            string_type TryGetComputerNameW()
            {
                string_type result;
                THROW_IF_FAILED((wil_env::TryGetComputerNameW<string_type, initialBufferLength>(result)));
                return result;
            }
#endif

            /** Looks up a registry value from 'key' and fails if it is not found. */
            template<typename string_type, size_t initialBufferLength = 256>
            inline HRESULT RegQueryValueExW(HKEY key, PCWSTR valueName, string_type& result) WI_NOEXCEPT
            {
                return wil::AdaptFixedSizeToAllocatedResult<string_type, initialBufferLength>(result,
                                                                                              [&](_Out_writes_(valueLength) PWSTR value, size_t valueLength, _Out_ size_t* valueLengthNeededWithNul) -> HRESULT {
                                                                                                  auto length = gsl::narrow<DWORD>(valueLength * sizeof(wchar_t));
                                                                                                  const auto status = ::RegQueryValueExW(key, valueName, 0, nullptr, reinterpret_cast<BYTE*>(value), &length);
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
                const auto hr = wil::AdaptFixedSizeToAllocatedResult<string_type, stackBufferLength>(path,
                                                                                                     [&](_Out_writes_(valueLength) PWSTR value, size_t valueLength, _Out_ size_t* valueLengthNeededWithNull) -> HRESULT {
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
        friend class EnvTests;
#endif

        std::map<std::wstring, std::wstring, til::details::wstring_case_insensitive_compare> _envMap{};

        // these wstring_views better be null terminated.
        void get(std::wstring variable)
        {
            if (auto value = wil::TryGetEnvironmentVariableW(variable.c_str()))
            {
                save_to_map(variable, value.get());
            }
        }

        void get_computer_name()
        {
            if (auto value = til::details::wil_env::TryGetComputerNameW())
            {
                save_to_map(std::wstring{ til::details::vars::computer_name }, value.get());
            }
        }

        void get_user_name_and_domain()
        try
        {
            auto token = wil::open_current_access_token();
            auto user = wil::get_token_information<TOKEN_USER>(token.get());

            DWORD accountNameSize = 0, userDomainSize = 0;
            SID_NAME_USE sidNameUse;
            SetLastError(ERROR_SUCCESS);
            if (LookupAccountSidW(nullptr, user.get()->User.Sid, nullptr, &accountNameSize, nullptr, &userDomainSize, &sidNameUse) || GetLastError() == ERROR_INSUFFICIENT_BUFFER)
            {
                std::wstring accountName, userDomain;
                accountName.resize(accountNameSize);
                userDomain.resize(userDomainSize);

                SetLastError(ERROR_SUCCESS);
                if (LookupAccountSidW(nullptr, user.get()->User.Sid, accountName.data(), &accountNameSize, userDomain.data(), &userDomainSize, &sidNameUse))
                {
                    save_to_map(std::wstring{ til::details::vars::user_name }, accountName);
                    save_to_map(std::wstring{ til::details::vars::user_domain }, userDomain);
                }
            }
        }
        CATCH_LOG()

        void get_program_files()
        {
            wil::unique_hkey key;
            if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, til::details::vars::reg::program_files_root.data(), 0, KEY_READ, &key) == ERROR_SUCCESS)
            {
                for (auto& [keyName, varName] : til::details::vars::program_files_map)
                {
                    auto value = til::details::wil_env::RegQueryValueExW<std::wstring, 256>(key.get(), keyName.c_str());
                    set_user_environment_var(std::wstring{ varName }, value);
                }
            }
        }

        void get_vars_from_registry(HKEY rootKey, std::wstring_view subkey)
        {
            wil::unique_hkey key;
            if (RegOpenKeyExW(rootKey, subkey.data(), 0, KEY_READ, &key) == ERROR_SUCCESS)
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
                            DWORD result = RegEnumValueW(key.get(), index, valueName.data(), &valueNameSize, nullptr, &type, valueData.data(), &valueDataSize);
                            if (result != ERROR_SUCCESS)
                            {
                                break;
                            }
                            valueName.resize(valueNameSize);
                            valueData.resize(valueDataSize);

                            if (valueNameSize)
                            {
                                std::wstring data;
                                if (pass == 0 && (type == REG_SZ) && valueDataSize >= sizeof(wchar_t))
                                {
                                    data = {
                                        reinterpret_cast<wchar_t*>(valueData.data()), valueData.size() / sizeof(wchar_t)
                                    };
                                }
                                else if (pass == 1 && (type == REG_EXPAND_SZ) && valueDataSize >= sizeof(wchar_t))
                                {
                                    data = {
                                        reinterpret_cast<wchar_t*>(valueData.data()), valueData.size() / sizeof(wchar_t)
                                    };
                                    data = expand_environment_strings(data.data());
                                }

                                // Because Registry data may or may not be null terminated... check if we've managed
                                // to store an extra null in the wstring by telling it to create itself from pointer and size.
                                // If we did, pull it off.
                                if (!data.empty())
                                {
                                    if (data.back() == L'\0')
                                    {
                                        data = data.substr(0, data.size() - 1);
                                    }
                                }

                                if (!data.empty())
                                {
                                    if (is_path_var(valueName))
                                    {
                                        concat_var(valueName, std::wstring{ data });
                                    }
                                    else
                                    {
                                        set_user_environment_var(valueName, std::wstring{ data });
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

        std::wstring expand_environment_strings(std::wstring input)
        {
            // TODO: this should be replacing from ourselves, not from the OS
            return wil::ExpandEnvironmentStringsW<std::wstring, 256>(input.data());
        }

        void set_user_environment_var(std::wstring var, std::wstring value)
        {
            value = expand_environment_strings(value);
            value = check_for_temp(value);
            save_to_map(var, value);
        }

        void concat_var(std::wstring var, std::wstring value)
        {
            if (const auto existing = _envMap.find(var); existing != _envMap.end())
            {
                // If it doesn't already trail with a ;, add one.
                // Otherwise, just take advantage of the one it has.
                if (existing->second.back() != L';')
                {
                    existing->second.append(L";");
                }
                existing->second.append(value);
                save_to_map(var, existing->second);
            }
            else
            {
                save_to_map(var, value);
            }
        }

        void save_to_map(std::wstring var, std::wstring value)
        {
            if (!var.empty() && !value.empty())
            {
                _envMap.insert_or_assign(var, value);
            }
        }

        static constexpr std::wstring_view temp{ L"temp" };
        static constexpr std::wstring_view tmp{ L"tmp" };
        std::wstring check_for_temp(std::wstring_view input)
        {
            if (!_wcsicmp(input.data(), temp.data()) ||
                !_wcsicmp(input.data(), tmp.data()))
            {
                return til::details::wil_env::GetShortPathNameW<std::wstring, 256>(input.data());
            }
            else
            {
                return std::wstring{ input };
            }
        }

        static constexpr std::wstring_view path{ L"Path" };
        static constexpr std::wstring_view libPath{ L"LibPath" };
        static constexpr std::wstring_view os2LibPath{ L"Os2LibPath" };
        bool is_path_var(std::wstring_view input)
        {
            return !_wcsicmp(input.data(), path.data()) || !_wcsicmp(input.data(), libPath.data()) || !_wcsicmp(input.data(), os2LibPath.data());
        }

        void parse(wchar_t* block)
        {
            for (wchar_t const* lastCh{ block }; *lastCh != '\0'; ++lastCh)
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
        env()
        {
        }

        env(wchar_t* block)
        {
            parse(block);
        }

        void regenerate()
        {
            // Generally replicates the behavior of shell32!RegenerateUserEnvironment
            get(std::wstring{ til::details::vars::system_root });
            get(std::wstring{ til::details::vars::system_drive });
            get(std::wstring{ til::details::vars::all_users_profile });
            get(std::wstring{ til::details::vars::public_var });
            get(std::wstring{ til::details::vars::program_data });
            get_computer_name();
            get_user_name_and_domain();
            get(std::wstring{ til::details::vars::user_dns_domain });
            get(std::wstring{ til::details::vars::home_drive });
            get(std::wstring{ til::details::vars::home_share });
            get(std::wstring{ til::details::vars::home_path });
            get(std::wstring{ til::details::vars::user_profile });
            get(std::wstring{ til::details::vars::app_data });
            get(std::wstring{ til::details::vars::local_app_data });
            get_program_files();
            get_vars_from_registry(HKEY_LOCAL_MACHINE, til::details::vars::reg::system_env_var_root);
            // not processing autoexec.bat
            get_vars_from_registry(HKEY_CURRENT_USER, til::details::vars::reg::user_env_var_root);
            get_vars_from_registry(HKEY_CURRENT_USER, til::details::vars::reg::user_volatile_env_var_root);
            get_vars_from_registry(HKEY_CURRENT_USER, fmt::format(til::details::vars::reg::user_volatile_session_env_var_root_pattern, NtCurrentTeb()->ProcessEnvironmentBlock->SessionId));
        }

        std::wstring to_string()
        {
            std::wstring result;
            for (const auto& [name, value] : _envMap)
            {
                result += name;
                result += L"=";
                result += value;
                result.append(L"\0", 1); // Override string's natural propensity to stop at \0
            }
            result.append(L"\0", 1);
            return result;
        }

        std::map<std::wstring, std::wstring, til::details::wstring_case_insensitive_compare>& as_map()
        {
            return _envMap;
        }

        // TODO: should we be a bunch of goofs and make a "watcher" here that sets up its own
        //       quiet little HWND and message pump watching for the WM_SETTINGCHANGE?
    };
};
