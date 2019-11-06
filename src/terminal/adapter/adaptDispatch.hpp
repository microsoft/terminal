/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- adaptDispatch.hpp

Abstract:
- This serves as the Windows Console API-specific implementation of the callbacks from our generic Virtual Terminal parser.

Author(s):
- Michael Niksa (MiNiksa) 30-July-2015
--*/

#pragma once

#include "termDispatch.hpp"
#include "DispatchCommon.hpp"
#include "conGetSet.hpp"
#include "adaptDefaults.hpp"
#include "terminalOutput.hpp"
#include <math.h>

namespace Microsoft::Console::VirtualTerminal
{
    class AdaptDispatch : public ITermDispatch
    {
    public:
        AdaptDispatch(ConGetSet* const pConApi,
                      AdaptDefaults* const pDefaults);

        void Execute(const wchar_t wchControl) override
        {
            _pDefaults->Execute(wchControl);
        }

        void PrintString(const wchar_t* const rgwch, const size_t cch) override;
        void Print(const wchar_t wchPrintable) override;

        bool CursorUp(_In_ unsigned int const uiDistance) override; // CUU
        bool CursorDown(_In_ unsigned int const uiDistance) override; // CUD
        bool CursorForward(_In_ unsigned int const uiDistance) override; // CUF
        bool CursorBackward(_In_ unsigned int const uiDistance) override; // CUB
        bool CursorNextLine(_In_ unsigned int const uiDistance) override; // CNL
        bool CursorPrevLine(_In_ unsigned int const uiDistance) override; // CPL
        bool CursorHorizontalPositionAbsolute(_In_ unsigned int const uiColumn) override; // CHA
        bool VerticalLinePositionAbsolute(_In_ unsigned int const uiLine) override; // VPA
        bool CursorPosition(_In_ unsigned int const uiLine, _In_ unsigned int const uiColumn) override; // CUP
        bool CursorSaveState() override; // DECSC
        bool CursorRestoreState() override; // DECRC
        bool CursorVisibility(const bool fIsVisible) override; // DECTCEM
        bool EraseInDisplay(const DispatchTypes::EraseType eraseType) override; // ED
        bool EraseInLine(const DispatchTypes::EraseType eraseType) override; // EL
        bool EraseCharacters(_In_ unsigned int const uiNumChars) override; // ECH
        bool InsertCharacter(_In_ unsigned int const uiCount) override; // ICH
        bool DeleteCharacter(_In_ unsigned int const uiCount) override; // DCH
        bool SetGraphicsRendition(_In_reads_(cOptions) const DispatchTypes::GraphicsOptions* const rgOptions,
                                  const size_t cOptions) override; // SGR
        bool DeviceStatusReport(const DispatchTypes::AnsiStatusType statusType) override; // DSR
        bool DeviceAttributes() override; // DA
        bool ScrollUp(_In_ unsigned int const uiDistance) override; // SU
        bool ScrollDown(_In_ unsigned int const uiDistance) override; // SD
        bool InsertLine(_In_ unsigned int const uiDistance) override; // IL
        bool DeleteLine(_In_ unsigned int const uiDistance) override; // DL
        bool SetColumns(_In_ unsigned int const uiColumns) override; // DECSCPP, DECCOLM
        bool SetPrivateModes(_In_reads_(cParams) const DispatchTypes::PrivateModeParams* const rParams,
                             const size_t cParams) override; // DECSET
        bool ResetPrivateModes(_In_reads_(cParams) const DispatchTypes::PrivateModeParams* const rParams,
                               const size_t cParams) override; // DECRST
        bool SetCursorKeysMode(const bool fApplicationMode) override; // DECCKM
        bool SetKeypadMode(const bool fApplicationMode) override; // DECKPAM, DECKPNM
        bool EnableCursorBlinking(const bool bEnable) override; // ATT610
        bool SetOriginMode(const bool fRelativeMode) override; // DECOM
        bool SetTopBottomScrollingMargins(const SHORT sTopMargin,
                                          const SHORT sBottomMargin) override; // DECSTBM
        bool ReverseLineFeed() override; // RI
        bool SetWindowTitle(const std::wstring_view title) override; // OscWindowTitle
        bool UseAlternateScreenBuffer() override; // ASBSET
        bool UseMainScreenBuffer() override; // ASBRST
        bool HorizontalTabSet() override; // HTS
        bool ForwardTab(const SHORT sNumTabs) override; // CHT
        bool BackwardsTab(const SHORT sNumTabs) override; // CBT
        bool TabClear(const SHORT sClearType) override; // TBC
        bool DesignateCharset(const wchar_t wchCharset) override; // DesignateCharset
        bool SoftReset() override; // DECSTR
        bool HardReset() override; // RIS
        bool EnableDECCOLMSupport(const bool fEnabled) override; // ?40
        bool EnableVT200MouseMode(const bool fEnabled) override; // ?1000
        bool EnableUTF8ExtendedMouseMode(const bool fEnabled) override; // ?1005
        bool EnableSGRExtendedMouseMode(const bool fEnabled) override; // ?1006
        bool EnableButtonEventMouseMode(const bool fEnabled) override; // ?1002
        bool EnableAnyEventMouseMode(const bool fEnabled) override; // ?1003
        bool EnableAlternateScroll(const bool fEnabled) override; // ?1007
        bool SetCursorStyle(const DispatchTypes::CursorStyle cursorStyle) override; // DECSCUSR
        bool SetCursorColor(const COLORREF cursorColor) override;

        bool SetColorTableEntry(const size_t tableIndex,
                                const DWORD dwColor) override; // OscColorTable
        bool SetDefaultForeground(const DWORD dwColor) override; // OSCDefaultForeground
        bool SetDefaultBackground(const DWORD dwColor) override; // OSCDefaultBackground

        bool WindowManipulation(const DispatchTypes::WindowManipulationType uiFunction,
                                _In_reads_(cParams) const unsigned short* const rgusParams,
                                const size_t cParams) override; // DTTERM_WindowManipulation

    private:
        enum class CursorDirection
        {
            Up,
            Down,
            Left,
            Right,
            NextLine,
            PrevLine
        };
        enum class ScrollDirection
        {
            Up,
            Down
        };
        struct CursorState
        {
            unsigned int Row = 1;
            unsigned int Column = 1;
            bool IsOriginModeRelative = false;
            TextAttribute Attributes = {};
            TerminalOutput TermOutput = {};
        };

        bool _CursorMovement(const CursorDirection dir, _In_ unsigned int const uiDistance) const;
        bool _CursorMovePosition(_In_opt_ const unsigned int* const puiRow, _In_opt_ const unsigned int* const puiCol) const;
        bool _EraseSingleLineHelper(const CONSOLE_SCREEN_BUFFER_INFOEX* const pcsbiex, const DispatchTypes::EraseType eraseType, const SHORT sLineId, const WORD wFillColor) const;
        void _SetGraphicsOptionHelper(const DispatchTypes::GraphicsOptions opt, _Inout_ WORD* const pAttr);
        bool _EraseAreaHelper(const COORD coordStartPosition, const COORD coordLastPosition, const WORD wFillColor);
        bool _EraseSingleLineDistanceHelper(const COORD coordStartPosition, const DWORD dwLength, const WORD wFillColor) const;
        bool _EraseScrollback();
        bool _EraseAll();
        void _SetGraphicsOptionHelper(const DispatchTypes::GraphicsOptions opt, _Inout_ WORD* const pAttr) const;
        bool _InsertDeleteHelper(_In_ unsigned int const uiCount, const bool fIsInsert) const;
        bool _ScrollMovement(const ScrollDirection dir, _In_ unsigned int const uiDistance) const;
        static void s_DisableAllColors(_Inout_ WORD* const pAttr, const bool fIsForeground);
        static void s_ApplyColors(_Inout_ WORD* const pAttr, const WORD wApplyThis, const bool fIsForeground);

        bool _DoSetTopBottomScrollingMargins(const SHORT sTopMargin,
                                             const SHORT sBottomMargin);
        bool _CursorPositionReport() const;

        bool _WriteResponse(_In_reads_(cchReply) PCWSTR pwszReply, const size_t cchReply) const;
        bool _SetResetPrivateModes(_In_reads_(cParams) const DispatchTypes::PrivateModeParams* const rgParams, const size_t cParams, const bool fEnable);
        bool _PrivateModeParamsHelper(_In_ DispatchTypes::PrivateModeParams const param, const bool fEnable);
        bool _DoDECCOLMHelper(_In_ unsigned int uiColumns);

        std::unique_ptr<ConGetSet> _conApi;
        std::unique_ptr<AdaptDefaults> _pDefaults;
        TerminalOutput _TermOutput;

        // We have two instances of the saved cursor state, because we need
        // one for the main buffer (at index 0), and another for the alt buffer
        // (at index 1). The _usingAltBuffer property keeps tracks of which
        // buffer is active, so can be used as an index into this array to
        // obtain the saved state that should be currently active.
        CursorState _savedCursorState[2];
        bool _usingAltBuffer;

        SMALL_RECT _srScrollMargins;

        bool _fIsOriginModeRelative;

        bool _fIsSetColumnsEnabled;

        bool _fIsDECCOLMAllowed;

        bool _fChangedForeground;
        bool _fChangedBackground;
        bool _fChangedMetaAttrs;

        bool _SetRgbColorsHelper(_In_reads_(cOptions) const DispatchTypes::GraphicsOptions* const rgOptions,
                                 const size_t cOptions,
                                 _Out_ COLORREF* const prgbColor,
                                 _Out_ bool* const pfIsForeground,
                                 _Out_ size_t* const pcOptionsConsumed);

        bool _SetBoldColorHelper(const DispatchTypes::GraphicsOptions option);
        bool _SetDefaultColorHelper(const DispatchTypes::GraphicsOptions option);
        bool _SetExtendedTextAttributeHelper(const DispatchTypes::GraphicsOptions option);

        static bool s_IsXtermColorOption(const DispatchTypes::GraphicsOptions opt);
        static bool s_IsRgbColorOption(const DispatchTypes::GraphicsOptions opt);
        static bool s_IsBoldColorOption(const DispatchTypes::GraphicsOptions opt) noexcept;
        static bool s_IsDefaultColorOption(const DispatchTypes::GraphicsOptions opt) noexcept;
        static bool s_IsExtendedTextAttribute(const DispatchTypes::GraphicsOptions opt) noexcept;
    };
}
