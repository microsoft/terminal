/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- IInheritable.h

Abstract:
- An interface allowing settings objects to inherit settings from a parent

Author(s):
- Carlos Zamora - October 2020

--*/
#pragma once

namespace winrt::Microsoft::Terminal::Settings::Model::implementation
{
    template<typename T>
    struct IInheritable
    {
    public:
        // Method Description:
        // - Create a new instance of T, but set its parent to this instance
        // Arguments:
        // - <none>
        // Return Value:
        // - a new instance of T with this instance set as its parent
        com_ptr<T> CreateChild() const
        {
            auto child{ winrt::make_self<T>() };
            winrt::copy_from_abi(child->_parent, const_cast<T*>(static_cast<const T*>(this)));
            child->_FinalizeInheritance();
            return child;
        }

    protected:
        com_ptr<T> _parent{ nullptr };

        // Method Description:
        // - Actions to be performed after a child was created. Generally used to set
        //   any extraneous data from the parent into the child.
        // Arguments:
        // - <none>
        // Return Value:
        // - <none>
        virtual void _FinalizeInheritance() {}
    };

    // This is like std::optional, but we can use it in inheritance to determine whether the user explicitly cleared it
    template<typename T>
    struct NullableSetting
    {
        winrt::Windows::Foundation::IReference<T> setting{ nullptr };
        bool set{ false };
    };
}

// Use this macro to quickly implement both getters and the setter for an
// inheritable and observable setting property. This is similar to the GETSET_PROPERTY macro, except...
// - Has(): checks if the user explicitly set a value for this setting
// - Getter(): return the resolved value
// - Setter(): set the value directly
// - Clear(): clear the user set value
// - the setting is saved as an optional, where nullopt means
//   that we must inherit the value from our parent
#define GETSET_SETTING(type, name, ...)                                     \
public:                                                                     \
    /* Returns true if the user explicitly set the value, false otherwise*/ \
    bool Has##name() const                                                  \
    {                                                                       \
        return _##name.has_value();                                         \
    };                                                                      \
                                                                            \
    /* Returns the resolved value for this setting */                       \
    /* fallback: user set value --> inherited value --> system set value */ \
    type name() const                                                       \
    {                                                                       \
        const auto val{ _get##name##Impl() };                               \
        return val ? *val : type{ __VA_ARGS__ };                            \
    };                                                                      \
                                                                            \
    /* Overwrite the user set value */                                      \
    void name(const type& value)                                            \
    {                                                                       \
        _##name = value;                                                    \
    };                                                                      \
                                                                            \
    /* Clear the user set value */                                          \
    void Clear##name()                                                      \
    {                                                                       \
        _##name = std::nullopt;                                             \
    };                                                                      \
                                                                            \
private:                                                                    \
    std::optional<type> _##name{ std::nullopt };                            \
    std::optional<type> _get##name##Impl() const                            \
    {                                                                       \
        return _##name ?                                                    \
                   _##name :                                                \
                   (_parent ?                                               \
                        _parent->_get##name##Impl() :                       \
                        std::nullopt);                                      \
    };

// This macro is similar to the one above, but is reserved for optional settings
// like Profile.StartingDirectory and Profile.Foreground (where null is interpreted
// as an acceptable value, rather than "inherit")
// "type" is exposed as an IReference
#define GETSET_NULLABLE_SETTING(type, name, ...)                                      \
public:                                                                               \
    /* Returns true if the user explicitly set the value, false otherwise*/           \
    bool Has##name() const                                                            \
    {                                                                                 \
        return _##name.set;                                                           \
    };                                                                                \
                                                                                      \
    /* Returns the resolved value for this setting */                                 \
    /* fallback: user set value --> inherited value --> system set value */           \
    winrt::Windows::Foundation::IReference<type> name() const                         \
    {                                                                                 \
        return _get##name##Impl();                                                    \
    };                                                                                \
                                                                                      \
    /* Overwrite the user set value */                                                \
    /* Dispatch event if value changed */                                             \
    void name(const winrt::Windows::Foundation::IReference<type>& value)              \
    {                                                                                 \
        if (!Has##name() /*value was not set*/                                        \
            || _##name.setting != value) /*set value is different*/                   \
        {                                                                             \
            _##name.setting = value;                                                  \
            _##name.set = true;                                                       \
        }                                                                             \
    };                                                                                \
                                                                                      \
    /* Clear the user set value */                                                    \
    /* Dispatch event if value changed */                                             \
    void Clear##name()                                                                \
    {                                                                                 \
        _##name.set = false;                                                          \
    };                                                                                \
                                                                                      \
private:                                                                              \
    NullableSetting<type> _##name{};                                                  \
    winrt::Windows::Foundation::IReference<type> _get##name##Impl() const             \
    {                                                                                 \
        return Has##name() ?                                                          \
                   _##name.setting :                                                  \
                   (_parent ?                                                         \
                        _parent->_get##name##Impl() :                                 \
                        winrt::Windows::Foundation::IReference<type>{ __VA_ARGS__ }); \
    };

// Use this macro for Profile::ApplyTo
// Checks if a given setting is set, and if it is, sets it on profile
#define APPLY_OUT(setting)                \
    if (Has##setting())                   \
    {                                     \
        profile->_##setting = _##setting; \
    }
