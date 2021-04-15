/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- ConsoleArguments.hpp

Abstract:
- Encapsulates the commandline arguments to the console host.

Author(s):
- Mike Griese (migrie) 07-Sept-2017
--*/

#pragma once

#ifdef UNIT_TESTING
#include "WexTestClass.h"
#endif

class ConsoleArguments
{
public:
    ConsoleArguments(const std::wstring& commandline,
                     const HANDLE hStdIn,
                     const HANDLE hStdOut);

    ConsoleArguments();

    ConsoleArguments& operator=(const ConsoleArguments& other);

    [[nodiscard]] HRESULT ParseCommandline();

    bool HasVtHandles() const;
    bool InConptyMode() const noexcept;
    bool IsHeadless() const;
    bool ShouldCreateServerHandle() const;
    bool ShouldRunAsComServer() const;

    HANDLE GetServerHandle() const;
    HANDLE GetVtInHandle() const;
    HANDLE GetVtOutHandle() const;

    bool HasSignalHandle() const;
    HANDLE GetSignalHandle() const;

    std::wstring GetOriginalCommandLine() const;
    std::wstring GetClientCommandline() const;
    std::wstring GetVtMode() const;
    bool GetForceV1() const;
    bool GetForceNoHandoff() const;

    short GetWidth() const;
    short GetHeight() const;
    bool GetInheritCursor() const;
    bool IsResizeQuirkEnabled() const;
    bool IsWin32InputModeEnabled() const;

    void SetExpectedSize(COORD dimensions) noexcept;

#ifdef UNIT_TESTING
    void EnableConptyModeForTests();
#endif

    static const std::wstring_view VT_MODE_ARG;
    static const std::wstring_view HEADLESS_ARG;
    static const std::wstring_view SERVER_HANDLE_ARG;
    static const std::wstring_view SIGNAL_HANDLE_ARG;
    static const std::wstring_view HANDLE_PREFIX;
    static const std::wstring_view CLIENT_COMMANDLINE_ARG;
    static const std::wstring_view FORCE_V1_ARG;
    static const std::wstring_view FORCE_NO_HANDOFF_ARG;
    static const std::wstring_view FILEPATH_LEADER_PREFIX;
    static const std::wstring_view WIDTH_ARG;
    static const std::wstring_view HEIGHT_ARG;
    static const std::wstring_view INHERIT_CURSOR_ARG;
    static const std::wstring_view RESIZE_QUIRK;
    static const std::wstring_view WIN32_INPUT_MODE;
    static const std::wstring_view FEATURE_ARG;
    static const std::wstring_view FEATURE_PTY_ARG;
    static const std::wstring_view COM_SERVER_ARG;

private:
#ifdef UNIT_TESTING
    // This accessor used to create a copy of this class for unit testing comparison ease.
    ConsoleArguments(const std::wstring commandline,
                     const std::wstring clientCommandline,
                     const HANDLE vtInHandle,
                     const HANDLE vtOutHandle,
                     const std::wstring vtMode,
                     const short width,
                     const short height,
                     const bool forceV1,
                     const bool forceNoHandoff,
                     const bool headless,
                     const bool createServerHandle,
                     const DWORD serverHandle,
                     const DWORD signalHandle,
                     const bool inheritCursor,
                     const bool runAsComServer) :
        _commandline(commandline),
        _clientCommandline(clientCommandline),
        _vtInHandle(vtInHandle),
        _vtOutHandle(vtOutHandle),
        _vtMode(vtMode),
        _width(width),
        _height(height),
        _forceV1(forceV1),
        _forceNoHandoff(forceNoHandoff),
        _headless(headless),
        _createServerHandle(createServerHandle),
        _serverHandle(serverHandle),
        _signalHandle(signalHandle),
        _inheritCursor(inheritCursor),
        _resizeQuirk(false),
        _receivedEarlySizeChange{ false },
        _originalWidth{ -1 },
        _originalHeight{ -1 },
        _runAsComServer{ runAsComServer }
    {
    }
#endif

    std::wstring _commandline;

    std::wstring _clientCommandline;

    HANDLE _vtInHandle;

    HANDLE _vtOutHandle;

    std::wstring _vtMode;

    bool _forceNoHandoff;
    bool _forceV1;
    bool _headless;

    short _width;
    short _height;

    bool _runAsComServer;
    bool _createServerHandle;
    DWORD _serverHandle;
    DWORD _signalHandle;
    bool _inheritCursor;
    bool _resizeQuirk{ false };
    bool _win32InputMode{ false };

    bool _receivedEarlySizeChange;
    short _originalWidth;
    short _originalHeight;

    [[nodiscard]] HRESULT _GetClientCommandline(_Inout_ std::vector<std::wstring>& args,
                                                const size_t index,
                                                const bool skipFirst);

    static void s_ConsumeArg(_Inout_ std::vector<std::wstring>& args,
                             _In_ size_t& index);
    [[nodiscard]] static HRESULT s_GetArgumentValue(_Inout_ std::vector<std::wstring>& args,
                                                    _Inout_ size_t& index,
                                                    _Out_opt_ std::wstring* const pSetting);
    [[nodiscard]] static HRESULT s_GetArgumentValue(_Inout_ std::vector<std::wstring>& args,
                                                    _Inout_ size_t& index,
                                                    _Out_opt_ short* const pSetting);
    [[nodiscard]] static HRESULT s_HandleFeatureValue(_Inout_ std::vector<std::wstring>& args,
                                                      _Inout_ size_t& index);

    [[nodiscard]] static HRESULT s_ParseHandleArg(const std::wstring& handleAsText,
                                                  _Inout_ DWORD& handleAsVal);

#ifdef UNIT_TESTING
    friend class ConsoleArgumentsTests;
#endif
};

#ifdef UNIT_TESTING
namespace WEX
{
    namespace TestExecution
    {
        template<>
        class VerifyOutputTraits<ConsoleArguments>
        {
        public:
            static WEX::Common::NoThrowString ToString(const ConsoleArguments& ci)
            {
                return WEX::Common::NoThrowString().Format(L"\r\nClient Command Line: '%ws',\r\n"
                                                           L"Use VT Handles: '%ws',\r\n"
                                                           L"VT In Handle: '0x%x',\r\n"
                                                           L"VT Out Handle: '0x%x',\r\n"
                                                           L"Vt Mode: '%ws',\r\n"
                                                           L"WidthxHeight: '%dx%d',\r\n"
                                                           L"ForceV1: '%ws',\r\n"
                                                           L"Headless: '%ws',\r\n"
                                                           L"Create Server Handle: '%ws',\r\n"
                                                           L"Server Handle: '0x%x'\r\n"
                                                           L"Use Signal Handle: '%ws'\r\n"
                                                           L"Signal Handle: '0x%x'\r\n",
                                                           L"Inherit Cursor: '%ws'\r\n",
                                                           L"Run As Com Server: '%ws'\r\n",
                                                           ci.GetClientCommandline().c_str(),
                                                           s_ToBoolString(ci.HasVtHandles()),
                                                           ci.GetVtInHandle(),
                                                           ci.GetVtOutHandle(),
                                                           ci.GetVtMode().c_str(),
                                                           ci.GetWidth(),
                                                           ci.GetHeight(),
                                                           s_ToBoolString(ci.GetForceV1()),
                                                           s_ToBoolString(ci.IsHeadless()),
                                                           s_ToBoolString(ci.ShouldCreateServerHandle()),
                                                           ci.GetServerHandle(),
                                                           s_ToBoolString(ci.HasSignalHandle()),
                                                           ci.GetSignalHandle(),
                                                           s_ToBoolString(ci.GetInheritCursor()),
                                                           s_ToBoolString(ci.ShouldRunAsComServer()));
            }

        private:
            static PCWSTR s_ToBoolString(const bool val)
            {
                return val ? L"true" : L"false";
            }
        };

        template<>
        class VerifyCompareTraits<ConsoleArguments, ConsoleArguments>
        {
        public:
            static bool AreEqual(const ConsoleArguments& expected, const ConsoleArguments& actual)
            {
                return expected.GetClientCommandline() == actual.GetClientCommandline() &&
                       expected.HasVtHandles() == actual.HasVtHandles() &&
                       expected.GetVtInHandle() == actual.GetVtInHandle() &&
                       expected.GetVtOutHandle() == actual.GetVtOutHandle() &&
                       expected.GetVtMode() == actual.GetVtMode() &&
                       expected.GetWidth() == actual.GetWidth() &&
                       expected.GetHeight() == actual.GetHeight() &&
                       expected.GetForceV1() == actual.GetForceV1() &&
                       expected.IsHeadless() == actual.IsHeadless() &&
                       expected.ShouldCreateServerHandle() == actual.ShouldCreateServerHandle() &&
                       expected.GetServerHandle() == actual.GetServerHandle() &&
                       expected.HasSignalHandle() == actual.HasSignalHandle() &&
                       expected.GetSignalHandle() == actual.GetSignalHandle() &&
                       expected.GetInheritCursor() == actual.GetInheritCursor();
            }

            static bool AreSame(const ConsoleArguments& expected, const ConsoleArguments& actual)
            {
                return &expected == &actual;
            }

            static bool IsLessThan(const ConsoleArguments&, const ConsoleArguments&) = delete;

            static bool IsGreaterThan(const ConsoleArguments&, const ConsoleArguments&) = delete;

            static bool IsNull(const ConsoleArguments& object)
            {
                return object.GetClientCommandline().empty() &&
                       (object.GetVtInHandle() == 0 || object.GetVtInHandle() == INVALID_HANDLE_VALUE) &&
                       (object.GetVtOutHandle() == 0 || object.GetVtOutHandle() == INVALID_HANDLE_VALUE) &&
                       object.GetVtMode().empty() &&
                       !object.GetForceV1() &&
                       (object.GetWidth() == 0) &&
                       (object.GetHeight() == 0) &&
                       !object.IsHeadless() &&
                       !object.ShouldCreateServerHandle() &&
                       object.GetServerHandle() == 0 &&
                       (object.GetSignalHandle() == 0 || object.GetSignalHandle() == INVALID_HANDLE_VALUE) &&
                       !object.GetInheritCursor();
            }
        };
    }
}
#endif
