// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include <WexTestClass.h>

#include "../cascadia/TerminalCore/Terminal.hpp"
#include "../renderer/inc/DummyRenderTarget.hpp"
#include "consoletaeftemplates.hpp"

#include "winrt/Microsoft.Terminal.Settings.h"

using namespace winrt::Microsoft::Terminal::Settings;
using namespace Microsoft::Terminal::Core;

namespace TerminalCoreUnitTests
{
class ScreenSizeLimitsTest
{
	TEST_CLASS(ScreenSizeLimitsTest);

	TEST_METHOD(ScreenWidthAndHeightAreClampedToBounds)
	{
		Terminal term;
		DummyRenderTarget emptyRenderTarget;
		auto settings = CreateTerminalSettings(9999999, -1234, 10000);
		term.CreateFromSettings(settings, emptyRenderTarget);
		
		COORD actualDimensions = term.GetViewport().Dimensions();
		VERIFY_ARE_EQUAL(actualDimensions.Y, 32767, L"Row count clamped to SHRT_MAX");
		VERIFY_ARE_EQUAL(actualDimensions.X, 1, L"Column count clamped to 1");
	}

	TEST_METHOD(ScrollbackHistorySizeIsClampedToBounds)
	{

	}

private:
	TerminalSettings CreateTerminalSettings(int initialRows, int initialCols, int historySize)
	{
		auto result = TerminalSettings{};
		result.HistorySize(historySize);
		result.InitialRows(initialRows);
		result.InitialCols(initialCols);
		return result;
	}
};
}
