//--------------------------------------------------------------------------------------
// File: Mouse.cpp
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
//
// http://go.microsoft.com/fwlink/?LinkId=248929
// http://go.microsoft.com/fwlink/?LinkID=615561
//--------------------------------------------------------------------------------------

#include "pch.h"
#include "Mouse.h"

#include "PlatformHelpers.h"

using namespace DirectX;
using Microsoft::WRL::ComPtr;

#pragma region Implementations
#if (defined(WINAPI_FAMILY) && (WINAPI_FAMILY == WINAPI_FAMILY_GAMES)) || (defined(_GAMING_DESKTOP) && (_GRDK_EDITION >= 220600))

#include <GameInput.h>

//======================================================================================
// Win32 + GameInput implementation
//======================================================================================

//
// Call this static function from your Window Message Procedure
//
// LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
// {
//     switch (message)
//     {
//     case WM_ACTIVATE:
//     case WM_ACTIVATEAPP:
//     case WM_MOUSEMOVE:
//     case WM_LBUTTONDOWN:
//     case WM_LBUTTONUP:
//     case WM_RBUTTONDOWN:
//     case WM_RBUTTONUP:
//     case WM_MBUTTONDOWN:
//     case WM_MBUTTONUP:
//     case WM_MOUSEWHEEL:
//     case WM_XBUTTONDOWN:
//     case WM_XBUTTONUP:
//         Mouse::ProcessMessage(message, wParam, lParam);
//         break;
//
//     }
// }
//

class Mouse::Impl
{
public:
    explicit Impl(Mouse* owner) noexcept(false) :
        mState{},
        mOwner(owner),
        mScale(1.f),
        mConnected(0),
        mDeviceToken(0),
        mWindow(nullptr),
        mMode(MODE_ABSOLUTE),
        mScrollWheelCurrent(0),
        mRelativeX(INT64_MAX),
        mRelativeY(INT64_MAX),
        mRelativeWheelY(INT64_MAX)
    {
        if (s_mouse)
        {
            throw std::logic_error("Mouse is a singleton");
        }

        s_mouse = this;

        ThrowIfFailed(GameInputCreate(mGameInput.GetAddressOf()));

        ThrowIfFailed(mGameInput->RegisterDeviceCallback(
            nullptr,
            GameInputKindMouse,
            GameInputDeviceConnected,
            GameInputBlockingEnumeration,
            this,
            OnGameInputDevice,
            &mDeviceToken));

        mScrollWheelValue.reset(CreateEventEx(nullptr, nullptr, CREATE_EVENT_MANUAL_RESET, EVENT_MODIFY_STATE | SYNCHRONIZE));
        if (!mScrollWheelValue)
        {
            throw std::system_error(std::error_code(static_cast<int>(GetLastError()), std::system_category()), "CreateEventEx");
        }
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
                    DebugTrace("ERROR: GameInput::UnregisterCallback [mouse] failed");
                }
            }

            mDeviceToken = 0;
        }

        s_mouse = nullptr;
    }

    void GetState(State& state) const
    {
        memcpy(&state, &mState, sizeof(State));
        state.positionMode = mMode;

        DWORD result = WaitForSingleObjectEx(mScrollWheelValue.get(), 0, FALSE);
        if (result == WAIT_FAILED)
            throw std::system_error(std::error_code(static_cast<int>(GetLastError()), std::system_category()), "WaitForSingleObjectEx");

        if (result == WAIT_OBJECT_0)
        {
            mScrollWheelCurrent = 0;
        }

        if (state.positionMode == MODE_RELATIVE)
        {
            state.x = state.y = 0;

            ComPtr<IGameInputReading> reading;
            if (SUCCEEDED(mGameInput->GetCurrentReading(GameInputKindMouse, nullptr, reading.GetAddressOf())))
            {
                GameInputMouseState mouse;
                if (reading->GetMouseState(&mouse))
                {
                    state.leftButton = (mouse.buttons & GameInputMouseLeftButton) != 0;
                    state.middleButton = (mouse.buttons & GameInputMouseMiddleButton) != 0;
                    state.rightButton = (mouse.buttons & GameInputMouseRightButton) != 0;
                    state.xButton1 = (mouse.buttons & GameInputMouseButton4) != 0;
                    state.xButton2 = (mouse.buttons & GameInputMouseButton5) != 0;

                    if (mRelativeX != INT64_MAX)
                    {
                        state.x = static_cast<int>(mouse.positionX - mRelativeX);
                        state.y = static_cast<int>(mouse.positionY - mRelativeY);
                        int scrollDelta = static_cast<int>(mouse.wheelY - mRelativeWheelY);
                        mScrollWheelCurrent += scrollDelta;
                    }

                    mRelativeX = mouse.positionX;
                    mRelativeY = mouse.positionY;
                    mRelativeWheelY = mouse.wheelY;
                }
            }
        }

        state.scrollWheelValue = mScrollWheelCurrent;
    }

    void ResetScrollWheelValue() noexcept
    {
        SetEvent(mScrollWheelValue.get());
    }

    void SetWindow(HWND window)
    {
        mWindow = window;
    }

    void SetMode(Mode mode)
    {
        if (mMode == mode)
            return;

        mMode = mode;
        mRelativeX = INT64_MAX;
        mRelativeY = INT64_MAX;
        mRelativeWheelY = INT64_MAX;

        if (mode == MODE_RELATIVE)
        {
            ShowCursor(FALSE);
            ClipToWindow();
        }
        else
        {
            ShowCursor(TRUE);
#ifndef _GAMING_XBOX
            ClipCursor(nullptr);
#endif
        }
    }

    bool IsConnected() const noexcept
    {
        return mConnected > 0;
    }

    bool IsVisible() const noexcept
    {
        if (mMode == MODE_RELATIVE)
            return false;

        CURSORINFO info = { sizeof(CURSORINFO), 0, nullptr, {} };
        if (!GetCursorInfo(&info))
            return false;

        return (info.flags & CURSOR_SHOWING) != 0;
    }

    void SetVisible(bool visible)
    {
        if (mMode == MODE_RELATIVE)
            return;

        CURSORINFO info = { sizeof(CURSORINFO), 0, nullptr, {} };
        if (!GetCursorInfo(&info))
        {
            throw std::system_error(std::error_code(static_cast<int>(GetLastError()), std::system_category()), "GetCursorInfo");
        }

        bool isvisible = (info.flags & CURSOR_SHOWING) != 0;
        if (isvisible != visible)
        {
            ShowCursor(visible);
        }
    }

    State           mState;
    Mouse*          mOwner;
    float           mScale;
    uint32_t        mConnected;

    static Mouse::Impl* s_mouse;

private:
    ComPtr<IGameInput>      mGameInput;
    GameInputCallbackToken  mDeviceToken;

    HWND                    mWindow;
    Mode                    mMode;
    ScopedHandle            mScrollWheelValue;

    mutable int             mScrollWheelCurrent;
    mutable int64_t         mRelativeX;
    mutable int64_t         mRelativeY;
    mutable int64_t         mRelativeWheelY;

    friend void Mouse::ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam);

    static void CALLBACK OnGameInputDevice(
        _In_ GameInputCallbackToken,
        _In_ void * context,
        _In_ IGameInputDevice *,
        _In_ uint64_t,
        _In_ GameInputDeviceStatus currentStatus,
        _In_ GameInputDeviceStatus) noexcept
    {
        auto impl = reinterpret_cast<Mouse::Impl*>(context);

        if (currentStatus & GameInputDeviceConnected)
        {
            ++impl->mConnected;
        }
        else if (impl->mConnected > 0)
        {
            --impl->mConnected;
        }
    }

    void ClipToWindow() noexcept
    {
#ifndef _GAMING_XBOX
        assert(mWindow != nullptr);

        RECT rect;
        GetClientRect(mWindow, &rect);

        POINT ul;
        ul.x = rect.left;
        ul.y = rect.top;

        POINT lr;
        lr.x = rect.right;
        lr.y = rect.bottom;

        std::ignore = MapWindowPoints(mWindow, nullptr, &ul, 1);
        std::ignore = MapWindowPoints(mWindow, nullptr, &lr, 1);

        rect.left = ul.x;
        rect.top = ul.y;

        rect.right = lr.x;
        rect.bottom = lr.y;

        ClipCursor(&rect);
#endif
    }
};


Mouse::Impl* Mouse::Impl::s_mouse = nullptr;


void Mouse::ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam)
{
    auto pImpl = Impl::s_mouse;

    if (!pImpl)
        return;

    DWORD result = WaitForSingleObjectEx(pImpl->mScrollWheelValue.get(), 0, FALSE);
    if (result == WAIT_FAILED)
        throw std::system_error(std::error_code(static_cast<int>(GetLastError()), std::system_category()), "WaitForSingleObjectEx");

    if (result == WAIT_OBJECT_0)
    {
        pImpl->mScrollWheelCurrent = 0;
    }

    switch (message)
    {
    case WM_ACTIVATE:
    case WM_ACTIVATEAPP:
        if (wParam)
        {
            if (pImpl->mMode == MODE_RELATIVE)
            {
                pImpl->mRelativeX = INT64_MAX;
                pImpl->mRelativeY = INT64_MAX;

                ShowCursor(FALSE);

                pImpl->ClipToWindow();
            }
            else
            {
#ifndef _GAMING_XBOX
                ClipCursor(nullptr);
#endif
            }
        }
        else
        {
            memset(&pImpl->mState, 0, sizeof(State));
        }
        return;

    case WM_MOUSEMOVE:
        break;

    case WM_LBUTTONDOWN:
        pImpl->mState.leftButton = true;
        break;

    case WM_LBUTTONUP:
        pImpl->mState.leftButton = false;
        break;

    case WM_RBUTTONDOWN:
        pImpl->mState.rightButton = true;
        break;

    case WM_RBUTTONUP:
        pImpl->mState.rightButton = false;
        break;

    case WM_MBUTTONDOWN:
        pImpl->mState.middleButton = true;
        break;

    case WM_MBUTTONUP:
        pImpl->mState.middleButton = false;
        break;

    case WM_MOUSEWHEEL:
        if (pImpl->mMode == MODE_ABSOLUTE)
        {
            pImpl->mScrollWheelCurrent += GET_WHEEL_DELTA_WPARAM(wParam);
        }
        return;

    case WM_XBUTTONDOWN:
        switch (GET_XBUTTON_WPARAM(wParam))
        {
        case XBUTTON1:
            pImpl->mState.xButton1 = true;
            break;

        case XBUTTON2:
            pImpl->mState.xButton2 = true;
            break;
        }
        break;

    case WM_XBUTTONUP:
        switch (GET_XBUTTON_WPARAM(wParam))
        {
        case XBUTTON1:
            pImpl->mState.xButton1 = false;
            break;

        case XBUTTON2:
            pImpl->mState.xButton2 = false;
            break;
        }
        break;

    default:
        // Not a mouse message, so exit
        return;
    }

    if (pImpl->mMode == MODE_ABSOLUTE)
    {
        // All mouse messages provide a new pointer position
        int xPos = static_cast<short>(LOWORD(lParam)); // GET_X_LPARAM(lParam);
        int yPos = static_cast<short>(HIWORD(lParam)); // GET_Y_LPARAM(lParam);

        pImpl->mState.x = static_cast<int>(static_cast<float>(xPos) * pImpl->mScale);
        pImpl->mState.y = static_cast<int>(static_cast<float>(yPos) * pImpl->mScale);
    }
}

#ifdef _GAMING_XBOX
void Mouse::SetResolution(float scale)
{
    auto pImpl = Impl::s_mouse;

    if (!pImpl)
        return;

    pImpl->mScale = scale;
}
#endif

void Mouse::SetWindow(HWND window)
{
    pImpl->SetWindow(window);
}


#elif defined(USING_COREWINDOW)

//======================================================================================
// Windows Store or Universal Windows Platform (UWP) app implementation
//======================================================================================

//
// For a Windows Store app or Universal Windows Platform (UWP) app, add the following to your existing
// application methods:
//
// void App::SetWindow(CoreWindow^ window )
// {
//     m_mouse->SetWindow(window);
// }
//
// void App::OnDpiChanged(DisplayInformation^ sender, Object^ args)
// {
//     m_mouse->SetDpi(sender->LogicalDpi);
// }
//

#include <Windows.Devices.Input.h>

class Mouse::Impl
{
public:
    explicit Impl(Mouse* owner) noexcept(false) :
        mState{},
        mOwner(owner),
        mDPI(96.f),
        mMode(MODE_ABSOLUTE),
        mPointerPressedToken{},
        mPointerReleasedToken{},
        mPointerMovedToken{},
        mPointerWheelToken{},
        mPointerMouseMovedToken{}
    {
        if (s_mouse)
        {
            throw std::logic_error("Mouse is a singleton");
        }

        s_mouse = this;

        mScrollWheelValue.reset(CreateEventEx(nullptr, nullptr, CREATE_EVENT_MANUAL_RESET, EVENT_MODIFY_STATE | SYNCHRONIZE));
        mRelativeRead.reset(CreateEventEx(nullptr, nullptr, CREATE_EVENT_MANUAL_RESET, EVENT_MODIFY_STATE | SYNCHRONIZE));
        if (!mScrollWheelValue
            || !mRelativeRead)
        {
            throw std::system_error(std::error_code(static_cast<int>(GetLastError()), std::system_category()), "CreateEventEx");
        }
    }

    ~Impl()
    {
        s_mouse = nullptr;

        RemoveHandlers();
    }

    void GetState(State& state) const
    {
        memcpy(&state, &mState, sizeof(State));

        DWORD result = WaitForSingleObjectEx(mScrollWheelValue.get(), 0, FALSE);
        if (result == WAIT_FAILED)
            throw std::system_error(std::error_code(static_cast<int>(GetLastError()), std::system_category()), "WaitForSingleObjectEx");

        if (result == WAIT_OBJECT_0)
        {
            state.scrollWheelValue = 0;
        }

        if (mMode == MODE_RELATIVE)
        {
            result = WaitForSingleObjectEx(mRelativeRead.get(), 0, FALSE);

            if (result == WAIT_FAILED)
                throw std::system_error(std::error_code(static_cast<int>(GetLastError()), std::system_category()), "WaitForSingleObjectEx");

            if (result == WAIT_OBJECT_0)
            {
                state.x = 0;
                state.y = 0;
            }
            else
            {
                SetEvent(mRelativeRead.get());
            }
        }

        state.positionMode = mMode;
    }

    void ResetScrollWheelValue() noexcept
    {
        SetEvent(mScrollWheelValue.get());
    }

    void SetMode(Mode mode)
    {
        using namespace Microsoft::WRL;
        using namespace Microsoft::WRL::Wrappers;
        using namespace ABI::Windows::UI::Core;
        using namespace ABI::Windows::Foundation;

        if (mMode == mode)
            return;

        ComPtr<ICoreWindowStatic> statics;
        HRESULT hr = GetActivationFactory(HStringReference(RuntimeClass_Windows_UI_Core_CoreWindow).Get(), statics.GetAddressOf());
        ThrowIfFailed(hr);

        ComPtr<ICoreWindow> window;
        hr = statics->GetForCurrentThread(window.GetAddressOf());
        ThrowIfFailed(hr);

        if (mode == MODE_RELATIVE)
        {
            hr = window->get_PointerCursor(mCursor.ReleaseAndGetAddressOf());
            ThrowIfFailed(hr);

            hr = window->put_PointerCursor(nullptr);
            ThrowIfFailed(hr);

            SetEvent(mRelativeRead.get());

            mMode = MODE_RELATIVE;
        }
        else
        {
            if (!mCursor)
            {
                ComPtr<ICoreCursorFactory> factory;
                hr = GetActivationFactory(HStringReference(RuntimeClass_Windows_UI_Core_CoreCursor).Get(), factory.GetAddressOf());
                ThrowIfFailed(hr);

                hr = factory->CreateCursor(CoreCursorType_Arrow, 0, mCursor.GetAddressOf());
                ThrowIfFailed(hr);
            }

            hr = window->put_PointerCursor(mCursor.Get());
            ThrowIfFailed(hr);

            mCursor.Reset();

            mMode = MODE_ABSOLUTE;
        }
    }

    bool IsConnected() const
    {
        using namespace Microsoft::WRL;
        using namespace Microsoft::WRL::Wrappers;
        using namespace ABI::Windows::Devices::Input;
        using namespace ABI::Windows::Foundation;

        ComPtr<IMouseCapabilities> caps;
        HRESULT hr = RoActivateInstance(HStringReference(RuntimeClass_Windows_Devices_Input_MouseCapabilities).Get(), &caps);
        ThrowIfFailed(hr);

        INT32 value;
        if (SUCCEEDED(caps->get_MousePresent(&value)))
        {
            return value != 0;
        }

        return false;
    }

    bool IsVisible() const noexcept
    {
        if (mMode == MODE_RELATIVE)
            return false;

        ComPtr<ABI::Windows::UI::Core::ICoreCursor> cursor;
        if (FAILED(mWindow->get_PointerCursor(cursor.GetAddressOf())))
            return false;

        return cursor != nullptr;
    }

    void SetVisible(bool visible)
    {
        using namespace Microsoft::WRL::Wrappers;
        using namespace ABI::Windows::Foundation;
        using namespace ABI::Windows::UI::Core;

        if (mMode == MODE_RELATIVE)
            return;

        if (visible)
        {
            if (!mCursor)
            {
                ComPtr<ICoreCursorFactory> factory;
                HRESULT hr = GetActivationFactory(HStringReference(RuntimeClass_Windows_UI_Core_CoreCursor).Get(), factory.GetAddressOf());
                ThrowIfFailed(hr);

                hr = factory->CreateCursor(CoreCursorType_Arrow, 0, mCursor.GetAddressOf());
                ThrowIfFailed(hr);
            }

            HRESULT hr = mWindow->put_PointerCursor(mCursor.Get());
            ThrowIfFailed(hr);
        }
        else
        {
            HRESULT hr = mWindow->put_PointerCursor(nullptr);
            ThrowIfFailed(hr);
        }
    }

    void SetWindow(ABI::Windows::UI::Core::ICoreWindow* window)
    {
        using namespace Microsoft::WRL;
        using namespace Microsoft::WRL::Wrappers;
        using namespace ABI::Windows::Foundation;
        using namespace ABI::Windows::Devices::Input;

        if (mWindow.Get() == window)
            return;

        RemoveHandlers();

        mWindow = window;

        if (!window)
        {
            mCursor.Reset();
            mMouse.Reset();
            return;
        }

        ComPtr<IMouseDeviceStatics> mouseStatics;
        HRESULT hr = GetActivationFactory(HStringReference(RuntimeClass_Windows_Devices_Input_MouseDevice).Get(), mouseStatics.GetAddressOf());
        ThrowIfFailed(hr);

        hr = mouseStatics->GetForCurrentView(mMouse.ReleaseAndGetAddressOf());
        ThrowIfFailed(hr);

        typedef __FITypedEventHandler_2_Windows__CDevices__CInput__CMouseDevice_Windows__CDevices__CInput__CMouseEventArgs MouseMovedHandler;
        hr = mMouse->add_MouseMoved(Callback<MouseMovedHandler>(MouseMovedEvent).Get(), &mPointerMouseMovedToken);
        ThrowIfFailed(hr);

        typedef __FITypedEventHandler_2_Windows__CUI__CCore__CCoreWindow_Windows__CUI__CCore__CPointerEventArgs PointerHandler;
        auto cb = Callback<PointerHandler>(PointerEvent);

        hr = window->add_PointerPressed(cb.Get(), &mPointerPressedToken);
        ThrowIfFailed(hr);

        hr = window->add_PointerReleased(cb.Get(), &mPointerReleasedToken);
        ThrowIfFailed(hr);

        hr = window->add_PointerMoved(cb.Get(), &mPointerMovedToken);
        ThrowIfFailed(hr);

        hr = window->add_PointerWheelChanged(Callback<PointerHandler>(PointerWheel).Get(), &mPointerWheelToken);
        ThrowIfFailed(hr);
    }

    State           mState;
    Mouse* mOwner;
    float           mDPI;

    static Mouse::Impl* s_mouse;

private:
    Mode            mMode;

    ComPtr<ABI::Windows::UI::Core::ICoreWindow> mWindow;
    ComPtr<ABI::Windows::Devices::Input::IMouseDevice> mMouse;
    ComPtr<ABI::Windows::UI::Core::ICoreCursor> mCursor;

    ScopedHandle    mScrollWheelValue;
    ScopedHandle    mRelativeRead;

    EventRegistrationToken mPointerPressedToken;
    EventRegistrationToken mPointerReleasedToken;
    EventRegistrationToken mPointerMovedToken;
    EventRegistrationToken mPointerWheelToken;
    EventRegistrationToken mPointerMouseMovedToken;

    void RemoveHandlers()
    {
        if (mWindow)
        {
            std::ignore = mWindow->remove_PointerPressed(mPointerPressedToken);
            mPointerPressedToken.value = 0;

            std::ignore = mWindow->remove_PointerReleased(mPointerReleasedToken);
            mPointerReleasedToken.value = 0;

            std::ignore = mWindow->remove_PointerMoved(mPointerMovedToken);
            mPointerMovedToken.value = 0;

            std::ignore = mWindow->remove_PointerWheelChanged(mPointerWheelToken);
            mPointerWheelToken.value = 0;
        }

        if (mMouse)
        {
            std::ignore = mMouse->remove_MouseMoved(mPointerMouseMovedToken);
            mPointerMouseMovedToken.value = 0;
        }
    }

    static HRESULT PointerEvent(IInspectable*, ABI::Windows::UI::Core::IPointerEventArgs* args)
    {
        using namespace ABI::Windows::Foundation;
        using namespace ABI::Windows::UI::Input;
        using namespace ABI::Windows::Devices::Input;

        if (!s_mouse)
            return S_OK;

        ComPtr<IPointerPoint> currentPoint;
        HRESULT hr = args->get_CurrentPoint(currentPoint.GetAddressOf());
        ThrowIfFailed(hr);

        ComPtr<IPointerDevice> pointerDevice;
        hr = currentPoint->get_PointerDevice(pointerDevice.GetAddressOf());
        ThrowIfFailed(hr);

        PointerDeviceType devType;
        hr = pointerDevice->get_PointerDeviceType(&devType);
        ThrowIfFailed(hr);

        if (devType == PointerDeviceType::PointerDeviceType_Mouse)
        {
            ComPtr<IPointerPointProperties> props;
            hr = currentPoint->get_Properties(props.GetAddressOf());
            ThrowIfFailed(hr);

            boolean value;
            hr = props->get_IsLeftButtonPressed(&value);
            ThrowIfFailed(hr);
            s_mouse->mState.leftButton = value != 0;

            hr = props->get_IsRightButtonPressed(&value);
            ThrowIfFailed(hr);
            s_mouse->mState.rightButton = value != 0;

            hr = props->get_IsMiddleButtonPressed(&value);
            ThrowIfFailed(hr);
            s_mouse->mState.middleButton = value != 0;

            hr = props->get_IsXButton1Pressed(&value);
            ThrowIfFailed(hr);
            s_mouse->mState.xButton1 = value != 0;

            hr = props->get_IsXButton2Pressed(&value);
            ThrowIfFailed(hr);
            s_mouse->mState.xButton2 = value != 0;
        }

        if (s_mouse->mMode == MODE_ABSOLUTE)
        {
            Point pos;
            hr = currentPoint->get_Position(&pos);
            ThrowIfFailed(hr);

            float dpi = s_mouse->mDPI;

            s_mouse->mState.x = static_cast<int>(pos.X * dpi / 96.f + 0.5f);
            s_mouse->mState.y = static_cast<int>(pos.Y * dpi / 96.f + 0.5f);
        }

        return S_OK;
    }

    static HRESULT PointerWheel(IInspectable*, ABI::Windows::UI::Core::IPointerEventArgs* args)
    {
        using namespace ABI::Windows::Foundation;
        using namespace ABI::Windows::UI::Input;
        using namespace ABI::Windows::Devices::Input;

        if (!s_mouse)
            return S_OK;

        ComPtr<IPointerPoint> currentPoint;
        HRESULT hr = args->get_CurrentPoint(currentPoint.GetAddressOf());
        ThrowIfFailed(hr);

        ComPtr<IPointerDevice> pointerDevice;
        hr = currentPoint->get_PointerDevice(pointerDevice.GetAddressOf());
        ThrowIfFailed(hr);

        PointerDeviceType devType;
        hr = pointerDevice->get_PointerDeviceType(&devType);
        ThrowIfFailed(hr);

        if (devType == PointerDeviceType::PointerDeviceType_Mouse)
        {
            ComPtr<IPointerPointProperties> props;
            hr = currentPoint->get_Properties(props.GetAddressOf());
            ThrowIfFailed(hr);

            boolean ishorz;
            hr = props->get_IsHorizontalMouseWheel(&ishorz);
            ThrowIfFailed(hr);
            if (ishorz)
            {
                // Mouse only exposes the vertical scroll wheel.
                return S_OK;
            }

            INT32 value;
            hr = props->get_MouseWheelDelta(&value);
            ThrowIfFailed(hr);

            HANDLE evt = s_mouse->mScrollWheelValue.get();
            if (WaitForSingleObjectEx(evt, 0, FALSE) == WAIT_OBJECT_0)
            {
                s_mouse->mState.scrollWheelValue = 0;
                ResetEvent(evt);
            }

            s_mouse->mState.scrollWheelValue += value;

            if (s_mouse->mMode == MODE_ABSOLUTE)
            {
                Point pos;
                hr = currentPoint->get_Position(&pos);
                ThrowIfFailed(hr);

                float dpi = s_mouse->mDPI;

                s_mouse->mState.x = static_cast<int>(pos.X * dpi / 96.f + 0.5f);
                s_mouse->mState.y = static_cast<int>(pos.Y * dpi / 96.f + 0.5f);
            }
        }

        return S_OK;
    }

    static HRESULT MouseMovedEvent(IInspectable*, ABI::Windows::Devices::Input::IMouseEventArgs* args)
    {
        using namespace ABI::Windows::Devices::Input;

        if (!s_mouse)
            return S_OK;

        if (s_mouse->mMode == MODE_RELATIVE)
        {
            MouseDelta delta;
            HRESULT hr = args->get_MouseDelta(&delta);
            ThrowIfFailed(hr);

            s_mouse->mState.x = delta.X;
            s_mouse->mState.y = delta.Y;

            ResetEvent(s_mouse->mRelativeRead.get());
        }

        return S_OK;
    }
};


Mouse::Impl* Mouse::Impl::s_mouse = nullptr;


void Mouse::SetWindow(ABI::Windows::UI::Core::ICoreWindow* window)
{
    pImpl->SetWindow(window);
}


void Mouse::SetDpi(float dpi)
{
    auto pImpl = Impl::s_mouse;

    if (!pImpl)
        return;

    pImpl->mDPI = dpi;
}


#else

//======================================================================================
// Win32 desktop implementation
//======================================================================================

//
// For a Win32 desktop application, in your window setup be sure to call this method:
//
// m_mouse->SetWindow(hwnd);
//
// And call this static function from your Window Message Procedure
//
// LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
// {
//     switch (message)
//     {
//     case WM_ACTIVATE:
//     case WM_ACTIVATEAPP:
//     case WM_INPUT:
//     case WM_MOUSEMOVE:
//     case WM_LBUTTONDOWN:
//     case WM_LBUTTONUP:
//     case WM_RBUTTONDOWN:
//     case WM_RBUTTONUP:
//     case WM_MBUTTONDOWN:
//     case WM_MBUTTONUP:
//     case WM_MOUSEWHEEL:
//     case WM_XBUTTONDOWN:
//     case WM_XBUTTONUP:
//     case WM_MOUSEHOVER:
//         Mouse::ProcessMessage(message, wParam, lParam);
//         break;
//
//     }
// }
//

class Mouse::Impl
{
public:
    explicit Impl(Mouse* owner) noexcept(false) :
        mState{},
        mOwner(owner),
        mWindow(nullptr),
        mMode(MODE_ABSOLUTE),
        mLastX(0),
        mLastY(0),
        mRelativeX(INT32_MAX),
        mRelativeY(INT32_MAX),
        mInFocus(true)
    {
        if (s_mouse)
        {
            throw std::logic_error("Mouse is a singleton");
        }

        s_mouse = this;

        mScrollWheelValue.reset(CreateEventEx(nullptr, nullptr, CREATE_EVENT_MANUAL_RESET, EVENT_MODIFY_STATE | SYNCHRONIZE));
        mRelativeRead.reset(CreateEventEx(nullptr, nullptr, CREATE_EVENT_MANUAL_RESET, EVENT_MODIFY_STATE | SYNCHRONIZE));
        mAbsoluteMode.reset(CreateEventEx(nullptr, nullptr, 0, EVENT_MODIFY_STATE | SYNCHRONIZE));
        mRelativeMode.reset(CreateEventEx(nullptr, nullptr, 0, EVENT_MODIFY_STATE | SYNCHRONIZE));
        if (!mScrollWheelValue
            || !mRelativeRead
            || !mAbsoluteMode
            || !mRelativeMode)
        {
            throw std::system_error(std::error_code(static_cast<int>(GetLastError()), std::system_category()), "CreateEventEx");
        }
    }

    Impl(Impl&&) = default;
    Impl& operator= (Impl&&) = default;

    Impl(Impl const&) = delete;
    Impl& operator= (Impl const&) = delete;

    ~Impl()
    {
        s_mouse = nullptr;
    }

    void GetState(State& state) const
    {
        memcpy(&state, &mState, sizeof(State));
        state.positionMode = mMode;

        DWORD result = WaitForSingleObjectEx(mScrollWheelValue.get(), 0, FALSE);
        if (result == WAIT_FAILED)
            throw std::system_error(std::error_code(static_cast<int>(GetLastError()), std::system_category()), "WaitForSingleObjectEx");

        if (result == WAIT_OBJECT_0)
        {
            state.scrollWheelValue = 0;
        }

        if (state.positionMode == MODE_RELATIVE)
        {
            result = WaitForSingleObjectEx(mRelativeRead.get(), 0, FALSE);

            if (result == WAIT_FAILED)
                throw std::system_error(std::error_code(static_cast<int>(GetLastError()), std::system_category()), "WaitForSingleObjectEx");

            if (result == WAIT_OBJECT_0)
            {
                state.x = 0;
                state.y = 0;
            }
            else
            {
                SetEvent(mRelativeRead.get());
            }
        }
    }

    void ResetScrollWheelValue() noexcept
    {
        SetEvent(mScrollWheelValue.get());
    }

    void SetMode(Mode mode)
    {
        if (mMode == mode)
            return;

        SetEvent((mode == MODE_ABSOLUTE) ? mAbsoluteMode.get() : mRelativeMode.get());

        assert(mWindow != nullptr);

        // Send a WM_HOVER as a way to 'kick' the message processing even if the mouse is still.
        TRACKMOUSEEVENT tme;
        tme.cbSize = sizeof(tme);
        tme.dwFlags = TME_HOVER;
        tme.hwndTrack = mWindow;
        tme.dwHoverTime = 1;
        if (!TrackMouseEvent(&tme))
        {
            throw std::system_error(std::error_code(static_cast<int>(GetLastError()), std::system_category()), "TrackMouseEvent");
        }
    }

    bool IsConnected() const noexcept
    {
        return GetSystemMetrics(SM_MOUSEPRESENT) != 0;
    }

    bool IsVisible() const noexcept
    {
        if (mMode == MODE_RELATIVE)
            return false;

        CURSORINFO info = { sizeof(CURSORINFO), 0, nullptr, {} };
        if (!GetCursorInfo(&info))
            return false;

        return (info.flags & CURSOR_SHOWING) != 0;
    }

    void SetVisible(bool visible)
    {
        if (mMode == MODE_RELATIVE)
            return;

        CURSORINFO info = { sizeof(CURSORINFO), 0, nullptr, {} };
        if (!GetCursorInfo(&info))
        {
            throw std::system_error(std::error_code(static_cast<int>(GetLastError()), std::system_category()), "GetCursorInfo");
        }

        const bool isvisible = (info.flags & CURSOR_SHOWING) != 0;
        if (isvisible != visible)
        {
            ShowCursor(visible);
        }
    }

    void SetWindow(HWND window)
    {
        if (mWindow == window)
            return;

        assert(window != nullptr);

        RAWINPUTDEVICE Rid;
        Rid.usUsagePage = 0x1 /* HID_USAGE_PAGE_GENERIC */;
        Rid.usUsage = 0x2 /* HID_USAGE_GENERIC_MOUSE */;
        Rid.dwFlags = RIDEV_INPUTSINK;
        Rid.hwndTarget = window;
        if (!RegisterRawInputDevices(&Rid, 1, sizeof(RAWINPUTDEVICE)))
        {
            throw std::system_error(std::error_code(static_cast<int>(GetLastError()), std::system_category()), "RegisterRawInputDevices");
        }

        mWindow = window;
    }

    State           mState;

    Mouse*          mOwner;

    static Mouse::Impl* s_mouse;

private:
    HWND            mWindow;
    Mode            mMode;

    ScopedHandle    mScrollWheelValue;
    ScopedHandle    mRelativeRead;
    ScopedHandle    mAbsoluteMode;
    ScopedHandle    mRelativeMode;

    int             mLastX;
    int             mLastY;
    int             mRelativeX;
    int             mRelativeY;

    bool            mInFocus;

    friend void Mouse::ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam);

    void ClipToWindow() noexcept
    {
        assert(mWindow != nullptr);

        RECT rect;
        GetClientRect(mWindow, &rect);

        POINT ul;
        ul.x = rect.left;
        ul.y = rect.top;

        POINT lr;
        lr.x = rect.right;
        lr.y = rect.bottom;

        std::ignore = MapWindowPoints(mWindow, nullptr, &ul, 1);
        std::ignore = MapWindowPoints(mWindow, nullptr, &lr, 1);

        rect.left = ul.x;
        rect.top = ul.y;

        rect.right = lr.x;
        rect.bottom = lr.y;

        ClipCursor(&rect);
    }
};


Mouse::Impl* Mouse::Impl::s_mouse = nullptr;


void Mouse::SetWindow(HWND window)
{
    pImpl->SetWindow(window);
}


void Mouse::ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam)
{
    auto pImpl = Impl::s_mouse;

    if (!pImpl)
        return;

    // First handle any pending scroll wheel reset event.
    switch (WaitForSingleObjectEx(pImpl->mScrollWheelValue.get(), 0, FALSE))
    {
    default:
    case WAIT_TIMEOUT:
        break;

    case WAIT_OBJECT_0:
        pImpl->mState.scrollWheelValue = 0;
        ResetEvent(pImpl->mScrollWheelValue.get());
        break;

    case WAIT_FAILED:
        throw std::system_error(std::error_code(static_cast<int>(GetLastError()), std::system_category()), "WaitForMultipleObjectsEx");
    }

    // Next handle mode change events.
    HANDLE events[2] = { pImpl->mAbsoluteMode.get(), pImpl->mRelativeMode.get() };
    switch (WaitForMultipleObjectsEx(static_cast<DWORD>(std::size(events)), events, FALSE, 0, FALSE))
    {
    default:
    case WAIT_TIMEOUT:
        break;

    case WAIT_OBJECT_0:
        {
            pImpl->mMode = MODE_ABSOLUTE;
            ClipCursor(nullptr);

            POINT point;
            point.x = pImpl->mLastX;
            point.y = pImpl->mLastY;

            // We show the cursor before moving it to support Remote Desktop
            ShowCursor(TRUE);

            if (MapWindowPoints(pImpl->mWindow, nullptr, &point, 1))
            {
                SetCursorPos(point.x, point.y);
            }
            pImpl->mState.x = pImpl->mLastX;
            pImpl->mState.y = pImpl->mLastY;
        }
        break;

    case (WAIT_OBJECT_0 + 1):
        {
            ResetEvent(pImpl->mRelativeRead.get());

            pImpl->mMode = MODE_RELATIVE;
            pImpl->mState.x = pImpl->mState.y = 0;
            pImpl->mRelativeX = INT32_MAX;
            pImpl->mRelativeY = INT32_MAX;

            ShowCursor(FALSE);

            pImpl->ClipToWindow();
        }
        break;

    case WAIT_FAILED:
        throw std::system_error(std::error_code(static_cast<int>(GetLastError()), std::system_category()), "WaitForMultipleObjectsEx");
    }

    switch (message)
    {
    case WM_ACTIVATE:
    case WM_ACTIVATEAPP:
        if (wParam)
        {
            pImpl->mInFocus = true;

            if (pImpl->mMode == MODE_RELATIVE)
            {
                pImpl->mState.x = pImpl->mState.y = 0;

                ShowCursor(FALSE);

                pImpl->ClipToWindow();
            }
        }
        else
        {
            const int scrollWheel = pImpl->mState.scrollWheelValue;
            memset(&pImpl->mState, 0, sizeof(State));
            pImpl->mState.scrollWheelValue = scrollWheel;

            if (pImpl->mMode == MODE_RELATIVE)
            {
                ClipCursor(nullptr);
            }

            pImpl->mInFocus = false;
        }
        return;

    case WM_INPUT:
        if (pImpl->mInFocus && pImpl->mMode == MODE_RELATIVE)
        {
            RAWINPUT raw;
            UINT rawSize = sizeof(raw);

            const UINT resultData = GetRawInputData(reinterpret_cast<HRAWINPUT>(lParam), RID_INPUT, &raw, &rawSize, sizeof(RAWINPUTHEADER));
            if (resultData == UINT(-1))
            {
                throw std::runtime_error("GetRawInputData");
            }

            if (raw.header.dwType == RIM_TYPEMOUSE)
            {
                if (!(raw.data.mouse.usFlags & MOUSE_MOVE_ABSOLUTE))
                {
                    pImpl->mState.x = raw.data.mouse.lLastX;
                    pImpl->mState.y = raw.data.mouse.lLastY;

                    ResetEvent(pImpl->mRelativeRead.get());
                }
                else if (raw.data.mouse.usFlags & MOUSE_VIRTUAL_DESKTOP)
                {
                    // This is used to make Remote Desktop sessons work
                    const int width = GetSystemMetrics(SM_CXVIRTUALSCREEN);
                    const int height = GetSystemMetrics(SM_CYVIRTUALSCREEN);

                    auto const x = static_cast<int>((float(raw.data.mouse.lLastX) / 65535.0f) * float(width));
                    auto const y = static_cast<int>((float(raw.data.mouse.lLastY) / 65535.0f) * float(height));

                    if (pImpl->mRelativeX == INT32_MAX)
                    {
                        pImpl->mState.x = pImpl->mState.y = 0;
                    }
                    else
                    {
                        pImpl->mState.x = x - pImpl->mRelativeX;
                        pImpl->mState.y = y - pImpl->mRelativeY;
                    }

                    pImpl->mRelativeX = x;
                    pImpl->mRelativeY = y;

                    ResetEvent(pImpl->mRelativeRead.get());
                }
            }
        }
        return;

    case WM_MOUSEMOVE:
        break;

    case WM_LBUTTONDOWN:
        pImpl->mState.leftButton = true;
        break;

    case WM_LBUTTONUP:
        pImpl->mState.leftButton = false;
        break;

    case WM_RBUTTONDOWN:
        pImpl->mState.rightButton = true;
        break;

    case WM_RBUTTONUP:
        pImpl->mState.rightButton = false;
        break;

    case WM_MBUTTONDOWN:
        pImpl->mState.middleButton = true;
        break;

    case WM_MBUTTONUP:
        pImpl->mState.middleButton = false;
        break;

    case WM_MOUSEWHEEL:
        pImpl->mState.scrollWheelValue += GET_WHEEL_DELTA_WPARAM(wParam);
        return;

    case WM_XBUTTONDOWN:
        switch (GET_XBUTTON_WPARAM(wParam))
        {
        case XBUTTON1:
            pImpl->mState.xButton1 = true;
            break;

        case XBUTTON2:
            pImpl->mState.xButton2 = true;
            break;
        }
        break;

    case WM_XBUTTONUP:
        switch (GET_XBUTTON_WPARAM(wParam))
        {
        case XBUTTON1:
            pImpl->mState.xButton1 = false;
            break;

        case XBUTTON2:
            pImpl->mState.xButton2 = false;
            break;
        }
        break;

    case WM_MOUSEHOVER:
        break;

    default:
        // Not a mouse message, so exit
        return;
    }

    if (pImpl->mMode == MODE_ABSOLUTE)
    {
        // All mouse messages provide a new pointer position
        const int xPos = static_cast<short>(LOWORD(lParam)); // GET_X_LPARAM(lParam);
        const int yPos = static_cast<short>(HIWORD(lParam)); // GET_Y_LPARAM(lParam);

        pImpl->mState.x = pImpl->mLastX = xPos;
        pImpl->mState.y = pImpl->mLastY = yPos;
    }
}

#endif
#pragma endregion

#pragma warning( disable : 4355 )

// Public constructor.
Mouse::Mouse() noexcept(false)
    : pImpl(std::make_unique<Impl>(this))
{
}


// Move constructor.
Mouse::Mouse(Mouse&& moveFrom) noexcept
    : pImpl(std::move(moveFrom.pImpl))
{
    pImpl->mOwner = this;
}


// Move assignment.
Mouse& Mouse::operator= (Mouse&& moveFrom) noexcept
{
    pImpl = std::move(moveFrom.pImpl);
    pImpl->mOwner = this;
    return *this;
}


// Public destructor.
Mouse::~Mouse() = default;


Mouse::State Mouse::GetState() const
{
    State state;
    pImpl->GetState(state);
    return state;
}


void Mouse::ResetScrollWheelValue() noexcept
{
    pImpl->ResetScrollWheelValue();
}


void Mouse::SetMode(Mode mode)
{
    pImpl->SetMode(mode);
}


bool Mouse::IsConnected() const
{
    return pImpl->IsConnected();
}

bool Mouse::IsVisible() const noexcept
{
    return pImpl->IsVisible();
}

void Mouse::SetVisible(bool visible)
{
    pImpl->SetVisible(visible);
}

Mouse& Mouse::Get()
{
    if (!Impl::s_mouse || !Impl::s_mouse->mOwner)
        throw std::logic_error("Mouse singleton not created");

    return *Impl::s_mouse->mOwner;
}



//======================================================================================
// ButtonStateTracker
//======================================================================================

#define UPDATE_BUTTON_STATE(field) field = static_cast<ButtonState>( ( !!state.field ) | ( ( !!state.field ^ !!lastState.field ) << 1 ) )

void Mouse::ButtonStateTracker::Update(const Mouse::State& state) noexcept
{
    UPDATE_BUTTON_STATE(leftButton);

    assert((!state.leftButton && !lastState.leftButton) == (leftButton == UP));
    assert((state.leftButton && lastState.leftButton) == (leftButton == HELD));
    assert((!state.leftButton && lastState.leftButton) == (leftButton == RELEASED));
    assert((state.leftButton && !lastState.leftButton) == (leftButton == PRESSED));

    UPDATE_BUTTON_STATE(middleButton);
    UPDATE_BUTTON_STATE(rightButton);
    UPDATE_BUTTON_STATE(xButton1);
    UPDATE_BUTTON_STATE(xButton2);

    lastState = state;
}

#undef UPDATE_BUTTON_STATE


void Mouse::ButtonStateTracker::Reset() noexcept
{
    memset(this, 0, sizeof(ButtonStateTracker));
}
