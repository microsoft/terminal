// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once
namespace Microsoft::Terminal::Core
{
    class ITerminalInput
    {
    public:
        virtual ~ITerminalInput() {}

        virtual bool SendKeyEvent(const WORD vkey, const DWORD modifiers) = 0;

        // void SendMouseEvent(uint row, uint col, KeyModifiers modifiers);
        [[nodiscard]] virtual HRESULT UserResize(const COORD size) noexcept = 0;
        virtual void UserScrollViewport(const int viewTop) = 0;
        virtual int GetScrollOffset() = 0;
    };
}
