#pragma once

#include "TermControl.h"
#include "TermControlAP.g.h"
#include <winrt/Microsoft.Terminal.TerminalControl.h>
#include <winrt/Windows.UI.Xaml.h>
#include <winrt/Windows.UI.Xaml.Automation.Peers.h>
#include "../../renderer/inc/IRenderData.hpp"
#include "../types/ScreenInfoUiaProvider.h"
#include "../types/WindowUiaProviderBase.hpp"

using namespace Microsoft::Console::Render;
using namespace Microsoft::Console::Types;
using namespace winrt::Windows::UI::Xaml::Automation::Peers;

namespace winrt::Microsoft::Terminal::TerminalControl::implementation
{
    struct TermControlAP :
        public TermControlAPT<TermControlAP>
    {
    public:
        TermControlAP(winrt::Microsoft::Terminal::TerminalControl::implementation::TermControl const& owner);

        winrt::hstring GetClassNameCore() const;
        AutomationControlType GetAutomationControlTypeCore() const;
        winrt::hstring GetLocalizedControlTypeCore() const;
        winrt::Windows::Foundation::IInspectable GetPatternCore(PatternInterface patternInterface) const;

        // ITextProvider Pattern
        Windows::UI::Xaml::Automation::Provider::ITextRangeProvider RangeFromPoint(Windows::Foundation::Point screenLocation);
        Windows::UI::Xaml::Automation::Provider::ITextRangeProvider RangeFromChild(Windows::UI::Xaml::Automation::Provider::IRawElementProviderSimple childElement);
        winrt::com_array<Windows::UI::Xaml::Automation::Provider::ITextRangeProvider> GetVisibleRanges();
        winrt::com_array<Windows::UI::Xaml::Automation::Provider::ITextRangeProvider> GetSelection();
        Windows::UI::Xaml::Automation::SupportedTextSelection SupportedTextSelection();
        Windows::UI::Xaml::Automation::Provider::ITextRangeProvider DocumentRange();

        RECT GetBoundingRectWrapped();

        int32_t MyProperty() { return 0; }

    private:
        TermControl* _owner;
        ScreenInfoUiaProvider _uiaProvider;
    };
}
