// A minimal Win32 terminal — Windows 95 conhost style.
// Fixed-size char32_t grid, GDI rendering, no Unicode shaping, no scrollback.
// Implements IPtyHost and uses VtParser to interpret output from the server.

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <wil/com.h>
#include <wil/resource.h>

#include <algorithm>
#include <atomic>
#include <string>
#include <thread>

#include <conpty.h>
#include "../VtParser.h"

// ============================================================================
// Terminal grid
// ============================================================================

static constexpr SHORT COLS = 120;
static constexpr SHORT ROWS = 30;
static constexpr int CELL_W = 8;
static constexpr int CELL_H = 16;

struct Cell
{
    char32_t ch = ' ';
    WORD attr = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
};

struct TermState
{
    Cell grid[ROWS][COLS]{};
    SHORT cursorX = 0;
    SHORT cursorY = 0;
    WORD currentAttr = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
    bool cursorVisible = true;
    std::wstring title = L"conpty-test";

    COLORREF colorTable[16] = {
        0x000000, 0x800000, 0x008000, 0x808000, 0x000080, 0x800080, 0x008080, 0xC0C0C0,
        0x808080, 0xFF0000, 0x00FF00, 0xFFFF00, 0x0000FF, 0xFF00FF, 0x00FFFF, 0xFFFFFF,
    };

    void scrollUp()
    {
        memmove(&grid[0], &grid[1], sizeof(Cell) * COLS * (ROWS - 1));
        for (SHORT x = 0; x < COLS; x++)
            grid[ROWS - 1][x] = Cell{};
    }

    void advanceCursor()
    {
        cursorX++;
        if (cursorX >= COLS)
        {
            cursorX = 0;
            cursorY++;
            if (cursorY >= ROWS)
            {
                cursorY = ROWS - 1;
                scrollUp();
            }
        }
    }

    void linefeed()
    {
        cursorY++;
        if (cursorY >= ROWS)
        {
            cursorY = ROWS - 1;
            scrollUp();
        }
    }

    void putChar(char32_t ch)
    {
        if (cursorX < COLS && cursorY < ROWS)
        {
            grid[cursorY][cursorX] = { ch, currentAttr };
            advanceCursor();
        }
    }

    void eraseDisplay(int mode)
    {
        const Cell blank = { ' ', currentAttr };
        if (mode == 0)
        {
            for (SHORT x = cursorX; x < COLS; x++) grid[cursorY][x] = blank;
            for (SHORT y = cursorY + 1; y < ROWS; y++)
                for (SHORT x = 0; x < COLS; x++) grid[y][x] = blank;
        }
        else if (mode == 1)
        {
            for (SHORT y = 0; y < cursorY; y++)
                for (SHORT x = 0; x < COLS; x++) grid[y][x] = blank;
            for (SHORT x = 0; x <= cursorX && x < COLS; x++) grid[cursorY][x] = blank;
        }
        else if (mode == 2 || mode == 3)
        {
            for (SHORT y = 0; y < ROWS; y++)
                for (SHORT x = 0; x < COLS; x++)
                    grid[y][x] = blank;
        }
    }

    void eraseLine(int mode)
    {
        const Cell blank = { ' ', currentAttr };
        SHORT start = 0, end = COLS;
        if (mode == 0) start = cursorX;
        else if (mode == 1) end = cursorX + 1;
        for (SHORT x = start; x < end; x++)
            grid[cursorY][x] = blank;
    }
};

// ============================================================================
// Globals
// ============================================================================

static TermState g_term;
static VtParser g_vtParser;
static HWND g_hwnd = nullptr;
static wil::com_ptr<IPtyServer> g_server;
static CRITICAL_SECTION g_lock;

static void invalidate()
{
    if (g_hwnd)
        InvalidateRect(g_hwnd, nullptr, FALSE);
}

// ============================================================================
// VT output interpreter
// ============================================================================

static COLORREF attrToFg(WORD attr)
{
    return g_term.colorTable[attr & 0x0F];
}

static COLORREF attrToBg(WORD attr)
{
    return g_term.colorTable[(attr >> 4) & 0x0F];
}

static void parseSGR(const VtCsi& csi)
{
    if (csi.paramCount == 0)
    {
        g_term.currentAttr = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
        return;
    }

    // VT→Console color index: VT uses RGB bit order, Console uses BGR.
    static constexpr WORD vtToConsole[] = { 0, 4, 2, 6, 1, 5, 3, 7 };

    for (size_t i = 0; i < csi.paramCount; i++)
    {
        const auto p = csi.params[i];
        switch (p)
        {
        case 0: g_term.currentAttr = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE; break;
        case 1: g_term.currentAttr |= FOREGROUND_INTENSITY; break;
        case 7: g_term.currentAttr |= COMMON_LVB_REVERSE_VIDEO; break;
        case 22: g_term.currentAttr &= ~FOREGROUND_INTENSITY; break;
        case 27: g_term.currentAttr &= ~COMMON_LVB_REVERSE_VIDEO; break;
        case 39: g_term.currentAttr = (g_term.currentAttr & ~0x0F) | 0x07; break;
        case 49: g_term.currentAttr &= ~0xF0; break;
        default:
            if (p >= 30 && p <= 37)
                g_term.currentAttr = (g_term.currentAttr & ~0x07) | vtToConsole[p - 30];
            else if (p >= 40 && p <= 47)
                g_term.currentAttr = (g_term.currentAttr & ~0x70) | (vtToConsole[p - 40] << 4);
            else if (p >= 90 && p <= 97)
                g_term.currentAttr = (g_term.currentAttr & ~0x0F) | vtToConsole[p - 90] | FOREGROUND_INTENSITY;
            else if (p >= 100 && p <= 107)
                g_term.currentAttr = (g_term.currentAttr & ~0xF0) | (vtToConsole[p - 100] << 4) | BACKGROUND_INTENSITY;
            break;
        }
    }
}

static void handleCsiOutput(const VtCsi& csi)
{
    const auto p0 = (csi.paramCount >= 1) ? csi.params[0] : 0;
    const auto p1 = (csi.paramCount >= 2) ? csi.params[1] : 0;

    switch (csi.finalByte)
    {
    case 'A': g_term.cursorY = std::max<SHORT>(0, g_term.cursorY - std::max<SHORT>(1, (SHORT)p0)); break;
    case 'B': g_term.cursorY = std::min<SHORT>(ROWS - 1, g_term.cursorY + std::max<SHORT>(1, (SHORT)p0)); break;
    case 'C': g_term.cursorX = std::min<SHORT>(COLS - 1, g_term.cursorX + std::max<SHORT>(1, (SHORT)p0)); break;
    case 'D': g_term.cursorX = std::max<SHORT>(0, g_term.cursorX - std::max<SHORT>(1, (SHORT)p0)); break;
    case 'H':
    case 'f':
        g_term.cursorY = std::clamp<SHORT>(p0 > 0 ? (SHORT)(p0 - 1) : 0, 0, ROWS - 1);
        g_term.cursorX = std::clamp<SHORT>(p1 > 0 ? (SHORT)(p1 - 1) : 0, 0, COLS - 1);
        break;
    case 'J': g_term.eraseDisplay(p0); break;
    case 'K': g_term.eraseLine(p0); break;
    case 'm': parseSGR(csi); break;
    case 'h':
        if (csi.privateByte == '?' && p0 == 25) g_term.cursorVisible = true;
        break;
    case 'l':
        if (csi.privateByte == '?' && p0 == 25) g_term.cursorVisible = false;
        break;
    default: break;
    }
}

static void processOutput(std::string_view text)
{
    auto stream = g_vtParser.parse(text);
    VtToken token;

    while (stream.next(token))
    {
        switch (token.type)
        {
        case VtToken::Text:
        {
            const auto* bytes = reinterpret_cast<const uint8_t*>(token.payload.data());
            size_t i = 0;
            while (i < token.payload.size())
            {
                uint32_t cp;
                size_t seqLen;
                const auto b = bytes[i];
                if (b < 0x80) { cp = b; seqLen = 1; }
                else if ((b & 0xE0) == 0xC0) { cp = b & 0x1F; seqLen = 2; }
                else if ((b & 0xF0) == 0xE0) { cp = b & 0x0F; seqLen = 3; }
                else if ((b & 0xF8) == 0xF0) { cp = b & 0x07; seqLen = 4; }
                else { i++; continue; }
                if (i + seqLen > token.payload.size()) break;
                for (size_t j = 1; j < seqLen; j++)
                    cp = (cp << 6) | (bytes[i + j] & 0x3F);
                i += seqLen;
                g_term.putChar(cp);
            }
            break;
        }
        case VtToken::Ctrl:
            switch (token.ch)
            {
            case '\r': g_term.cursorX = 0; break;
            case '\n': g_term.linefeed(); break;
            case '\b': if (g_term.cursorX > 0) g_term.cursorX--; break;
            case '\t': g_term.cursorX = std::min<SHORT>(COLS - 1, (g_term.cursorX + 8) & ~7); break;
            case '\a': MessageBeep(MB_OK); break;
            default: break;
            }
            break;
        case VtToken::Csi:
            handleCsiOutput(*token.csi);
            break;
        case VtToken::Osc:
            if (!token.partial && token.payload.size() >= 2 &&
                (token.payload[0] == '0' || token.payload[0] == '2') && token.payload[1] == ';')
            {
                auto data = token.payload.substr(2);
                const auto wLen = MultiByteToWideChar(CP_UTF8, 0, data.data(), (int)data.size(), nullptr, 0);
                if (wLen > 0)
                {
                    g_term.title.resize(wLen);
                    MultiByteToWideChar(CP_UTF8, 0, data.data(), (int)data.size(), g_term.title.data(), wLen);
                    if (g_hwnd) SetWindowTextW(g_hwnd, g_term.title.c_str());
                }
            }
            break;
        default: break;
        }
    }
    invalidate();
}

// ============================================================================
// IPtyHost implementation
// ============================================================================

struct TestHost : IPtyHost
{
    HRESULT QueryInterface(const IID& riid, void** ppvObject) override
    {
        if (!ppvObject) return E_POINTER;
        if (riid == __uuidof(IPtyHost) || riid == __uuidof(IUnknown))
        {
            *ppvObject = static_cast<IPtyHost*>(this);
            AddRef();
            return S_OK;
        }
        *ppvObject = nullptr;
        return E_NOINTERFACE;
    }

    ULONG AddRef() override { return m_refCount.fetch_add(1) + 1; }
    ULONG Release() override
    {
        const auto c = m_refCount.fetch_sub(1) - 1;
        if (c == 0) delete this;
        return c;
    }

    void WriteUTF8(PTY_UTF8_STRING text) override
    {
        EnterCriticalSection(&g_lock);
        processOutput({ text.data, text.length });
        LeaveCriticalSection(&g_lock);
    }

    void WriteUTF16(PTY_UTF16_STRING text) override
    {
        const auto len = WideCharToMultiByte(CP_UTF8, 0, text.data, (int)text.length, nullptr, 0, nullptr, nullptr);
        if (len > 0)
        {
            std::string utf8(len, '\0');
            WideCharToMultiByte(CP_UTF8, 0, text.data, (int)text.length, utf8.data(), len, nullptr, nullptr);
            EnterCriticalSection(&g_lock);
            processOutput(utf8);
            LeaveCriticalSection(&g_lock);
        }
    }

    HRESULT CreateBuffer(void** buffer) override { *buffer = nullptr; return E_NOTIMPL; }
    HRESULT ReleaseBuffer(void*) override { return S_OK; }
    HRESULT ActivateBuffer(void*) override { return S_OK; }

    HRESULT GetScreenBufferInfo(PTY_SCREEN_BUFFER_INFO* info) override
    {
        EnterCriticalSection(&g_lock);
        *info = {};
        info->Size = { COLS, ROWS };
        info->CursorPosition = { g_term.cursorX, g_term.cursorY };
        info->Attributes = g_term.currentAttr;
        info->Window = { 0, 0, COLS - 1, ROWS - 1 };
        info->MaximumWindowSize = { COLS, ROWS };
        info->CursorSize = 25;
        info->CursorVisible = g_term.cursorVisible;
        info->FontSize = { CELL_W, CELL_H };
        info->FontFamily = FF_MODERN | FIXED_PITCH;
        info->FontWeight = FW_NORMAL;
        wcscpy_s(info->FaceName, L"Terminal");
        memcpy(info->ColorTable, g_term.colorTable, sizeof(g_term.colorTable));
        LeaveCriticalSection(&g_lock);
        return S_OK;
    }

    HRESULT SetScreenBufferInfo(const PTY_SCREEN_BUFFER_INFO_CHANGE* change) override
    {
        EnterCriticalSection(&g_lock);
        if (change->CursorPosition)
        {
            g_term.cursorX = std::clamp(change->CursorPosition->X, SHORT(0), SHORT(COLS - 1));
            g_term.cursorY = std::clamp(change->CursorPosition->Y, SHORT(0), SHORT(ROWS - 1));
        }
        if (change->Attributes) g_term.currentAttr = *change->Attributes;
        if (change->CursorVisible) g_term.cursorVisible = *change->CursorVisible != 0;
        if (change->ColorTable) memcpy(g_term.colorTable, change->ColorTable, sizeof(g_term.colorTable));
        invalidate();
        LeaveCriticalSection(&g_lock);
        return S_OK;
    }

    HRESULT ReadBuffer(COORD pos, LONG count, PTY_CHAR_INFO* infos) override
    {
        EnterCriticalSection(&g_lock);
        for (LONG i = 0; i < count; i++)
        {
            SHORT x = pos.X + static_cast<SHORT>(i);
            SHORT y = pos.Y;
            while (x >= COLS && y < ROWS) { x -= COLS; y++; }
            if (y >= 0 && y < ROWS && x >= 0 && x < COLS)
            {
                infos[i].Char = static_cast<WCHAR>(g_term.grid[y][x].ch <= 0xFFFF ? g_term.grid[y][x].ch : L'?');
                infos[i].Attributes = g_term.grid[y][x].attr;
            }
            else
            {
                infos[i].Char = L' ';
                infos[i].Attributes = 0x07;
            }
        }
        LeaveCriticalSection(&g_lock);
        return S_OK;
    }

    HRESULT GetConsoleWindow(HWND* hwnd) override { *hwnd = g_hwnd; return S_OK; }

private:
    std::atomic<ULONG> m_refCount{ 1 };
};

// ============================================================================
// GDI rendering
// ============================================================================

static void paint(HWND hwnd, HDC hdc)
{
    EnterCriticalSection(&g_lock);

    RECT rc;
    GetClientRect(hwnd, &rc);
    const auto bgBrush = CreateSolidBrush(g_term.colorTable[0]);
    FillRect(hdc, &rc, bgBrush);
    DeleteObject(bgBrush);

    const auto font = CreateFontW(
        CELL_H, CELL_W, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        NONANTIALIASED_QUALITY, FIXED_PITCH | FF_MODERN, L"Terminal");
    const auto oldFont = SelectObject(hdc, font);
    SetBkMode(hdc, OPAQUE);

    for (SHORT y = 0; y < ROWS; y++)
    {
        for (SHORT x = 0; x < COLS; x++)
        {
            const auto& cell = g_term.grid[y][x];
            COLORREF fg, bg;
            if (cell.attr & COMMON_LVB_REVERSE_VIDEO)
            {
                fg = attrToBg(cell.attr);
                bg = attrToFg(cell.attr);
            }
            else
            {
                fg = attrToFg(cell.attr);
                bg = attrToBg(cell.attr);
            }

            SetTextColor(hdc, fg);
            SetBkColor(hdc, bg);

            wchar_t wch = static_cast<wchar_t>(cell.ch <= 0xFFFF ? cell.ch : L'?');
            if (wch < 0x20) wch = L' ';
            TextOutW(hdc, x * CELL_W, y * CELL_H, &wch, 1);
        }
    }

    if (g_term.cursorVisible)
    {
        RECT cur = { g_term.cursorX * CELL_W, g_term.cursorY * CELL_H + CELL_H - 2,
                     g_term.cursorX * CELL_W + CELL_W, g_term.cursorY * CELL_H + CELL_H };
        const auto curBrush = CreateSolidBrush(g_term.colorTable[7]);
        FillRect(hdc, &cur, curBrush);
        DeleteObject(curBrush);
    }

    SelectObject(hdc, oldFont);
    DeleteObject(font);
    LeaveCriticalSection(&g_lock);
}

// ============================================================================
// Window procedure
// ============================================================================

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        const auto hdc = BeginPaint(hwnd, &ps);
        paint(hwnd, hdc);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_CHAR:
    {
        wchar_t wch = static_cast<wchar_t>(wParam);
        char utf8[4];
        const auto len = WideCharToMultiByte(CP_UTF8, 0, &wch, 1, utf8, sizeof(utf8), nullptr, nullptr);
        if (len > 0 && g_server)
        {
            PTY_UTF8_STRING input{ utf8, static_cast<SIZE_T>(len) };
            g_server->WriteUTF8(input);
        }
        return 0;
    }
    case WM_KEYDOWN:
    {
        const char* seq = nullptr;
        switch (wParam)
        {
        case VK_UP:     seq = "\x1b[A"; break;
        case VK_DOWN:   seq = "\x1b[B"; break;
        case VK_RIGHT:  seq = "\x1b[C"; break;
        case VK_LEFT:   seq = "\x1b[D"; break;
        case VK_HOME:   seq = "\x1b[H"; break;
        case VK_END:    seq = "\x1b[F"; break;
        case VK_INSERT: seq = "\x1b[2~"; break;
        case VK_DELETE: seq = "\x1b[3~"; break;
        case VK_PRIOR:  seq = "\x1b[5~"; break;
        case VK_NEXT:   seq = "\x1b[6~"; break;
        case VK_F1:     seq = "\x1bOP"; break;
        case VK_F2:     seq = "\x1bOQ"; break;
        case VK_F3:     seq = "\x1bOR"; break;
        case VK_F4:     seq = "\x1bOS"; break;
        case VK_F5:     seq = "\x1b[15~"; break;
        case VK_F6:     seq = "\x1b[17~"; break;
        case VK_F7:     seq = "\x1b[18~"; break;
        case VK_F8:     seq = "\x1b[19~"; break;
        case VK_F9:     seq = "\x1b[20~"; break;
        case VK_F10:    seq = "\x1b[21~"; break;
        case VK_F11:    seq = "\x1b[23~"; break;
        case VK_F12:    seq = "\x1b[24~"; break;
        default: return DefWindowProcW(hwnd, msg, wParam, lParam);
        }
        if (seq && g_server)
        {
            PTY_UTF8_STRING input{ seq, strlen(seq) };
            g_server->WriteUTF8(input);
        }
        return 0;
    }
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
}

// ============================================================================
// Entry point
// ============================================================================

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR, int nCmdShow)
{
    InitializeCriticalSection(&g_lock);

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = L"ConPtyTestWindow";
    RegisterClassExW(&wc);

    RECT wr = { 0, 0, COLS * CELL_W, ROWS * CELL_H };
    AdjustWindowRect(&wr, WS_OVERLAPPEDWINDOW, FALSE);

    g_hwnd = CreateWindowExW(
        0, L"ConPtyTestWindow", L"conpty-test",
        WS_OVERLAPPEDWINDOW & ~(WS_THICKFRAME | WS_MAXIMIZEBOX),
        CW_USEDEFAULT, CW_USEDEFAULT,
        wr.right - wr.left, wr.bottom - wr.top,
        nullptr, nullptr, hInstance, nullptr);

    ShowWindow(g_hwnd, nCmdShow);
    UpdateWindow(g_hwnd);

    THROW_IF_FAILED(PtyCreateServer(IID_PPV_ARGS(g_server.addressof())));
    THROW_IF_FAILED(g_server->SetHost(new TestHost()));

    wil::unique_process_information pi;
    THROW_IF_FAILED(g_server->CreateProcessW(
        nullptr, _wcsdup(L"cmd.exe"),
        nullptr, nullptr, FALSE, 0, nullptr, nullptr,
        pi.addressof()));

    // Run the console server on a background thread.
    // It blocks in its message loop until all clients disconnect.
    std::thread serverThread([&] {
        g_server->Run();
        PostMessage(g_hwnd, WM_CLOSE, 0, 0);
    });
    serverThread.detach();

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    DeleteCriticalSection(&g_lock);
    return 0;
}
