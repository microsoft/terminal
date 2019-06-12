// Source: https://gist.github.com/rprichard/7ec3fe1b199f513bee82fea196a82a79
// Date Imported: 7/11/2017 @ 2:43PM PDT by miniksa@microsoft.com

/*
The MIT License (MIT)
Copyright (c) 2017 Ryan Prichard
Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:
The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

/*
QUICK START:
Build with one of:
 - cl /EHsc /nologo closetest.cc
 - i686-w64-mingw32-g++ -Wall -static -std=c++11 closetest.cc -o closetest.exe
 - x86_64-w64-mingw32-g++ -Wall -static -std=c++11 closetest.cc -o closetest.exe
Use Sysinternals DbgView to see messages generated while the test runs.
Run the test:
 - Run closetest.exe:  e.g.:
    - Run with no arguments to see the order in which processes are signaled.
    - Run `closetest.exe -d alternate --gap -n 4` to require multiple Close
      button clicks.
 - Observe the "closetest: child nnn: attached to console" messages in DbgView
 - Click the console's Close button.
 - Observe the `CTRL_CLOSE_EVENT` messages in DbgView.
DETAILS:
Use --help to see options.  The program detaches from its console, creates a
new console, then spawns multiple instances of itself.  Some of the children
are configured such that when another child exits, they too exit.  The
dependency is implemented with either a pipe or a job object.  (When A kills B,
either A holds the write-end of a pipe that B reads from, or A holds a job
object that B is assigned to.)
The test demonstrates how it can be necessary to click the console's Close
button multiple times to kill all the processes in the console, even though no
new processes start during the test.
OBSERVATIONS:
Aftering closing a console, Windows delivers a CTRL_CLOSE_EVENT event to each
attached process, giving it 5 seconds to handle it before terminating the
process.  If a process-to-signal has already died, Windows apparently aborts
the signaling process, so all the other processes remain.
Different Windows versions have different behavior:
 * XP: Windows signals processes from last-to-first and skips over
   already-dead processes.  However, it pops up a "Windows can't end this
   program" dialog for every process.
 * Vista and Win 7: Same as XP, but the dialogs don't appear.
 * Win 8 and Win 8.1: Windows signals processes from last-to-first and aborts
   on an already-dead process.
 * Win 10.0.14393: v1 and v2 consoles behave the same as Win 8.1.
 * Win 10.0.15063: v1 behaves as before, but v2 signals in first-to-last
   order.
Use Sysinternals DbgView to see messages generated while the test runs.
EXAMPLE 1: Sample DbgView output (-d none -n 4)
    Last-to-first (legacy order)
        0.04571486  [15948] closetest: child 1: attached to console
        0.08462632  [5132] closetest: child 2: attached to console
        0.09434677  [7300] closetest: child 3: attached to console
        0.13065934  [3920] closetest: child 4: attached to console
        0.13111357  [6888] closetest: attached process list: 3920, 7300, 5132, 15948, 6888
        4.51475239  [3920] closetest: child 4: CTRL_CLOSE_EVENT received, pausing...
        4.77758837  [3920] closetest: child 4: CTRL_CLOSE_EVENT received, exiting...
        4.78069544  [7300] closetest: child 3: CTRL_CLOSE_EVENT received, pausing...
        5.03125525  [7300] closetest: child 3: CTRL_CLOSE_EVENT received, exiting...
        5.03372812  [5132] closetest: child 2: CTRL_CLOSE_EVENT received, pausing...
        5.29285145  [5132] closetest: child 2: CTRL_CLOSE_EVENT received, exiting...
        5.29587698  [15948] closetest: child 1: CTRL_CLOSE_EVENT received, pausing...
        5.55155897  [15948] closetest: child 1: CTRL_CLOSE_EVENT received, exiting...
    First-to-last (order in 15063 v2 console)
        0.06677166  [7144] closetest: child 1: attached to console
        0.07975050  [4568] closetest: child 2: attached to console
        0.08957358  [1100] closetest: child 3: attached to console
        0.10003299  [7904] closetest: child 4: attached to console
        0.10047545  [816] closetest: attached process list: 816, 7144, 4568, 1100, 7904
        4.97500944  [7144] closetest: child 1: CTRL_CLOSE_EVENT received, pausing...
        5.23319721  [7144] closetest: child 1: CTRL_CLOSE_EVENT received, exiting...
        5.23594141  [4568] closetest: child 2: CTRL_CLOSE_EVENT received, pausing...
        5.49145126  [4568] closetest: child 2: CTRL_CLOSE_EVENT received, exiting...
        5.49386692  [1100] closetest: child 3: CTRL_CLOSE_EVENT received, pausing...
        5.74885798  [1100] closetest: child 3: CTRL_CLOSE_EVENT received, exiting...
        5.75202417  [7904] closetest: child 4: CTRL_CLOSE_EVENT received, pausing...
        6.02625036  [7904] closetest: child 4: CTRL_CLOSE_EVENT received, exiting...
EXAMPLE 2: multiple close clicks required
    Run closetest.exe with triggering order equal to the console signaling
    order (backward for v1 or Windows <= 14393, forward for 15063 v2):
        closetest.exe -d backward/forward --gap -m pipe -n 4
    The Close button must be clicked 4 times to finish killing the processes:
        0.10701734  [3776] closetest: child 1: attached to console (child 1 kills child 3)
        0.13791052  [9172] closetest: child 2: attached to console
        0.15273562  [5488] closetest: child 3: attached to console
        0.18887523  [2096] closetest: child 4: attached to console (child 4 kills child 6)
        0.20132381  [9820] closetest: child 5: attached to console
        0.24170215  [1020] closetest: child 6: attached to console
        0.25150394  [7008] closetest: child 7: attached to console (child 7 kills child 9)
        0.29081795  [9148] closetest: child 8: attached to console
        0.30123973  [6040] closetest: child 9: attached to console
        0.33976573  [15356] closetest: child 10: attached to console (child 10 kills child 12)
        0.35181633  [1068] closetest: child 11: attached to console
        0.38771760  [9028] closetest: child 12: attached to console
        0.38878006  [1880] closetest: attached process list: 1880, 3776, 9172, 5488, 2096, 9820, 1020, 7008, 9148, 6040, 15356, 1068, 9028
        6.56705523  [3776] closetest: child 1: CTRL_CLOSE_EVENT received, pausing...
        6.81864643  [3776] closetest: child 1: CTRL_CLOSE_EVENT received, exiting...
        6.82033300  [5488] closetest: child 3: ReadFile() returned, exiting...
        6.82126617  [9172] closetest: child 2: CTRL_CLOSE_EVENT received, pausing...
        7.08446360  [9172] closetest: child 2: CTRL_CLOSE_EVENT received, exiting...
        11.20391464 [2096] closetest: child 4: CTRL_CLOSE_EVENT received, pausing...
        11.45907879 [2096] closetest: child 4: CTRL_CLOSE_EVENT received, exiting...
        11.46081257 [1020] closetest: child 6: ReadFile() returned, exiting...
        11.46188736 [9820] closetest: child 5: CTRL_CLOSE_EVENT received, pausing...
        11.72536087 [9820] closetest: child 5: CTRL_CLOSE_EVENT received, exiting...
        15.43600082 [7008] closetest: child 7: CTRL_CLOSE_EVENT received, pausing...
        15.69347382 [7008] closetest: child 7: CTRL_CLOSE_EVENT received, exiting...
        15.69517517 [6040] closetest: child 9: ReadFile() returned, exiting...
        15.69644451 [9148] closetest: child 8: CTRL_CLOSE_EVENT received, pausing...
        15.95952320 [9148] closetest: child 8: CTRL_CLOSE_EVENT received, exiting...
        19.46793747 [15356] closetest: child 10: CTRL_CLOSE_EVENT received, pausing...
        19.72507477 [15356] closetest: child 10: CTRL_CLOSE_EVENT received, exiting...
        19.72667694 [9028] closetest: child 12: ReadFile() returned, exiting...
        19.72741890 [1068] closetest: child 11: CTRL_CLOSE_EVENT received, pausing...
        19.99090767 [1068] closetest: child 11: CTRL_CLOSE_EVENT received, exiting...
EXAMPLE 3: race condition between process cleanup and close signaling
    Run closetest.exe with triggering order equal to the console signaling
    order (backward for v1 or Windows <= 14393, forward for 15063 v2):
        closetest.exe -d backward/forward -m job -n 50
    Try to close the console.  Sometimes only one process exits each time the
    Close button is closed, sometimes all of them exit, and sometimes several
    exit.
    The `--alloc` setting defaults to 0.  When I tested in a VM, increasing the
    value increased the frequency at which the console got "stuck" closing
    processes.  At 10, it sticks every few processes.  At 100, each Close click
    only kills one pair of processes:
        closetest.exe -d backward/forward -m job -n 2 --alloc 100
    I've reproduced this race on Win 8.1 and Win 10.0.15063.332.
    Example on 15063 v2 console (closetest.exe -d forward -m job -n 10 --alloc 1):
        0.11303009  [13236] closetest: child 1: attached to console (child 1 kills child 2)
        0.14890400  [12104] closetest: child 2: attached to console
        0.16104582  [12504] closetest: child 3: attached to console (child 3 kills child 4)
        0.20047462  [13064] closetest: child 4: attached to console
        0.21102031  [9744] closetest: child 5: attached to console (child 5 kills child 6)
        0.25124130  [13380] closetest: child 6: attached to console
        0.27692872  [13016] closetest: child 7: attached to console (child 7 kills child 8)
        0.29775897  [16036] closetest: child 8: attached to console
        0.33673882  [6284] closetest: child 9: attached to console (child 9 kills child 10)
        0.34709904  [13544] closetest: child 10: attached to console
        0.36960980  [13080] closetest: child 11: attached to console (child 11 kills child 12)
        0.39724991  [13656] closetest: child 12: attached to console
        0.43287444  [13524] closetest: child 13: attached to console (child 13 kills child 14)
        0.44224671  [13716] closetest: child 14: attached to console
        0.47673470  [13820] closetest: child 15: attached to console (child 15 kills child 16)
        0.49054298  [14792] closetest: child 16: attached to console
        0.50006652  [15448] closetest: child 17: attached to console (child 17 kills child 18)
        0.53570819  [13764] closetest: child 18: attached to console
        0.54578292  [14000] closetest: child 19: attached to console (child 19 kills child 20)
        0.55962348  [14040] closetest: child 20: attached to console
        0.57240164  [16336] closetest: attached process list: 16336, 13236, 12104, 12504, 13064, 9744, 13380, 13016, 16036, 6284, 13544, 13080, 13656, 13524, 13716, 13820, 14792, 15448, 13764, 14000, 14040
        ... wait a moment before clicking Close ...
        8.84291363  [13236] closetest: child 1: CTRL_CLOSE_EVENT received, pausing...
        9.10296917  [13236] closetest: child 1: CTRL_CLOSE_EVENT received, exiting...
        9.10612774  [12504] closetest: child 3: CTRL_CLOSE_EVENT received, pausing...
        9.36894321  [12504] closetest: child 3: CTRL_CLOSE_EVENT received, exiting...
        9.37183952  [9744] closetest: child 5: CTRL_CLOSE_EVENT received, pausing...
        9.63473129  [9744] closetest: child 5: CTRL_CLOSE_EVENT received, exiting...
        9.63787746  [13016] closetest: child 7: CTRL_CLOSE_EVENT received, pausing...
        9.89983940  [13016] closetest: child 7: CTRL_CLOSE_EVENT received, exiting...
        ... the close operation stops here, wait a bit before clicking Close again ...
        13.31911087 [6284] closetest: child 9: CTRL_CLOSE_EVENT received, pausing...
        13.57188797 [6284] closetest: child 9: CTRL_CLOSE_EVENT received, exiting...
        13.57480621 [13080] closetest: child 11: CTRL_CLOSE_EVENT received, pausing...
        13.83760452 [13080] closetest: child 11: CTRL_CLOSE_EVENT received, exiting...
        13.84090614 [13524] closetest: child 13: CTRL_CLOSE_EVENT received, pausing...
        14.10908413 [13524] closetest: child 13: CTRL_CLOSE_EVENT received, exiting...
        14.11201859 [13820] closetest: child 15: CTRL_CLOSE_EVENT received, pausing...
        14.36907291 [13820] closetest: child 15: CTRL_CLOSE_EVENT received, exiting...
        14.37195015 [15448] closetest: child 17: CTRL_CLOSE_EVENT received, pausing...
        14.63435650 [15448] closetest: child 17: CTRL_CLOSE_EVENT received, exiting...
        14.63759804 [14000] closetest: child 19: CTRL_CLOSE_EVENT received, pausing...
        14.89994526 [14000] closetest: child 19: CTRL_CLOSE_EVENT received, exiting...
*/

#ifdef _MSC_VER
#pragma comment(lib, "Shell32.lib")
#endif

#include <windows.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <locale.h>

#include <algorithm>
#include <array>
#include <deque>
#include <initializer_list>
#include <string>
#include <vector>
#include <chrono>
#include <thread>

static std::wstring g_pipestr;
static HANDLE g_hLogging = INVALID_HANDLE_VALUE;
static int g_childNum;

static const wchar_t* const kChildDivider = L"--";
static const wchar_t* const kChildCommand_Job = L"j";
static const wchar_t* const kChildCommand_Read = L"r";
static const wchar_t* const kChildCommand_Hold = L"h";

struct PipeHandles
{
    HANDLE rh;
    HANDLE wh;
};

static PipeHandles createPipe()
{
    PipeHandles ret{};
    const BOOL success = CreatePipe(&ret.rh, &ret.wh, nullptr, 0);
    UNREFERENCED_PARAMETER(success); // to make release builds happy.
    assert(success && "CreatePipe failed");
    return ret;
}

static HANDLE makeJob()
{
    HANDLE job = CreateJobObjectW(nullptr, nullptr);
    assert(job);
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION info{};
    info.BasicLimitInformation.LimitFlags =
        JOB_OBJECT_LIMIT_BREAKAWAY_OK |
        JOB_OBJECT_LIMIT_SILENT_BREAKAWAY_OK |
        JOB_OBJECT_LIMIT_DIE_ON_UNHANDLED_EXCEPTION |
        JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    BOOL success = SetInformationJobObject(job,
                                           JobObjectExtendedLimitInformation,
                                           &info,
                                           sizeof(info));
    UNREFERENCED_PARAMETER(success); // to make release builds happy.
    assert(success && "SetInformationJobObject failed");
    return job;
}

static std::wstring exeName()
{
    std::array<wchar_t, 4096> self{};
    DWORD len = GetModuleFileNameW(nullptr, self.data(), (DWORD)self.size());
    UNREFERENCED_PARAMETER(len); // to make release builds happy.
    assert(len >= 1 && len < self.size() && "GetModuleFileNameW failed");
    return self.data();
}

#if defined(__GNUC__)
#define PL_PRINTF_FORMAT(fmtarg, vararg) __attribute__((format(ms_printf, (fmtarg), ((vararg)))))
#else
#define PL_PRINTF_FORMAT(fmtarg, vararg)
#endif

#define TRACE(fmt, ...) trace("closetest: " fmt, ##__VA_ARGS__)
static void trace(const char* fmt, ...) PL_PRINTF_FORMAT(1, 2);
static void trace(const char* fmt, ...)
{
    std::array<char, 4096> buf;
    int written = 0;
    va_list ap;
    va_start(ap, fmt);
    written = vsnprintf(buf.data(), buf.size(), fmt, ap);
    va_end(ap);

    assert(written < 4094);

    // terminate string with \r\n so remote side can see it.
    buf[written] = '\r';
    buf[written + 1] = '\n';
    if (g_hLogging != INVALID_HANDLE_VALUE)
    {
        WriteFile(g_hLogging, buf.data(), (DWORD)written + 2, nullptr, nullptr);
    }

    OutputDebugStringA(buf.data());
}

static std::vector<DWORD> getConsoleProcessList()
{
    std::vector<DWORD> ret;
    ret.resize(1);
    const DWORD count1 = GetConsoleProcessList(&ret[0], (DWORD)ret.size());
    assert(count1 >= 1 && "GetConsoleProcessList failed");
    ret.resize(count1);
    const DWORD count2 = GetConsoleProcessList(&ret[0], (DWORD)ret.size());
    assert(count1 == count2 && "GetConsoleProcessList failed");
    return ret;
}

static void dumpConsoleProcessList()
{
    std::string msg;
    for (DWORD pid : getConsoleProcessList())
    {
        if (!msg.empty())
        {
            msg += ", ";
        }
        msg += std::to_string(pid);
    }
    TRACE("attached process list: %s", msg.c_str());
}

static std::wstring argvToCommandLine(const std::vector<std::wstring>& argv)
{
    std::wstring ret;
    for (const auto& arg : argv)
    {
        // Strictly incorrect, but good enough.
        if (!ret.empty())
        {
            ret.push_back(L' ');
        }
        const bool quote = arg.empty() || arg.find(L' ') != std::wstring::npos;
        if (quote)
        {
            ret.push_back(L'\"');
        }
        ret.append(arg);
        if (quote)
        {
            ret.push_back(L'\"');
        }
    }
    return ret;
}

static void spawnChildTree(DWORD masterPid, const std::vector<std::wstring>& extraArgs)
{
    if (extraArgs.empty())
    {
        return;
    }

    const HANDLE readyEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);
    assert(readyEvent != nullptr);

    std::vector<std::wstring> argv;
    argv.push_back(exeName());
    argv.push_back(L"--child");
    argv.push_back(std::to_wstring(masterPid));
    argv.push_back(std::to_wstring(GetCurrentProcessId()));
    argv.push_back(std::to_wstring((uintptr_t)readyEvent));

    if (g_hLogging != INVALID_HANDLE_VALUE)
    {
        argv.push_back(g_pipestr);
    }

    argv.insert(argv.end(), extraArgs.begin(), extraArgs.end());
    auto cmdline = argvToCommandLine(argv);

    //TRACE("spawning: %ls", cmdline.c_str());

    BOOL success{};
    STARTUPINFOW sui{ sizeof(sui) };
    PROCESS_INFORMATION pi{};
    success = CreateProcessW(exeName().c_str(), &cmdline[0], nullptr, nullptr, FALSE, 0, nullptr, nullptr, &sui, &pi);
    assert(success && "CreateProcessW failed");

    const DWORD waitRet = WaitForSingleObject(readyEvent, INFINITE);
    assert(waitRet == WAIT_OBJECT_0 && "WaitForSingleObject failed");
    CloseHandle(readyEvent);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
}

// Split args up into children and spawn each child as a sibling.
static void spawnSiblings(DWORD masterPid, const std::vector<std::wstring>& args)
{
    auto it = args.begin();
    while (it != args.end())
    {
        assert(*it == kChildDivider);
        const auto itEnd = std::find(it + 1, args.end(), kChildDivider);
        const std::vector<std::wstring> child(it, itEnd);
        spawnChildTree(masterPid, child);
        it = itEnd;
    }
}

static void genChild(int n, const std::wstring& desc, int allocChunk, std::vector<std::wstring>& out)
{
    assert(desc != kChildDivider); // A divider as the desc would break spawnSiblings's parsing.
    out.push_back(kChildDivider);
    out.push_back(std::to_wstring(n));
    out.push_back(desc);
    out.push_back(std::to_wstring(allocChunk / 1024));
}

static void genBatch(bool forward,
                     bool useJob,
                     bool useGapProcess,
                     int allocChunk,
                     std::vector<std::wstring>& out,
                     std::vector<HANDLE>& handles)
{
    static int cnt = 1;

    const PipeHandles pipe = createPipe();
    const HANDLE job = makeJob();
    handles.push_back(pipe.rh);
    handles.push_back(pipe.wh);
    handles.push_back(job);

    auto genVictim = [&](int n, int /*n2*/) {
        genChild(n, L"", allocChunk, out);
        if (useJob)
        {
            out.push_back(kChildCommand_Job);
            out.push_back(std::to_wstring((uintptr_t)job));
        }
        else
        {
            out.push_back(kChildCommand_Read);
            out.push_back(std::to_wstring((uintptr_t)pipe.rh));
        }
    };

    auto genKiller = [&](int n, int n2) {
        const auto desc = L"child " + std::to_wstring(n) +
                          L" kills child " + std::to_wstring(n2);
        genChild(n, desc, allocChunk, out);
        out.push_back(kChildCommand_Hold);
        out.push_back(std::to_wstring((uintptr_t)(useJob ? job : pipe.wh)));
    };

    const int first = cnt;
    const int gap = cnt + 1;
    const int second = cnt + 1 + useGapProcess;

    if (forward)
    {
        genKiller(first, second);
        if (useGapProcess)
        {
            genChild(gap, L"", allocChunk, out);
        }
        genVictim(second, first);
    }
    else
    {
        genVictim(first, second);
        if (useGapProcess)
        {
            genChild(gap, L"", allocChunk, out);
        }
        genKiller(second, first);
    }
    cnt += 2 + useGapProcess;
}

static BOOL WINAPI ctrlHandler(DWORD type)
{
    if (type == CTRL_CLOSE_EVENT)
    {
        TRACE("child %d: CTRL_CLOSE_EVENT received, pausing...", g_childNum);
        Sleep(250);
        TRACE("child %d: CTRL_CLOSE_EVENT received, exiting...", g_childNum);
        return TRUE;
    }
    return FALSE;
}

// Duplicate a handle from `srcProc` into a non-inheritable handle in the
// current process.
static HANDLE duplicateHandle(HANDLE srcProc, HANDLE srcHandle)
{
    HANDLE ret{};
    const auto success =
        DuplicateHandle(srcProc, srcHandle, GetCurrentProcess(), &ret, 0, FALSE, DUPLICATE_SAME_ACCESS);
    assert(success && "DuplicateHandle failed");
    return ret;
}

static HANDLE openProcess(DWORD pid)
{
    const HANDLE ret = OpenProcess(PROCESS_DUP_HANDLE, FALSE, pid);
    assert(ret != nullptr && "OpenProcess failed");
    return ret;
}

static std::deque<std::wstring> getCommandLine()
{
    // Link with Shell32.lib for CommandLineToArgvW.
    std::deque<std::wstring> ret;
    const auto cmdline = GetCommandLineW();
    int argc{};
    const auto argv = CommandLineToArgvW(cmdline, &argc);
    for (int i = 0; i < argc; ++i)
    {
        ret.push_back(argv[i]);
    }
    LocalFree((HLOCAL)argv);
    return ret;
}

template<typename T>
static T shift(std::deque<T>& container)
{
    assert(!container.empty());
    T ret = container.front();
    container.pop_front();
    return ret;
}

static int shiftInt(std::deque<std::wstring>& container)
{
    return _wtoi(shift(container).c_str());
}

static HANDLE shiftHandle(std::deque<std::wstring>& container)
{
    return (HANDLE)(uintptr_t)shiftInt(container);
}

static int doChild(std::deque<std::wstring> argv)
{
    // closetest.exe --child <masterPid> <parentPid> <readyEvent> <opt:pipeName> -- <num> <desc> <alloc> [cmd arg] [...]
    shift(argv);
    assert(shift(argv) == L"--child");
    const DWORD masterPid = shiftInt(argv);
    const DWORD parentPid = shiftInt(argv);
    HANDLE masterProc = openProcess(masterPid);
    HANDLE parentProc = openProcess(parentPid);
    HANDLE readyEvent = duplicateHandle(parentProc, shiftHandle(argv));

    auto optPipeName = shift(argv);
    if (optPipeName != kChildDivider)
    {
        std::wstring fullPipeName(L"\\\\.\\pipe\\");
        fullPipeName.append(optPipeName);
        /*TRACE("child %d: log pipe %ls", fullPipeName.c_str());*/
        g_hLogging = CreateFileW(fullPipeName.c_str(), GENERIC_WRITE, FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
        assert(g_hLogging != INVALID_HANDLE_VALUE);
        g_pipestr = optPipeName;

        const auto childKeyword = shift(argv);
        assert(childKeyword == kChildDivider);
    }

    g_childNum = shiftInt(argv);
    const auto desc = shift(argv);
    const size_t allocChunk = shiftInt(argv) * 1024;

    SetConsoleCtrlHandler(ctrlHandler, TRUE);

    TRACE("child %d: attached to console%ls", g_childNum, desc.empty() ? L"" : (std::wstring(L" (") + desc + L")").c_str());

    if (allocChunk > 0)
    {
        // Slow process termination down by allocating a chunk of memory.
        char* buf = new char[allocChunk];
        memset(buf, 0xcc, allocChunk);
    }

    HANDLE readHandle = nullptr;
    HANDLE jobHandle = nullptr;

    while (!argv.empty() && argv.front() != kChildDivider)
    {
        const auto cmd = shift(argv);
        if (cmd == kChildCommand_Hold)
        {
            // Duplicate the handle into this process, then forget it.
            duplicateHandle(masterProc, shiftHandle(argv));
        }
        else if (cmd == kChildCommand_Read)
        {
            assert(readHandle == nullptr);
            readHandle = duplicateHandle(masterProc, shiftHandle(argv));
        }
        else if (cmd == kChildCommand_Job)
        {
            assert(jobHandle == nullptr);
            jobHandle = duplicateHandle(masterProc, shiftHandle(argv));
        }
        else
        {
            TRACE("Invalid child command: %ls", cmd.c_str());
            exit(1);
        }
    }

    spawnChildTree(masterPid, std::vector<std::wstring>(argv.begin(), argv.end()));

    // Assign self to a job object.
    if (jobHandle != nullptr)
    {
        const BOOL success =
            AssignProcessToJobObject(jobHandle, GetCurrentProcess());
        assert(success && "AssignProcessToJobObject failed");
        CloseHandle(jobHandle);
        jobHandle = nullptr;
    }

    CloseHandle(masterProc);
    masterProc = nullptr;
    CloseHandle(parentProc);
    parentProc = nullptr;

    // Signal the parent once all the subprocesses are spawned.
    BOOL success = SetEvent(readyEvent);
    UNREFERENCED_PARAMETER(success); // to make release builds happy.
    assert(success && "SetEvent failed");
    CloseHandle(readyEvent);
    readyEvent = nullptr;

    if (readHandle != nullptr)
    {
        char buf{};
        DWORD actual{};
        ReadFile(readHandle, &buf, sizeof(buf), &actual, nullptr);
        TRACE("child %d: ReadFile() returned, exiting...", g_childNum);
    }
    else
    {
        Sleep(300 * 1000);
    }

    return 0;
}

static void usage()
{
    printf("usage: %ls [options]\n", exeName().c_str());
    printf("Options:\n");
    printf("  -n NUM_BATCHES    Start NUM_BATCHES batches of processes [default: 4]\n");
    printf("  -d DIR            Set direction of process killing\n");
    printf("                       forward: early process kills later process\n");
    printf("                       backward: vice versa\n");
    printf("                       alternate: alternate between forward/backward\n");
    printf("                       none: no triggered process killing [default]\n");
    printf("  --gap             Create a gap process between killer and target\n");
    printf("  --no-gap          Disable the gap process [default]\n");
    printf("  --alloc SZ        Allocate an SZ MiB buffer in each child [default: 0]\n");
    printf("  -m METHOD         Method used to kill processes\n");
    printf("                       pipe [default]\n");
    printf("                       job\n");
    printf("  --log PIPENAME    Log output to a named pipe\n");
    printf("  --graph GRAPH     Process graph:\n");
    printf("                       tree: degenerate tree of processes [default]\n");
    printf("                       list: all processes are siblings\n");
    printf("  --delay TIME      Time in milliseconds to delay starting the test\n");
    printf("  --no-realloc      Skip free/alloc console to break out of the initial session\n");
}

static int doParent(std::deque<std::wstring> argv)
{
    int numBatches = 4;
    int dir = 0;
    bool useJob = false;
    bool useGapProcess = false;
    int allocChunk = 0;
    bool useSiblings = false;
    bool noRealloc = false;

    // Parse arguments
    shift(argv); // discard the program name.
    while (!argv.empty())
    {
        const auto arg = shift(argv);
        const auto hasNext = !argv.empty();

        if (arg == L"--help" || arg == L"-h")
        {
            usage();
            exit(0);
        }
        else if (arg == L"-n" && hasNext)
        {
            numBatches = shiftInt(argv);
        }
        else if (arg == L"-d" && hasNext)
        {
            const auto next = shift(argv);
            if (next == L"forward")
            {
                dir = 1;
            }
            else if (next == L"backward")
            {
                dir = 2;
            }
            else if (next == L"alternate")
            {
                dir = 3;
            }
            else if (next == L"none")
            {
                dir = 0;
            }
            else
            {
                fprintf(stderr, "error: unrecognized -d argument: %ls\n", next.c_str());
                exit(1);
            }
        }
        else if (arg == L"--gap")
        {
            useGapProcess = true;
        }
        else if (arg == L"--no-gap")
        {
            useGapProcess = false;
        }
        else if (arg == L"--alloc" && hasNext)
        {
            const auto next = shift(argv);
            allocChunk = (int)(_wtof(next.c_str()) * 1024.0 * 1024.0);
        }
        else if (arg == L"-m" && hasNext)
        {
            const auto next = shift(argv);
            if (next == L"pipe")
            {
                useJob = false;
            }
            else if (next == L"job")
            {
                useJob = true;
            }
            else
            {
                fprintf(stderr, "error: unrecognized -m argument: %ls\n", next.c_str());
                exit(1);
            }
        }
        else if (arg == L"--graph" && hasNext)
        {
            const auto next = shift(argv);
            if (next == L"tree")
            {
                useSiblings = false;
            }
            else if (next == L"list")
            {
                useSiblings = true;
            }
            else
            {
                fprintf(stderr, "error: unrecognized --graph argument: %ls\n", next.c_str());
                exit(1);
            }
        }
        else if (arg == L"--log" && hasNext)
        {
            const auto next = shift(argv);
            std::wstring pipename(L"\\\\.\\pipe\\");
            pipename.append(next);

            g_hLogging = CreateFileW(pipename.c_str(), GENERIC_WRITE, FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
            if (g_hLogging != INVALID_HANDLE_VALUE)
            {
                g_pipestr = next;
            }
            else
            {
                fprintf(stderr, "error: pipe cannot be opened: %ls\n", next.c_str());
                exit(1);
            }
        }
        else if (arg == L"--delay" && hasNext)
        {
            const auto next = shift(argv);
            const long ms = std::stol(next);
            std::this_thread::sleep_for(std::chrono::milliseconds(ms));
        }
        else if (arg == L"--no-realloc")
        {
            noRealloc = true;
        }
        else
        {
            usage();
            fprintf(stderr, "\nerror: unrecognized argument: %ls\n", arg.c_str());
            exit(1);
        }
    }

    // Decide which children to start.
    std::vector<std::wstring> spawnList;
    std::vector<HANDLE> handles;
    for (int i = 0; i < numBatches; ++i)
    {
        if (dir == 0)
        {
            genChild(i + 1, L"", allocChunk, spawnList);
        }
        if (dir & 1)
        {
            genBatch(true, useJob, useGapProcess, allocChunk, spawnList, handles);
        }
        if (dir & 2)
        {
            genBatch(false, useJob, useGapProcess, allocChunk, spawnList, handles);
        }
    }

    // Spawn the children.
    if (!noRealloc)
    {
        FreeConsole();
        AllocConsole();
    }

    if (useSiblings)
    {
        spawnSiblings(GetCurrentProcessId(), spawnList);
    }
    else
    {
        spawnChildTree(GetCurrentProcessId(), spawnList);
    }
    for (auto h : handles)
    {
        CloseHandle(h);
    }

    // Wait until we're killed.
    dumpConsoleProcessList();
    Sleep(300 * 1000);

    return 0;
}

int main()
{
    setlocale(LC_ALL, "");
    auto argv = getCommandLine();
    if (argv.size() >= 2 && argv[1] == L"--child")
    {
        return doChild(std::move(argv));
    }
    else
    {
        return doParent(std::move(argv));
    }
}
