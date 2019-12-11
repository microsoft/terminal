// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

/*
Module Name:
- IStateMachineEngine.hpp

Abstract:
- This is the interface for a VT state machine language
    The terminal handles input sequences and output sequences differently,
    almost as two seperate grammars. This enables different grammars to leverage
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
        virtual bool ActionPrintString(const wchar_t* const rgwch,
                                       size_t const cch) = 0;

        virtual bool ActionPassThroughString(const wchar_t* const rgwch,
                                             size_t const cch) = 0;

        virtual bool ActionEscDispatch(const wchar_t wch,
                                       const unsigned short cIntermediate,
                                       const wchar_t wchIntermediate) = 0;
        virtual bool ActionCsiDispatch(const wchar_t wch,
                                       const unsigned short cIntermediate,
                                       const wchar_t wchIntermediate,
                                       _In_reads_(cParams) const unsigned short* const rgusParams,
                                       const unsigned short cParams) = 0;

        virtual bool ActionClear() = 0;

        virtual bool ActionIgnore() = 0;

        virtual bool ActionOscDispatch(const wchar_t wch,
                                       const unsigned short sOscParam,
                                       _Inout_updates_(cchOscString) wchar_t* const pwchOscStringBuffer,
                                       const unsigned short cchOscString) = 0;

        virtual bool ActionSs3Dispatch(const wchar_t wch,
                                       _In_reads_(cParams) const unsigned short* const rgusParams,
                                       const unsigned short cParams) = 0;

        virtual bool FlushAtEndOfString() const = 0;
        virtual bool DispatchControlCharsFromEscape() const = 0;
        virtual bool DispatchIntermediatesFromEscape() const = 0;

    protected:
        IStateMachineEngine() = default;
    };

    inline IStateMachineEngine::~IStateMachineEngine() {}
}
