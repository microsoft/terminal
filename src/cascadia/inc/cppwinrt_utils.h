// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

/*++
Module Name:
- cppwinrt_utils.h

Abstract:
- This module is used for winrt event declarations/definitions

Author(s):
- Carlos Zamora (CaZamor) 23-Apr-2019

Revision History:
- N/A
--*/

#pragma once

// This type is identical to winrt::fire_and_forget, but its unhandled_exception
// handler logs the exception instead of terminating the application.
//
// Ideally, we'd just use wil::com_task<void>, but it currently crashes
// with an AV if an exception is thrown after the first suspension point.
struct safe_void_coroutine
{
};

namespace std
{
    template<typename... Args>
    struct coroutine_traits<safe_void_coroutine, Args...>
    {
        struct promise_type
        {
            safe_void_coroutine get_return_object() const noexcept
            {
                return {};
            }

            void return_void() const noexcept
            {
            }

            suspend_never initial_suspend() const noexcept
            {
                return {};
            }

            suspend_never final_suspend() const noexcept
            {
                return {};
            }

            void unhandled_exception() const noexcept
            {
                LOG_CAUGHT_EXCEPTION();
                // If you get here, an unhandled exception was thrown.
                // In a Release build this would get silently swallowed.
                // You should probably fix the source of the exception, because it may have
                // unintended side effects, in particular with exception-unsafe logic.
                assert(false);
            }
        };
    };
}

template<>
struct fmt::formatter<winrt::hstring, wchar_t> : fmt::formatter<fmt::wstring_view, wchar_t>
{
    auto format(const winrt::hstring& str, auto& ctx) const
    {
        return fmt::formatter<fmt::wstring_view, wchar_t>::format({ str.data(), str.size() }, ctx);
    }
};

// This is a helper macro for both declaring the signature of an event, and
// defining the body. Winrt events need a method for adding a callback to the
// event and removing the callback. This macro will both declare the method
// signatures and define them both for you, because they don't really vary from
// event to event.
// Use this in a classes header if you have a Windows.Foundation.TypedEventHandler
#define TYPED_EVENT(name, sender, args)                                                                 \
public:                                                                                                 \
    winrt::event_token name(const winrt::Windows::Foundation::TypedEventHandler<sender, args>& handler) \
    {                                                                                                   \
        return _##name##Handlers.add(handler);                                                          \
    }                                                                                                   \
    void name(const winrt::event_token& token)                                                          \
    {                                                                                                   \
        _##name##Handlers.remove(token);                                                                \
    }                                                                                                   \
                                                                                                        \
private:                                                                                                \
    winrt::event<winrt::Windows::Foundation::TypedEventHandler<sender, args>> _##name##Handlers;

// This is a helper macro for both declaring the signature of a callback (nee event) and
// defining the body. Winrt callbacks need a method for adding a delegate to the
// callback and removing the delegate. This macro will both declare the method
// signatures and define them both for you, because they don't really vary from
// event to event.
// Use this in a class's header if you have a "delegate" type in your IDL.
#define WINRT_CALLBACK(name, args)               \
public:                                          \
    winrt::event_token name(const args& handler) \
    {                                            \
        return _##name##Handlers.add(handler);   \
    }                                            \
    void name(const winrt::event_token& token)   \
    {                                            \
        _##name##Handlers.remove(token);         \
    }                                            \
                                                 \
protected:                                       \
    winrt::event<args> _##name##Handlers;

// This is a helper macro for both declaring the signature and body of an event
// which is exposed by one class, but actually handled entirely by one of the
// class's members. This type of event could be considered "forwarded" or
// "proxied" to the handling type. Case in point: many of the events on App are
// just forwarded straight to TerminalPage. This macro will both declare the
// method signatures and define them both for you.
#define FORWARDED_TYPED_EVENT(name, sender, args, handler, handlerName)                    \
public:                                                                                    \
    winrt::event_token name(const Windows::Foundation::TypedEventHandler<sender, args>& h) \
    {                                                                                      \
        return handler->handlerName(h);                                                    \
    }                                                                                      \
    void name(const winrt::event_token& token) noexcept                                    \
    {                                                                                      \
        handler->handlerName(token);                                                       \
    }

// Same thing, but handler is a projected type, not an implementation
#define PROJECTED_FORWARDED_TYPED_EVENT(name, sender, args, handler, handlerName)          \
public:                                                                                    \
    winrt::event_token name(const Windows::Foundation::TypedEventHandler<sender, args>& h) \
    {                                                                                      \
        return handler.handlerName(h);                                                     \
    }                                                                                      \
    void name(const winrt::event_token& token) noexcept                                    \
    {                                                                                      \
        handler.handlerName(token);                                                        \
    }

// This is a bit like *FORWARDED_TYPED_EVENT. When you use a forwarded event,
// the handler gets added to the object that's raising the event. For example,
// the TerminalPage might be the handler for the TermControl's
// BackgroundColorChanged event, which is actually implemented by the
// ControlCore. So when Core raises an event, it immediately calls the handler
// on the Page.
//
// Instead, the BUBBLED event introduces an indirection layer. In the above
// example, the Core would raise the event, but now the Control would handle it,
// and raise an event with each of its own handlers.
//
// This allows us to detach the core from the control safely, without needing to
// re-wire all the event handlers from page->control again.
//
// Implement like:
//
//    _core.TitleChanged({ get_weak(), &TermControl::_bubbleTitleChanged });
#define BUBBLED_FORWARDED_TYPED_EVENT(name, sender, args) \
    TYPED_EVENT(name, sender, args)                       \
    void _bubble##name(const sender& s, const args& a)    \
    {                                                     \
        _##name##Handlers(s, a);                          \
    }

// Use this macro to quick implement both the getter and setter for a property.
// This should only be used for simple types where there's no logic in the
// getter/setter beyond just accessing/updating the value.
#define WINRT_PROPERTY(type, name, ...)   \
public:                                   \
    type name() const noexcept            \
    {                                     \
        return _##name;                   \
    }                                     \
    void name(const type& value) noexcept \
    {                                     \
        _##name = value;                  \
    }                                     \
                                          \
protected:                                \
    type _##name{ __VA_ARGS__ };

// Use this macro to quickly implement both the getter and setter for an
// observable property. This is similar to the WINRT_PROPERTY macro above,
// except this will also raise a PropertyChanged event with the name of the
// property that has changed inside of the setter. This also implements a
// private _setName() method, that the class can internally use to change the
// value when it _knows_ it doesn't need to raise the PropertyChanged event
// (like when the class is being initialized).
#define WINRT_OBSERVABLE_PROPERTY(type, name, event, ...)                                 \
public:                                                                                   \
    type name() const noexcept                                                            \
    {                                                                                     \
        return _##name;                                                                   \
    };                                                                                    \
    void name(const type& value)                                                          \
    {                                                                                     \
        if (_##name != value)                                                             \
        {                                                                                 \
            _##name = value;                                                              \
            event(*this, Windows::UI::Xaml::Data::PropertyChangedEventArgs{ L## #name }); \
        }                                                                                 \
    };                                                                                    \
                                                                                          \
protected:                                                                                \
    type _##name{ __VA_ARGS__ };                                                          \
    void _set##name(const type& value) noexcept(noexcept(_##name = value))                \
    {                                                                                     \
        _##name = value;                                                                  \
    };

// Use this macro for quickly defining the factory_implementation part of a
// class. CppWinrt requires these for the compiler, but more often than not,
// they require no customization. See
// https://docs.microsoft.com/en-us/uwp/cpp-ref-for-winrt/implements#marker-types
// and https://docs.microsoft.com/en-us/uwp/cpp-ref-for-winrt/static-lifetime
// for examples of when you might _not_ want to use this.
#define BASIC_FACTORY(typeName)                                       \
    struct typeName : typeName##T<typeName, implementation::typeName> \
    {                                                                 \
    };

inline winrt::array_view<const char16_t> winrt_wstring_to_array_view(const std::wstring_view& str)
{
#pragma warning(suppress : 26490) // Don't use reinterpret_cast (type.1).
    return winrt::array_view<const char16_t>(reinterpret_cast<const char16_t*>(str.data()), gsl::narrow<uint32_t>(str.size()));
}

inline std::wstring_view winrt_array_to_wstring_view(const winrt::array_view<const char16_t>& str) noexcept
{
#pragma warning(suppress : 26490) // Don't use reinterpret_cast (type.1).
    return { reinterpret_cast<const wchar_t*>(str.data()), str.size() };
}

// This is a helper method for deserializing a SAFEARRAY of
// COM objects and converting it to a vector that
// owns the extracted COM objects
template<typename T>
std::vector<wil::com_ptr<T>> SafeArrayToOwningVector(SAFEARRAY* safeArray)
{
    T** pVals;
    THROW_IF_FAILED(SafeArrayAccessData(safeArray, (void**)&pVals));

    THROW_HR_IF(E_UNEXPECTED, SafeArrayGetDim(safeArray) != 1);

    long lBound, uBound;
    THROW_IF_FAILED(SafeArrayGetLBound(safeArray, 1, &lBound));
    THROW_IF_FAILED(SafeArrayGetUBound(safeArray, 1, &uBound));

    long count = uBound - lBound + 1;

    // If any of the above fail, we cannot destruct/release
    // any of the elements in the SAFEARRAY because we
    // cannot identify how many elements there are.

    std::vector<wil::com_ptr<T>> result{ gsl::narrow<std::size_t>(count) };
    for (int i = 0; i < count; i++)
    {
        result[i].attach(pVals[i]);
    }

    return result;
}

#define DECLARE_CONVERTER(nameSpace, className)                                                                   \
    namespace nameSpace::implementation                                                                           \
    {                                                                                                             \
        struct className : className##T<className>                                                                \
        {                                                                                                         \
            className() = default;                                                                                \
                                                                                                                  \
            Windows::Foundation::IInspectable Convert(const Windows::Foundation::IInspectable& value,             \
                                                      const Windows::UI::Xaml::Interop::TypeName& targetType,     \
                                                      const Windows::Foundation::IInspectable& parameter,         \
                                                      const hstring& language);                                   \
                                                                                                                  \
            Windows::Foundation::IInspectable ConvertBack(const Windows::Foundation::IInspectable& value,         \
                                                          const Windows::UI::Xaml::Interop::TypeName& targetType, \
                                                          const Windows::Foundation::IInspectable& parameter,     \
                                                          const hstring& language);                               \
        };                                                                                                        \
    }                                                                                                             \
                                                                                                                  \
    namespace nameSpace::factory_implementation                                                                   \
    {                                                                                                             \
        BASIC_FACTORY(className);                                                                                 \
    }

#ifdef WINRT_Windows_UI_Xaml_H

inline ::winrt::hstring XamlThicknessToOptimalString(const ::winrt::Windows::UI::Xaml::Thickness& t)
{
    if (t.Left == t.Right)
    {
        if (t.Top == t.Bottom)
        {
            if (t.Top == t.Left)
            {
                return ::winrt::hstring{ fmt::format(FMT_COMPILE(L"{}"), t.Left) };
            }
            return ::winrt::hstring{ fmt::format(FMT_COMPILE(L"{},{}"), t.Left, t.Top) };
        }
        // fall through
    }
    return ::winrt::hstring{ fmt::format(FMT_COMPILE(L"{},{},{},{}"), t.Left, t.Top, t.Right, t.Bottom) };
}

inline ::winrt::Windows::UI::Xaml::Thickness StringToXamlThickness(std::wstring_view padding)
try
{
    uintptr_t count{ 0 };
    double t[4]{ 0. }; // left, top, right, bottom
    std::wstring buf;
    auto& errnoRef = errno; // Nonzero cost, pay it once
    for (const auto& token : til::split_iterator{ padding, L',' })
    {
        buf.assign(token);
        // wcstod handles whitespace prefix (which is ignored) & stops the
        // scan when first char outside the range of radix is encountered.
        // We'll be permissive till the extent that stod function allows us to be by default
        // Ex. a value like 100.3#535w2 will be read as 100.3, but ;df25 will fail
        errnoRef = 0;
        wchar_t* end;
        const auto val{ std::wcstod(buf.c_str(), &end) };
        if (end != buf.c_str() && errnoRef != ERANGE)
        {
            til::at(t, count) = val;
        }

        if (++count >= 4)
        {
            break;
        }
    }

#pragma warning(push)
#pragma warning(disable : 26446) // Prefer to use gsl::at() instead of unchecked subscript operator (bounds.4).
    switch (count)
    {
    case 1: // one input = all 4 values are the same
        t[1] = t[0]; // top = left
        __fallthrough;
    case 2: // two inputs = top/bottom and left/right are the same
        t[2] = t[0]; // right = left
        t[3] = t[1]; // bottom = top
        __fallthrough;
    case 4: // four inputs = fully specified
        break;
    default:
        return {};
    }
    return { t[0], t[1], t[2], t[3] };
#pragma warning(pop)
}
catch (...)
{
    LOG_CAUGHT_EXCEPTION();
    return {};
}

#endif
