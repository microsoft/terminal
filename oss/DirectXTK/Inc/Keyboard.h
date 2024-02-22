//--------------------------------------------------------------------------------------
// File: Keyboard.h
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
//
// http://go.microsoft.com/fwlink/?LinkId=248929
// http://go.microsoft.com/fwlink/?LinkID=615561
//--------------------------------------------------------------------------------------

#pragma once

#if (defined(WINAPI_FAMILY) && (WINAPI_FAMILY == WINAPI_FAMILY_APP)) || (defined(_XBOX_ONE) && defined(_TITLE))
#ifndef USING_COREWINDOW
#define USING_COREWINDOW
#endif
#endif

#include <cstdint>
#include <memory>

#ifdef USING_COREWINDOW
namespace ABI { namespace Windows { namespace UI { namespace Core { struct ICoreWindow; } } } }
#endif

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunknown-pragmas"
#endif


namespace DirectX
{
    class Keyboard
    {
    public:
        Keyboard() noexcept(false);

        Keyboard(Keyboard&&) noexcept;
        Keyboard& operator= (Keyboard&&) noexcept;

        Keyboard(Keyboard const&) = delete;
        Keyboard& operator=(Keyboard const&) = delete;

        virtual ~Keyboard();

        enum Keys : unsigned char
        {
            None = 0,

            Back = 0x8,
            Tab = 0x9,

            Enter = 0xd,

            Pause = 0x13,
            CapsLock = 0x14,
            Kana = 0x15,
            ImeOn = 0x16,

            Kanji = 0x19,

            ImeOff = 0x1a,
            Escape = 0x1b,
            ImeConvert = 0x1c,
            ImeNoConvert = 0x1d,

            Space = 0x20,
            PageUp = 0x21,
            PageDown = 0x22,
            End = 0x23,
            Home = 0x24,
            Left = 0x25,
            Up = 0x26,
            Right = 0x27,
            Down = 0x28,
            Select = 0x29,
            Print = 0x2a,
            Execute = 0x2b,
            PrintScreen = 0x2c,
            Insert = 0x2d,
            Delete = 0x2e,
            Help = 0x2f,
            D0 = 0x30,
            D1 = 0x31,
            D2 = 0x32,
            D3 = 0x33,
            D4 = 0x34,
            D5 = 0x35,
            D6 = 0x36,
            D7 = 0x37,
            D8 = 0x38,
            D9 = 0x39,

            A = 0x41,
            B = 0x42,
            C = 0x43,
            D = 0x44,
            E = 0x45,
            F = 0x46,
            G = 0x47,
            H = 0x48,
            I = 0x49,
            J = 0x4a,
            K = 0x4b,
            L = 0x4c,
            M = 0x4d,
            N = 0x4e,
            O = 0x4f,
            P = 0x50,
            Q = 0x51,
            R = 0x52,
            S = 0x53,
            T = 0x54,
            U = 0x55,
            V = 0x56,
            W = 0x57,
            X = 0x58,
            Y = 0x59,
            Z = 0x5a,
            LeftWindows = 0x5b,
            RightWindows = 0x5c,
            Apps = 0x5d,

            Sleep = 0x5f,
            NumPad0 = 0x60,
            NumPad1 = 0x61,
            NumPad2 = 0x62,
            NumPad3 = 0x63,
            NumPad4 = 0x64,
            NumPad5 = 0x65,
            NumPad6 = 0x66,
            NumPad7 = 0x67,
            NumPad8 = 0x68,
            NumPad9 = 0x69,
            Multiply = 0x6a,
            Add = 0x6b,
            Separator = 0x6c,
            Subtract = 0x6d,

            Decimal = 0x6e,
            Divide = 0x6f,
            F1 = 0x70,
            F2 = 0x71,
            F3 = 0x72,
            F4 = 0x73,
            F5 = 0x74,
            F6 = 0x75,
            F7 = 0x76,
            F8 = 0x77,
            F9 = 0x78,
            F10 = 0x79,
            F11 = 0x7a,
            F12 = 0x7b,
            F13 = 0x7c,
            F14 = 0x7d,
            F15 = 0x7e,
            F16 = 0x7f,
            F17 = 0x80,
            F18 = 0x81,
            F19 = 0x82,
            F20 = 0x83,
            F21 = 0x84,
            F22 = 0x85,
            F23 = 0x86,
            F24 = 0x87,

            NumLock = 0x90,
            Scroll = 0x91,

            LeftShift = 0xa0,
            RightShift = 0xa1,
            LeftControl = 0xa2,
            RightControl = 0xa3,
            LeftAlt = 0xa4,
            RightAlt = 0xa5,
            BrowserBack = 0xa6,
            BrowserForward = 0xa7,
            BrowserRefresh = 0xa8,
            BrowserStop = 0xa9,
            BrowserSearch = 0xaa,
            BrowserFavorites = 0xab,
            BrowserHome = 0xac,
            VolumeMute = 0xad,
            VolumeDown = 0xae,
            VolumeUp = 0xaf,
            MediaNextTrack = 0xb0,
            MediaPreviousTrack = 0xb1,
            MediaStop = 0xb2,
            MediaPlayPause = 0xb3,
            LaunchMail = 0xb4,
            SelectMedia = 0xb5,
            LaunchApplication1 = 0xb6,
            LaunchApplication2 = 0xb7,

            OemSemicolon = 0xba,
            OemPlus = 0xbb,
            OemComma = 0xbc,
            OemMinus = 0xbd,
            OemPeriod = 0xbe,
            OemQuestion = 0xbf,
            OemTilde = 0xc0,

            OemOpenBrackets = 0xdb,
            OemPipe = 0xdc,
            OemCloseBrackets = 0xdd,
            OemQuotes = 0xde,
            Oem8 = 0xdf,

            OemBackslash = 0xe2,

            ProcessKey = 0xe5,

            OemCopy = 0xf2,
            OemAuto = 0xf3,
            OemEnlW = 0xf4,

            Attn = 0xf6,
            Crsel = 0xf7,
            Exsel = 0xf8,
            EraseEof = 0xf9,
            Play = 0xfa,
            Zoom = 0xfb,

            Pa1 = 0xfd,
            OemClear = 0xfe,
        };

        struct State
        {
            bool Reserved0 : 8;
            bool Back : 1;              // VK_BACK, 0x8
            bool Tab : 1;               // VK_TAB, 0x9
            bool Reserved1 : 3;
            bool Enter : 1;             // VK_RETURN, 0xD
            bool Reserved2 : 2;
            bool Reserved3 : 3;
            bool Pause : 1;             // VK_PAUSE, 0x13
            bool CapsLock : 1;          // VK_CAPITAL, 0x14
            bool Kana : 1;              // VK_KANA, 0x15
            bool ImeOn : 1;             // VK_IME_ON, 0x16
            bool Reserved4 : 1;
            bool Reserved5 : 1;
            bool Kanji : 1;             // VK_KANJI, 0x19
            bool ImeOff : 1;            // VK_IME_OFF, 0X1A
            bool Escape : 1;            // VK_ESCAPE, 0x1B
            bool ImeConvert : 1;        // VK_CONVERT, 0x1C
            bool ImeNoConvert : 1;      // VK_NONCONVERT, 0x1D
            bool Reserved7 : 2;
            bool Space : 1;             // VK_SPACE, 0x20
            bool PageUp : 1;            // VK_PRIOR, 0x21
            bool PageDown : 1;          // VK_NEXT, 0x22
            bool End : 1;               // VK_END, 0x23
            bool Home : 1;              // VK_HOME, 0x24
            bool Left : 1;              // VK_LEFT, 0x25
            bool Up : 1;                // VK_UP, 0x26
            bool Right : 1;             // VK_RIGHT, 0x27
            bool Down : 1;              // VK_DOWN, 0x28
            bool Select : 1;            // VK_SELECT, 0x29
            bool Print : 1;             // VK_PRINT, 0x2A
            bool Execute : 1;           // VK_EXECUTE, 0x2B
            bool PrintScreen : 1;       // VK_SNAPSHOT, 0x2C
            bool Insert : 1;            // VK_INSERT, 0x2D
            bool Delete : 1;            // VK_DELETE, 0x2E
            bool Help : 1;              // VK_HELP, 0x2F
            bool D0 : 1;                // 0x30
            bool D1 : 1;                // 0x31
            bool D2 : 1;                // 0x32
            bool D3 : 1;                // 0x33
            bool D4 : 1;                // 0x34
            bool D5 : 1;                // 0x35
            bool D6 : 1;                // 0x36
            bool D7 : 1;                // 0x37
            bool D8 : 1;                // 0x38
            bool D9 : 1;                // 0x39
            bool Reserved8 : 6;
            bool Reserved9 : 1;
            bool A : 1;                 // 0x41
            bool B : 1;                 // 0x42
            bool C : 1;                 // 0x43
            bool D : 1;                 // 0x44
            bool E : 1;                 // 0x45
            bool F : 1;                 // 0x46
            bool G : 1;                 // 0x47
            bool H : 1;                 // 0x48
            bool I : 1;                 // 0x49
            bool J : 1;                 // 0x4A
            bool K : 1;                 // 0x4B
            bool L : 1;                 // 0x4C
            bool M : 1;                 // 0x4D
            bool N : 1;                 // 0x4E
            bool O : 1;                 // 0x4F
            bool P : 1;                 // 0x50
            bool Q : 1;                 // 0x51
            bool R : 1;                 // 0x52
            bool S : 1;                 // 0x53
            bool T : 1;                 // 0x54
            bool U : 1;                 // 0x55
            bool V : 1;                 // 0x56
            bool W : 1;                 // 0x57
            bool X : 1;                 // 0x58
            bool Y : 1;                 // 0x59
            bool Z : 1;                 // 0x5A
            bool LeftWindows : 1;       // VK_LWIN, 0x5B
            bool RightWindows : 1;      // VK_RWIN, 0x5C
            bool Apps : 1;              // VK_APPS, 0x5D
            bool Reserved10 : 1;
            bool Sleep : 1;             // VK_SLEEP, 0x5F
            bool NumPad0 : 1;           // VK_NUMPAD0, 0x60
            bool NumPad1 : 1;           // VK_NUMPAD1, 0x61
            bool NumPad2 : 1;           // VK_NUMPAD2, 0x62
            bool NumPad3 : 1;           // VK_NUMPAD3, 0x63
            bool NumPad4 : 1;           // VK_NUMPAD4, 0x64
            bool NumPad5 : 1;           // VK_NUMPAD5, 0x65
            bool NumPad6 : 1;           // VK_NUMPAD6, 0x66
            bool NumPad7 : 1;           // VK_NUMPAD7, 0x67
            bool NumPad8 : 1;           // VK_NUMPAD8, 0x68
            bool NumPad9 : 1;           // VK_NUMPAD9, 0x69
            bool Multiply : 1;          // VK_MULTIPLY, 0x6A
            bool Add : 1;               // VK_ADD, 0x6B
            bool Separator : 1;         // VK_SEPARATOR, 0x6C
            bool Subtract : 1;          // VK_SUBTRACT, 0x6D
            bool Decimal : 1;           // VK_DECIMANL, 0x6E
            bool Divide : 1;            // VK_DIVIDE, 0x6F
            bool F1 : 1;                // VK_F1, 0x70
            bool F2 : 1;                // VK_F2, 0x71
            bool F3 : 1;                // VK_F3, 0x72
            bool F4 : 1;                // VK_F4, 0x73
            bool F5 : 1;                // VK_F5, 0x74
            bool F6 : 1;                // VK_F6, 0x75
            bool F7 : 1;                // VK_F7, 0x76
            bool F8 : 1;                // VK_F8, 0x77
            bool F9 : 1;                // VK_F9, 0x78
            bool F10 : 1;               // VK_F10, 0x79
            bool F11 : 1;               // VK_F11, 0x7A
            bool F12 : 1;               // VK_F12, 0x7B
            bool F13 : 1;               // VK_F13, 0x7C
            bool F14 : 1;               // VK_F14, 0x7D
            bool F15 : 1;               // VK_F15, 0x7E
            bool F16 : 1;               // VK_F16, 0x7F
            bool F17 : 1;               // VK_F17, 0x80
            bool F18 : 1;               // VK_F18, 0x81
            bool F19 : 1;               // VK_F19, 0x82
            bool F20 : 1;               // VK_F20, 0x83
            bool F21 : 1;               // VK_F21, 0x84
            bool F22 : 1;               // VK_F22, 0x85
            bool F23 : 1;               // VK_F23, 0x86
            bool F24 : 1;               // VK_F24, 0x87
            bool Reserved11 : 8;
            bool NumLock : 1;           // VK_NUMLOCK, 0x90
            bool Scroll : 1;            // VK_SCROLL, 0x91
            bool Reserved12 : 6;
            bool Reserved13 : 8;
            bool LeftShift : 1;         // VK_LSHIFT, 0xA0
            bool RightShift : 1;        // VK_RSHIFT, 0xA1
            bool LeftControl : 1;       // VK_LCONTROL, 0xA2
            bool RightControl : 1;      // VK_RCONTROL, 0xA3
            bool LeftAlt : 1;           // VK_LMENU, 0xA4
            bool RightAlt : 1;          // VK_RMENU, 0xA5
            bool BrowserBack : 1;       // VK_BROWSER_BACK, 0xA6
            bool BrowserForward : 1;    // VK_BROWSER_FORWARD, 0xA7
            bool BrowserRefresh : 1;    // VK_BROWSER_REFRESH, 0xA8
            bool BrowserStop : 1;       // VK_BROWSER_STOP, 0xA9
            bool BrowserSearch : 1;     // VK_BROWSER_SEARCH, 0xAA
            bool BrowserFavorites : 1;  // VK_BROWSER_FAVORITES, 0xAB
            bool BrowserHome : 1;       // VK_BROWSER_HOME, 0xAC
            bool VolumeMute : 1;        // VK_VOLUME_MUTE, 0xAD
            bool VolumeDown : 1;        // VK_VOLUME_DOWN, 0xAE
            bool VolumeUp : 1;          // VK_VOLUME_UP, 0xAF
            bool MediaNextTrack : 1;    // VK_MEDIA_NEXT_TRACK, 0xB0
            bool MediaPreviousTrack : 1;// VK_MEDIA_PREV_TRACK, 0xB1
            bool MediaStop : 1;         // VK_MEDIA_STOP, 0xB2
            bool MediaPlayPause : 1;    // VK_MEDIA_PLAY_PAUSE, 0xB3
            bool LaunchMail : 1;        // VK_LAUNCH_MAIL, 0xB4
            bool SelectMedia : 1;       // VK_LAUNCH_MEDIA_SELECT, 0xB5
            bool LaunchApplication1 : 1;// VK_LAUNCH_APP1, 0xB6
            bool LaunchApplication2 : 1;// VK_LAUNCH_APP2, 0xB7
            bool Reserved14 : 2;
            bool OemSemicolon : 1;      // VK_OEM_1, 0xBA
            bool OemPlus : 1;           // VK_OEM_PLUS, 0xBB
            bool OemComma : 1;          // VK_OEM_COMMA, 0xBC
            bool OemMinus : 1;          // VK_OEM_MINUS, 0xBD
            bool OemPeriod : 1;         // VK_OEM_PERIOD, 0xBE
            bool OemQuestion : 1;       // VK_OEM_2, 0xBF
            bool OemTilde : 1;          // VK_OEM_3, 0xC0
            bool Reserved15 : 7;
            bool Reserved16 : 8;
            bool Reserved17 : 8;
            bool Reserved18 : 3;
            bool OemOpenBrackets : 1;   // VK_OEM_4, 0xDB
            bool OemPipe : 1;           // VK_OEM_5, 0xDC
            bool OemCloseBrackets : 1;  // VK_OEM_6, 0xDD
            bool OemQuotes : 1;         // VK_OEM_7, 0xDE
            bool Oem8 : 1;              // VK_OEM_8, 0xDF
            bool Reserved19 : 2;
            bool OemBackslash : 1;      // VK_OEM_102, 0xE2
            bool Reserved20 : 2;
            bool ProcessKey : 1;        // VK_PROCESSKEY, 0xE5
            bool Reserved21 : 2;
            bool Reserved22 : 8;
            bool Reserved23 : 2;
            bool OemCopy : 1;           // 0XF2
            bool OemAuto : 1;           // 0xF3
            bool OemEnlW : 1;           // 0xF4
            bool Reserved24 : 1;
            bool Attn : 1;              // VK_ATTN, 0xF6
            bool Crsel : 1;             // VK_CRSEL, 0xF7
            bool Exsel : 1;             // VK_EXSEL, 0xF8
            bool EraseEof : 1;          // VK_EREOF, 0xF9
            bool Play : 1;              // VK_PLAY, 0xFA
            bool Zoom : 1;              // VK_ZOOM, 0xFB
            bool Reserved25 : 1;
            bool Pa1 : 1;               // VK_PA1, 0xFD
            bool OemClear : 1;          // VK_OEM_CLEAR, 0xFE
            bool Reserved26 : 1;

            bool __cdecl IsKeyDown(Keys key) const noexcept
            {
                if (key <= 0xfe)
                {
                    auto ptr = reinterpret_cast<const uint32_t*>(this);
                    const unsigned int bf = 1u << (key & 0x1f);
                    return (ptr[(key >> 5)] & bf) != 0;
                }
                return false;
            }

            bool __cdecl IsKeyUp(Keys key) const noexcept
            {
                if (key <= 0xfe)
                {
                    auto ptr = reinterpret_cast<const uint32_t*>(this);
                    const unsigned int bf = 1u << (key & 0x1f);
                    return (ptr[(key >> 5)] & bf) == 0;
                }
                return false;
            }
        };

        class KeyboardStateTracker
        {
        public:
            State released;
            State pressed;

        #pragma prefast(suppress: 26495, "Reset() performs the initialization")
            KeyboardStateTracker() noexcept { Reset(); }

            void __cdecl Update(const State& state) noexcept;

            void __cdecl Reset() noexcept;

            bool __cdecl IsKeyPressed(Keys key) const noexcept { return pressed.IsKeyDown(key); }
            bool __cdecl IsKeyReleased(Keys key) const noexcept { return released.IsKeyDown(key); }

            State __cdecl GetLastState() const noexcept { return lastState; }

        public:
            State lastState;
        };

        // Retrieve the current state of the keyboard
        State __cdecl GetState() const;

        // Reset the keyboard state
        void __cdecl Reset() noexcept;

        // Feature detection
        bool __cdecl IsConnected() const;

    #ifdef USING_COREWINDOW
        void __cdecl SetWindow(ABI::Windows::UI::Core::ICoreWindow* window);
    #ifdef __cplusplus_winrt
        void __cdecl SetWindow(Windows::UI::Core::CoreWindow^ window)
        {
            // See https://msdn.microsoft.com/en-us/library/hh755802.aspx
            SetWindow(reinterpret_cast<ABI::Windows::UI::Core::ICoreWindow*>(window));
        }
    #endif
    #ifdef CPPWINRT_VERSION
        void __cdecl SetWindow(winrt::Windows::UI::Core::CoreWindow window)
        {
            // See https://docs.microsoft.com/en-us/windows/uwp/cpp-and-winrt-apis/interop-winrt-abi
            SetWindow(reinterpret_cast<ABI::Windows::UI::Core::ICoreWindow*>(winrt::get_abi(window)));
        }
    #endif
    #elif defined(WM_USER)
        static void __cdecl ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam);
    #endif

        // Singleton
        static Keyboard& __cdecl Get();

    private:
        // Private implementation.
        class Impl;

        std::unique_ptr<Impl> pImpl;
    };
}

#ifdef __clang__
#pragma clang diagnostic pop
#endif
