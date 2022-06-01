/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- sgrStack.hpp

Abstract:
- Encapsulates logic for the XTPUSHSGR / XTPOPSGR VT control sequences, which save and
  restore text attributes on a stack.

--*/

#pragma once

#include "../../buffer/out/TextAttribute.hpp"
#include "../../terminal/adapter/DispatchTypes.hpp"
#include <til/enumset.h>

namespace Microsoft::Console::VirtualTerminal
{
    class SgrStack
    {
    public:
        SgrStack() noexcept;

        // Method Description:
        // - Saves the specified text attributes onto an internal stack.
        // Arguments:
        // - currentAttributes - The attributes to save onto the stack.
        // - options - If none supplied, the full attributes are saved. Else only the
        //   specified parts of currentAttributes are saved.
        // Return Value:
        // - <none>
        void Push(const TextAttribute& currentAttributes,
                  const VTParameters options) noexcept;

        // Method Description:
        // - Restores text attributes by removing from the top of the internal stack,
        //   combining them with the supplied currentAttributes, if appropriate.
        // Arguments:
        // - currentAttributes - The current text attributes. If only a portion of
        //   attributes were saved on the internal stack, then those attributes will be
        //   combined with the currentAttributes passed in to form the return value.
        // Return Value:
        // - The TextAttribute that has been removed from the top of the stack, possibly
        //   combined with currentAttributes.
        const TextAttribute Pop(const TextAttribute& currentAttributes) noexcept;

        // Xterm allows the save stack to go ten deep, so we'll follow suit.
        static constexpr int c_MaxStoredSgrPushes = 10;

    private:
        typedef til::enumset<DispatchTypes::SgrSaveRestoreStackOptions> AttrBitset;

        TextAttribute _CombineWithCurrentAttributes(const TextAttribute& currentAttributes,
                                                    const TextAttribute& savedAttribute,
                                                    const AttrBitset validParts) noexcept; // valid parts of savedAttribute

        struct SavedSgrAttributes
        {
            TextAttribute TextAttributes;
            AttrBitset ValidParts; // flags that indicate which parts of TextAttributes are meaningful
        };

        // The number of "save slots" on the stack is limited (let's say there are N). So
        // there are a couple of problems to think about: what to do about apps that try
        // to do more pushes than will fit, and how to recover from garbage (such as
        // accidentally running "cat" on a binary file that looks like lots of pushes).
        //
        // Dealing with more pops than pushes is simple: just ignore pops when the stack
        // is empty.
        //
        // But how should we handle doing more pushes than are supported by the storage?
        //
        // One approach might be to ignore pushes once the stack is full. Things won't
        // look right while the number of outstanding pushes is above the stack, but once
        // it gets popped back down into range, things start working again. Put another
        // way: with a traditional stack, the first N pushes work, and the last N pops
        // work. But that introduces a burden: you have to do something (lots of pops) in
        // order to recover from garbage. (There are strategies that could be employed to
        // place an upper bound on how many pops are required (say K), but it's still
        // something that /must/ be done to recover from a blown stack.)
        //
        // An alternative approach is a "ring stack": if you do another push when the
        // stack is already full, it just drops the bottom of the stack. With this
        // strategy, the last N pushes work, and the first N pops work. And the advantage
        // of this approach is that there is no "recovery procedure" necessary: if you
        // want a clean slate, you can just declare a clean slate--you will always have N
        // slots for pushes and pops in front of you.
        //
        // A ring stack will also lead to apps that are friendlier to cross-app
        // pushes/pops.
        //
        // Consider using a traditional stack. In that case, an app might be tempted to
        // always begin by issuing a bunch of pops (K), in order to ensure they have a
        // clean state. However, apps that behave that way would not work well with
        // cross-app push/pops (e.g. I push before I ssh to my remote system, and will pop
        // when after closing the connection, and during the connection I'll run apps on
        // the remote host which might also do pushes and pops). By using a ring stack, an
        // app does not need to do /anything/ to start in a "clean state"--an app can
        // ALWAYS consider its initial state to be clean.
        //
        // So we've chosen to use a "ring stack", because it is simplest for apps to deal
        // with.

        int _nextPushIndex; // will wrap around once the stack is full
        int _numSavedAttrs; // how much of _storedSgrAttributes is actually in use
        std::array<SavedSgrAttributes, c_MaxStoredSgrPushes> _storedSgrAttributes;
    };
}
