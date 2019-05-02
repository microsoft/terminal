// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "../../adapter/termDispatch.hpp"

namespace Microsoft
{
    namespace Console
    {
        namespace VirtualTerminal
        {
            class EchoDispatch : public TermDispatch
            {
            public:
                void Print(const wchar_t wchPrintable) override;
                void PrintString(const wchar_t* const rgwch, const size_t cch) override;
                void Execute(const wchar_t wchControl) override;
            };
        }
    }
}
