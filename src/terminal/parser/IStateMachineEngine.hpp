// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

/*
Module Name:
- IStateMachineEngine.hpp

Abstract:
- This is the interface for a VT state machine language
    The terminal handles input sequences and output sequences differently,
    almost as two separate grammars. This enables different grammars to leverage
    the existing VT parsing.
*/
#pragma once
namespace Microsoft::Console::VirtualTerminal
{
    class IStateMachineEngine
    {
    public:
        virtual ~IStateMachineEngine() = 0;
        IStateMachineEngine(const IStateMachineEngine&) = default;
        IStateMachineEngine(IStateMachineEngine&&) = default;
        IStateMachineEngine& operator=(const IStateMachineEngine&) = default;
        IStateMachineEngine& operator=(IStateMachineEngine&&) = default;

        virtual bool ActionExecute(const wchar_t wch) = 0;
        virtual bool ActionExecuteFromEscape(const wchar_t wch) = 0;
        virtual bool ActionPrint(const wchar_t wch) = 0;
        virtual bool ActionPrintString(const std::wstring_view string) = 0;

        virtual bool ActionPassThroughString(const std::wstring_view string) = 0;

        virtual bool ActionEscDispatch(const wchar_t wch,
                                       const std::basic_string_view<wchar_t> intermediates) = 0;
        virtual bool ActionVt52EscDispatch(const wchar_t wch,
                                           const std::basic_string_view<wchar_t> intermediates,
                                           const std::basic_string_view<size_t> parameters) = 0;
        virtual bool ActionCsiDispatch(const wchar_t wch,
                                       const std::basic_string_view<wchar_t> intermediates,
                                       const std::basic_string_view<size_t> parameters) = 0;

        virtual bool ActionClear() = 0;

        virtual bool ActionIgnore() = 0;

        virtual bool ActionOscDispatch(const wchar_t wch,
                                       const size_t parameter,
                                       const std::wstring_view string) = 0;

        virtual bool ActionSs3Dispatch(const wchar_t wch,
                                       const std::basic_string_view<size_t> parameters) = 0;

        virtual bool ParseControlSequenceAfterSs3() const = 0;
        virtual bool FlushAtEndOfString() const = 0;
        virtual bool DispatchControlCharsFromEscape() const = 0;
        virtual bool DispatchIntermediatesFromEscape() const = 0;

    protected:
        IStateMachineEngine() = default;
    };

    inline IStateMachineEngine::~IStateMachineEngine() {}
}
