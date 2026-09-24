// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "../../renderer/atlas/AtlasEngine.h"

#include <psapi.h> // GetProcessMemoryInfo

using namespace Microsoft::Console::Render;
using namespace Microsoft::Console::Render::Atlas;
using namespace WEX::Logging;
using namespace WEX::TestExecution;
using namespace WEX::Common;

namespace ControlUnitTests
{
    class AtlasEngineTests
    {
        BEGIN_TEST_CLASS(AtlasEngineTests)
            TEST_CLASS_PROPERTY(L"TestTimeout", L"0:0:30") // 30s timeout
        END_TEST_CLASS()

        TEST_METHOD(SettingsUpdateRecoversFromAllocationFailure);

    private:
        static void initFont(AtlasEngine& engine)
        {
            FontInfoDesired desired{ L"Consolas", 0, FW_NORMAL, 12.0f, CP_UTF8 };
            FontInfo actual{ L"Consolas", 0, FW_NORMAL, { 0, 12 }, CP_UTF8, false };
            VERIFY_SUCCEEDED(engine.UpdateFont(desired, actual));
        }

        static HRESULT setViewport(AtlasEngine& engine, til::CoordType width, til::CoordType height)
        {
            return engine.UpdateViewport(til::inclusive_rect{ 0, 0, width - 1, height - 1 });
        }

        static CursorOptions cursorAtRow(til::CoordType row) noexcept
        {
            CursorOptions options{};
            options.coordCursor = { 0, row };
            options.cursorType = CursorType::Legacy;
            options.ulCursorHeightPercent = 25;
            options.isOn = true;
            return options;
        }

        struct Ballast
        {
            std::vector<void*> blocks;
            size_t bytes = 0;
            bool exhausted = false;
        };

        // Hard stop for the ballast fill, in case the Job Object limit fails to constrain us.
        static constexpr size_t ballastSafetyLimit = size_t{ 256 } << 20;

        // Limits the process commit charge to its current usage plus `headroom` bytes.
        // Returns an empty handle if the limit cannot be applied.
        static wil::unique_handle capProcessCommit(size_t headroom)
        {
            PROCESS_MEMORY_COUNTERS counters{ .cb = sizeof(PROCESS_MEMORY_COUNTERS) };
            if (!GetProcessMemoryInfo(GetCurrentProcess(), &counters, sizeof(counters)))
            {
                return {};
            }

            wil::unique_handle job{ CreateJobObjectW(nullptr, nullptr) };
            if (!job)
            {
                return {};
            }

            JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
            limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_PROCESS_MEMORY;
            limits.ProcessMemoryLimit = counters.PagefileUsage + headroom;
            if (!SetInformationJobObject(job.get(), JobObjectExtendedLimitInformation, &limits, sizeof(limits)) ||
                !AssignProcessToJobObject(job.get(), GetCurrentProcess()))
            {
                return {};
            }
            return job;
        }

        // A process cannot leave its job, but clearing the limits makes it inert for any tests
        // that run in this host afterwards.
        static void uncapProcessCommit(HANDLE job) noexcept
        {
            JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
            SetInformationJobObject(job, JobObjectExtendedLimitInformation, &limits, sizeof(limits));
        }

        // Commits memory in shrinking chunks until even a 64 KiB request fails. `blocks` is
        // reserved up front so the squeeze itself doesn't allocate from the heap.
        static Ballast exhaustCommitBudget()
        {
            Ballast ballast;
            ballast.blocks.reserve(4096);
            for (const auto chunk : { size_t{ 4 } << 20, size_t{ 256 } << 10, size_t{ 64 } << 10 })
            {
                while (ballast.bytes < ballastSafetyLimit)
                {
                    const auto block = VirtualAlloc(nullptr, chunk, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
                    if (!block)
                    {
                        break;
                    }
                    ballast.blocks.push_back(block);
                    ballast.bytes += chunk;
                }
            }
            ballast.exhausted = ballast.bytes < ballastSafetyLimit;
            return ballast;
        }

        static void releaseBallast(Ballast& ballast) noexcept
        {
            for (const auto block : ballast.blocks)
            {
                VirtualFree(block, 0, MEM_RELEASE);
            }
            ballast.blocks.clear();
            ballast.bytes = 0;
        }
    };

    // GH#20269: A bad_alloc during _handleSettingsUpdate() used to leave _p.s permanently out of
    // sync with the buffers it describes, because StartPaint()'s `_p.s != _api.s` check never
    // retried the rebuild. A later, healthy frame then read rows that were never rebuilt.
    void AtlasEngineTests::SettingsUpdateRecoversFromAllocationFailure()
    {
        AtlasEngine engine;
        initFont(engine);
        VERIFY_SUCCEEDED(setViewport(engine, 120, 30));
        VERIFY_SUCCEEDED(engine.StartPaint());

        // 426x113 cells = a maximized 4K window at 9x19px per cell. Its color bitmap alone
        // (432 * 113 * 3 * sizeof(u32) after row stride padding, ~572 KiB) cannot fit into the
        // headroom the ballast below leaves behind.
        VERIFY_SUCCEEDED(setViewport(engine, 426, 113));

        const auto job = capProcessCommit(size_t{ 64 } << 20);
        if (!job)
        {
            Log::Result(TestResults::Skipped, L"cannot place the test process under a Job Object commit limit");
            return;
        }
        auto uncap = wil::scope_exit([&]() noexcept { uncapProcessCommit(job.get()); });

        auto ballast = exhaustCommitBudget();
        auto releaseOnUnwind = wil::scope_exit([&]() noexcept { releaseBallast(ballast); });
        if (!ballast.exhausted)
        {
            releaseBallast(ballast);
            Log::Result(TestResults::Skipped, L"the commit limit did not constrain the process");
            return;
        }

        // No Log::Comment and no VERIFY until the ballast is released: the test host itself
        // cannot reliably allocate here.
        const auto pressured = engine.StartPaint();
        releaseBallast(ballast);

        VERIFY_ARE_EQUAL(E_OUTOFMEMORY, pressured);

        // The pressure has passed; the engine must retry the rebuild on its own.
        VERIFY_SUCCEEDED(engine.StartPaint());
        VERIFY_SUCCEEDED(engine.PaintCursor(cursorAtRow(112)));
    }
}
