//--------------------------------------------------------------------------------------
// File: Keyboard.cpp
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
//
// http://go.microsoft.com/fwlink/?LinkId=248929
// http://go.microsoft.com/fwlink/?LinkID=615561
//--------------------------------------------------------------------------------------

#include "pch.h"
#include "Keyboard.h"

#include "PlatformHelpers.h"

using namespace DirectX;
using Microsoft::WRL::ComPtr;

static_assert(sizeof(Keyboard::State) == (256 / 8), "Size mismatch for State");

#ifdef __clang__
#pragma clang diagnostic ignored "-Wunused-function"
#endif

namespace
{
    inline void KeyDown(int key, Keyboard::State& state) noexcept
    {
        if (key < 0 || key > 0xfe)
            return;

        auto ptr = reinterpret_cast<uint32_t*>(&state);

        const unsigned int bf = 1u << (key & 0x1f);
        ptr[(key >> 5)] |= bf;
    }

    inline void KeyUp(int key, Keyboard::State& state) noexcept
    {
        if (key < 0 || key > 0xfe)
            return;

        auto ptr = reinterpret_cast<uint32_t*>(&state);

        const unsigned int bf = 1u << (key & 0x1f);
        ptr[(key >> 5)] &= ~bf;
    }
}


#pragma region Implementations
#if (defined(WINAPI_FAMILY) && (WINAPI_FAMILY == WINAPI_FAMILY_GAMES)) || (defined(_GAMING_DESKTOP) && (_GRDK_EDITION >= 220600))

#include <GameInput.h>

//======================================================================================
// GameInput
//======================================================================================

class Keyboard::Impl
{
public:
    Impl(Keyboard* owner) :
        mOwner(owner),
        mConnected(0),
        mDeviceToken(0),
        mKeyState{}
    {
        if (s_keyboard)
        {
            throw std::logic_error("Keyboard is a singleton");
        }

        s_keyboard = this;

        ThrowIfFailed(GameInputCreate(mGameInput.GetAddressOf()));

        ThrowIfFailed(mGameInput->RegisterDeviceCallback(
            nullptr,
            GameInputKindKeyboard,
            GameInputDeviceConnected,
            GameInputBlockingEnumeration,
            this,
            OnGameInputDevice,
            &mDeviceToken));
    }

    Impl(Impl&&) = default;
    Impl& operator= (Impl&&) = default;

    Impl(Impl const&) = delete;
    Impl& operator= (Impl const&) = delete;

    ~Impl()
    {
        if (mDeviceToken)
        {
            if (mGameInput)
            {
                if (!mGameInput->UnregisterCallback(mDeviceToken, UINT64_MAX))
                {
                    DebugTrace("ERROR: GameInput::UnregisterCallback [keyboard] failed");
                }
            }

            mDeviceToken = 0;
        }

        s_keyboard = nullptr;
    }

    void GetState(State& state) const
    {
        state = {};

        ComPtr<IGameInputReading> reading;
        if (SUCCEEDED(mGameInput->GetCurrentReading(GameInputKindKeyboard, nullptr, reading.GetAddressOf())))
        {
            uint32_t readCount = reading->GetKeyState(c_MaxSimultaneousKeys, mKeyState);
            for (size_t j = 0; j < readCount; ++j)
            {
                int vk = static_cast<int>(mKeyState[j].virtualKey);

                // Workaround for known issues with VK_RSHIFT and VK_NUMLOCK
                if (vk == 0)
                {
                    switch (mKeyState[j].scanCode)
                    {
                    case 0xe036: vk = VK_RSHIFT; break;
                    case 0xe045: vk = VK_NUMLOCK; break;
                    default: break;
                    }
                }

                KeyDown(vk, state);
            }
        }
    }

    void Reset() noexcept
    {
    }

    bool IsConnected() const
    {
        return mConnected > 0;
    }

    Keyboard*       mOwner;
    uint32_t        mConnected;

    static Keyboard::Impl* s_keyboard;

private:
    static constexpr size_t     c_MaxSimultaneousKeys = 16;

    ComPtr<IGameInput>          mGameInput;
    GameInputCallbackToken      mDeviceToken;

    mutable GameInputKeyState   mKeyState[c_MaxSimultaneousKeys];

    static void CALLBACK OnGameInputDevice(
        _In_ GameInputCallbackToken,
        _In_ void * context,
        _In_ IGameInputDevice *,
        _In_ uint64_t,
        _In_ GameInputDeviceStatus currentStatus,
        _In_ GameInputDeviceStatus) noexcept
    {
        auto impl = reinterpret_cast<Keyboard::Impl*>(context);

        if (currentStatus & GameInputDeviceConnected)
        {
            ++impl->mConnected;
        }
        else if (impl->mConnected > 0)
        {
            --impl->mConnected;
        }
    }
};


Keyboard::Impl* Keyboard::Impl::s_keyboard = nullptr;


void Keyboard::ProcessMessage(UINT, WPARAM, LPARAM)
{
    // GameInput for Keyboard doesn't require Win32 messages, but this simplifies integration.
}


#elif defined(USING_COREWINDOW)

//======================================================================================
// Windows Store or Universal Windows Platform (UWP) app implementation
//======================================================================================

//
// For a Windows Store app or Universal Windows Platform (UWP) app, add the following:
//
// void App::SetWindow(CoreWindow^ window )
// {
//     m_keyboard->SetWindow(window);
// }
//

#include <Windows.Devices.Input.h>

class Keyboard::Impl
{
public:
    Impl(Keyboard* owner) :
        mState{},
        mOwner(owner),
        mAcceleratorKeyToken{},
        mActivatedToken{}
    {
        if (s_keyboard)
        {
            throw std::logic_error("Keyboard is a singleton");
        }

        s_keyboard = this;
    }

    ~Impl()
    {
        s_keyboard = nullptr;

        RemoveHandlers();
    }

    void GetState(State& state) const
    {
        memcpy(&state, &mState, sizeof(State));
    }

    void Reset() noexcept
    {
        memset(&mState, 0, sizeof(State));
    }

    bool IsConnected() const
    {
        using namespace Microsoft::WRL;
        using namespace Microsoft::WRL::Wrappers;
        using namespace ABI::Windows::Devices::Input;
        using namespace ABI::Windows::Foundation;

        ComPtr<IKeyboardCapabilities> caps;
        HRESULT hr = RoActivateInstance(HStringReference(RuntimeClass_Windows_Devices_Input_KeyboardCapabilities).Get(), &caps);
        ThrowIfFailed(hr);

        INT32 value;
        if (SUCCEEDED(caps->get_KeyboardPresent(&value)))
        {
            return value != 0;
        }

        return false;
    }

    void SetWindow(ABI::Windows::UI::Core::ICoreWindow* window)
    {
        using namespace Microsoft::WRL;
        using namespace Microsoft::WRL::Wrappers;
        using namespace ABI::Windows::UI::Core;

        if (mWindow.Get() == window)
            return;

        RemoveHandlers();

        mWindow = window;

        if (!window)
            return;

        typedef __FITypedEventHandler_2_Windows__CUI__CCore__CCoreWindow_Windows__CUI__CCore__CWindowActivatedEventArgs ActivatedHandler;
        HRESULT hr = window->add_Activated(Callback<ActivatedHandler>(Activated).Get(), &mActivatedToken);
        ThrowIfFailed(hr);

        ComPtr<ICoreDispatcher> dispatcher;
        hr = window->get_Dispatcher(dispatcher.GetAddressOf());
        ThrowIfFailed(hr);

        ComPtr<ICoreAcceleratorKeys> keys;
        hr = dispatcher.As(&keys);
        ThrowIfFailed(hr);

        typedef __FITypedEventHandler_2_Windows__CUI__CCore__CCoreDispatcher_Windows__CUI__CCore__CAcceleratorKeyEventArgs AcceleratorKeyHandler;
        hr = keys->add_AcceleratorKeyActivated(Callback<AcceleratorKeyHandler>(AcceleratorKeyEvent).Get(), &mAcceleratorKeyToken);
        ThrowIfFailed(hr);
    }

    State       mState;
    Keyboard*   mOwner;

    static Keyboard::Impl* s_keyboard;

private:
    ComPtr<ABI::Windows::UI::Core::ICoreWindow> mWindow;

    EventRegistrationToken mAcceleratorKeyToken;
    EventRegistrationToken mActivatedToken;

    void RemoveHandlers()
    {
        if (mWindow)
        {
            using namespace ABI::Windows::UI::Core;

            ComPtr<ICoreDispatcher> dispatcher;
            HRESULT hr = mWindow->get_Dispatcher(dispatcher.GetAddressOf());
            ThrowIfFailed(hr);

            std::ignore = mWindow->remove_Activated(mActivatedToken);
            mActivatedToken.value = 0;

            ComPtr<ICoreAcceleratorKeys> keys;
            hr = dispatcher.As(&keys);
            ThrowIfFailed(hr);

            std::ignore = keys->remove_AcceleratorKeyActivated(mAcceleratorKeyToken);
            mAcceleratorKeyToken.value = 0;
        }
    }

    static HRESULT Activated(IInspectable*, ABI::Windows::UI::Core::IWindowActivatedEventArgs*)
    {
        auto pImpl = Impl::s_keyboard;

        if (!pImpl)
            return S_OK;

        pImpl->Reset();

        return S_OK;
    }

    static HRESULT AcceleratorKeyEvent(IInspectable*, ABI::Windows::UI::Core::IAcceleratorKeyEventArgs* args)
    {
        using namespace ABI::Windows::System;
        using namespace ABI::Windows::UI::Core;

        auto pImpl = Impl::s_keyboard;

        if (!pImpl)
            return S_OK;

        CoreAcceleratorKeyEventType evtType;
        HRESULT hr = args->get_EventType(&evtType);
        ThrowIfFailed(hr);

        bool down = false;

        switch (evtType)
        {
        case CoreAcceleratorKeyEventType_KeyDown:
        case CoreAcceleratorKeyEventType_SystemKeyDown:
            down = true;
            break;

        case CoreAcceleratorKeyEventType_KeyUp:
        case CoreAcceleratorKeyEventType_SystemKeyUp:
            break;

        default:
            return S_OK;
        }

        CorePhysicalKeyStatus status;
        hr = args->get_KeyStatus(&status);
        ThrowIfFailed(hr);

        VirtualKey virtualKey;
        hr = args->get_VirtualKey(&virtualKey);
        ThrowIfFailed(hr);

        int vk = static_cast<int>(virtualKey);

        switch (vk)
        {
        case VK_SHIFT:
            vk = (status.ScanCode == 0x36) ? VK_RSHIFT : VK_LSHIFT;
            if (!down)
            {
                // Workaround to ensure left vs. right shift get cleared when both were pressed at same time
                KeyUp(VK_LSHIFT, pImpl->mState);
                KeyUp(VK_RSHIFT, pImpl->mState);
            }
            break;

        case VK_CONTROL:
            vk = (status.IsExtendedKey) ? VK_RCONTROL : VK_LCONTROL;
            break;

        case VK_MENU:
            vk = (status.IsExtendedKey) ? VK_RMENU : VK_LMENU;
            break;
        }

        if (down)
        {
            KeyDown(vk, pImpl->mState);
        }
        else
        {
            KeyUp(vk, pImpl->mState);
        }

        return S_OK;
    }
};


Keyboard::Impl* Keyboard::Impl::s_keyboard = nullptr;


void Keyboard::SetWindow(ABI::Windows::UI::Core::ICoreWindow* window)
{
    pImpl->SetWindow(window);
}


#else

//======================================================================================
// Win32 desktop implementation
//======================================================================================

//
// For a Win32 desktop application, call this function from your Window Message Procedure
//
// LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
// {
//     switch (message)
//     {
//
//     case WM_ACTIVATE:
//     case WM_ACTIVATEAPP:
//         Keyboard::ProcessMessage(message, wParam, lParam);
//         break;
//
//     case WM_KEYDOWN:
//     case WM_SYSKEYDOWN:
//     case WM_KEYUP:
//     case WM_SYSKEYUP:
//         Keyboard::ProcessMessage(message, wParam, lParam);
//         break;
//
//     }
// }
//

class Keyboard::Impl
{
public:
    Impl(Keyboard* owner) :
        mState{},
        mOwner(owner)
    {
        if (s_keyboard)
        {
            throw std::logic_error("Keyboard is a singleton");
        }

        s_keyboard = this;
    }

    Impl(Impl&&) = default;
    Impl& operator= (Impl&&) = default;

    Impl(Impl const&) = delete;
    Impl& operator= (Impl const&) = delete;

    ~Impl()
    {
        s_keyboard = nullptr;
    }

    void GetState(State& state) const
    {
        memcpy(&state, &mState, sizeof(State));
    }

    void Reset() noexcept
    {
        memset(&mState, 0, sizeof(State));
    }

    bool IsConnected() const
    {
        return true;
    }

    State           mState;
    Keyboard*       mOwner;

    static Keyboard::Impl* s_keyboard;
};


Keyboard::Impl* Keyboard::Impl::s_keyboard = nullptr;


void Keyboard::ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam)
{
    auto pImpl = Impl::s_keyboard;

    if (!pImpl)
        return;

    bool down = false;

    switch (message)
    {
    case WM_ACTIVATE:
    case WM_ACTIVATEAPP:
        pImpl->Reset();
        return;

    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
        down = true;
        break;

    case WM_KEYUP:
    case WM_SYSKEYUP:
        break;

    default:
        return;
    }

    int vk = LOWORD(wParam);
    // We want to distinguish left and right shift/ctrl/alt keys
    switch (vk)
    {
    case VK_SHIFT:
    case VK_CONTROL:
    case VK_MENU:
        {
            if (vk == VK_SHIFT && !down)
            {
                // Workaround to ensure left vs. right shift get cleared when both were pressed at same time
                KeyUp(VK_LSHIFT, pImpl->mState);
                KeyUp(VK_RSHIFT, pImpl->mState);
            }

            bool isExtendedKey = (HIWORD(lParam) & KF_EXTENDED) == KF_EXTENDED;
            int scanCode = LOBYTE(HIWORD(lParam)) | (isExtendedKey ? 0xe000 : 0);
            vk = LOWORD(MapVirtualKeyW(static_cast<UINT>(scanCode), MAPVK_VSC_TO_VK_EX));
        }
        break;
    }

    if (down)
    {
        KeyDown(vk, pImpl->mState);
    }
    else
    {
        KeyUp(vk, pImpl->mState);
    }
}

#endif
#pragma endregion

#pragma warning( disable : 4355 )

// Public constructor.
Keyboard::Keyboard() noexcept(false)
    : pImpl(std::make_unique<Impl>(this))
{
}


// Move constructor.
Keyboard::Keyboard(Keyboard&& moveFrom) noexcept
    : pImpl(std::move(moveFrom.pImpl))
{
    pImpl->mOwner = this;
}


// Move assignment.
Keyboard& Keyboard::operator= (Keyboard&& moveFrom) noexcept
{
    pImpl = std::move(moveFrom.pImpl);
    pImpl->mOwner = this;
    return *this;
}


// Public destructor.
Keyboard::~Keyboard() = default;


Keyboard::State Keyboard::GetState() const
{
    State state;
    pImpl->GetState(state);
    return state;
}


void Keyboard::Reset() noexcept
{
    pImpl->Reset();
}


bool Keyboard::IsConnected() const
{
    return pImpl->IsConnected();
}

Keyboard& Keyboard::Get()
{
    if (!Impl::s_keyboard || !Impl::s_keyboard->mOwner)
        throw std::logic_error("Keyboard singleton not created");

    return *Impl::s_keyboard->mOwner;
}



//======================================================================================
// KeyboardStateTracker
//======================================================================================

void Keyboard::KeyboardStateTracker::Update(const State& state) noexcept
{
    auto currPtr = reinterpret_cast<const uint32_t*>(&state);
    auto prevPtr = reinterpret_cast<const uint32_t*>(&lastState);
    auto releasedPtr = reinterpret_cast<uint32_t*>(&released);
    auto pressedPtr = reinterpret_cast<uint32_t*>(&pressed);
    for (size_t j = 0; j < (256 / 32); ++j)
    {
        *pressedPtr = *currPtr & ~(*prevPtr);
        *releasedPtr = ~(*currPtr) & *prevPtr;

        ++currPtr;
        ++prevPtr;
        ++releasedPtr;
        ++pressedPtr;
    }

    lastState = state;
}

void Keyboard::KeyboardStateTracker::Reset() noexcept
{
    memset(this, 0, sizeof(KeyboardStateTracker));
}
