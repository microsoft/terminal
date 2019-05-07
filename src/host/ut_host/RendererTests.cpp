// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "WexTestClass.h"

#include "CommonState.hpp"

#include "..\..\host\renderData.hpp"
#include "..\..\renderer\base\renderer.hpp"

using namespace WEX::Logging;
using namespace WEX::TestExecution;

class RendererTests
{
    TEST_CLASS(RendererTests);

    std::unique_ptr<CommonState> m_state;
    std::unique_ptr<RenderData> m_renderData;
    std::unique_ptr<Renderer> m_renderer;

    TEST_CLASS_SETUP(ClassSetup)
    {
        m_state = std::make_unique<CommonState>();

        m_state->PrepareGlobalFont();
        m_state->PrepareGlobalScreenBuffer();

        m_state->PrepareGlobalInputBuffer();

        m_renderData = std::make_unique<RenderData>();

        return true;
    }

    TEST_CLASS_CLEANUP(ClassCleanup)
    {
        m_renderData.reset(nullptr);

        m_state->CleanupGlobalInputBuffer();

        m_state->CleanupGlobalScreenBuffer();
        m_state->CleanupGlobalFont();

        m_state.reset(nullptr);

        return true;
    }

    TEST_METHOD_SETUP(MethodSetup)
    {
        Renderer* pRenderer = nullptr;
        Globals& g = ServiceLocator::LocateGlobals();
        CONSOLE_INFORMATION& gci = g.getConsoleInformation();
        VERIFY_SUCCEEDED(Renderer::s_CreateInstance(gci.renderData, &pRenderer));
        m_renderer.reset(pRenderer);
        return true;
    }

    TEST_METHOD_CLEANUP(MethodCleanup)
    {
        m_renderer.reset(nullptr);
        return true;
    }

    TEST_METHOD(Sample)
    {
        m_renderer->TriggerTitleChange();
    }
};
