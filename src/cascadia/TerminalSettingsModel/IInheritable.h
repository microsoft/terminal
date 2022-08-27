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

            // set "this" as the parent.
            // However, "this" is an IInheritable, so we need to cast it as T (the impl winrt type)
            //  to pass ownership over to the com_ptr.
            com_ptr<T> parent;
            winrt::copy_from_abi(parent, const_cast<T*>(static_cast<const T*>(this)));
            child->AddLeastImportantParent(parent);

            child->_FinalizeInheritance();
            return child;
        }

        void ClearParents()
        {
            _parents.clear();
        }

        void AddLeastImportantParent(com_ptr<T> parent)
        {
            _parents.emplace_back(std::move(parent));
        }

        void AddMostImportantParent(com_ptr<T> parent)
        {
            _parents.emplace(_parents.begin(), std::move(parent));
        }

        const std::vector<com_ptr<T>>& Parents()
        {
            return _parents;
        }

    protected:
        std::vector<com_ptr<T>> _parents{};

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
    using NullableSetting = std::optional<std::optional<T>>;
}

// Shared implementation between the INHERITABLE_SETTING and INHERITABLE_NULLABLE_SETTING macros.
// See below for more details.
#define _BASE_INHERITABLE_SETTING(projectedType, storageType, name, ...)    \
public:                                                                     \
    /* Returns true if the user explicitly set the value, false otherwise*/ \
    bool Has##name() const                                                  \
    {                                                                       \
        return _##name.has_value();                                         \
    }                                                                       \
                                                                            \
    projectedType name##OverrideSource()                                    \
    {                                                                       \
        /*user set value was not set*/                                      \
        /*iterate through parents to find one with a value*/                \
        for (const auto& parent : _parents)                                 \
        {                                                                   \
            if (auto source{ parent->_get##name##OverrideSourceImpl() })    \
            {                                                               \
                return source;                                              \
            }                                                               \
        }                                                                   \
                                                                            \
        /*no source was found*/                                             \
        return nullptr;                                                     \
    }                                                                       \
                                                                            \
    /* Clear the user set value */                                          \
    void Clear##name()                                                      \
    {                                                                       \
        _##name = std::nullopt;                                             \
    }                                                                       \
                                                                            \
private:                                                                    \
    storageType _##name{ std::nullopt };                                    \
                                                                            \
    storageType _get##name##Impl() const                                    \
    {                                                                       \
        /*return user set value*/                                           \
        if (_##name)                                                        \
        {                                                                   \
            return _##name;                                                 \
        }                                                                   \
                                                                            \
        /*user set value was not set*/                                      \
        /*iterate through parents to find a value*/                         \
        for (const auto& parent : _parents)                                 \
        {                                                                   \
            if (auto val{ parent->_get##name##Impl() })                     \
            {                                                               \
                return val;                                                 \
            }                                                               \
        }                                                                   \
                                                                            \
        /*no value was found*/                                              \
        return std::nullopt;                                                \
    }                                                                       \
                                                                            \
    projectedType _get##name##OverrideSourceImpl() const                    \
    {                                                                       \
        /*we have a value*/                                                 \
        if (_##name)                                                        \
        {                                                                   \
            return *this;                                                   \
        }                                                                   \
                                                                            \
        /*user set value was not set*/                                      \
        /*iterate through parents to find one with a value*/                \
        for (const auto& parent : _parents)                                 \
        {                                                                   \
            if (auto source{ parent->_get##name##OverrideSourceImpl() })    \
            {                                                               \
                return source;                                              \
            }                                                               \
        }                                                                   \
                                                                            \
        /*no value was found*/                                              \
        return nullptr;                                                     \
    }

// Use INHERITABLE_SETTING and INHERITABLE_NULLABLE_SETTING to implement
// standard functions for inheritable settings. This is similar to the WINRT_PROPERTY macro,
// except...
// - Has<NAME>(): checks if the user explicitly set a value for this setting
// - <NAME>OverrideSource(): return the object that provides the resolved value
// - Getter(): return the resolved value
// - Setter(): set the value directly
// - Clear(): clear the user set value
// - the setting is saved as an optional, where nullopt means
//   that we must inherit the value from our parent
#define INHERITABLE_SETTING(projectedType, type, name, ...)                  \
    _BASE_INHERITABLE_SETTING(projectedType, std::optional<type>, name, ...) \
public:                                                                      \
    /* Returns the resolved value for this setting */                        \
    /* fallback: user set value --> inherited value --> system set value */  \
    type name() const                                                        \
    {                                                                        \
        const auto val{ _get##name##Impl() };                                \
        return val ? *val : type{ __VA_ARGS__ };                             \
    }                                                                        \
                                                                             \
    /* Overwrite the user set value */                                       \
    void name(const type& value)                                             \
    {                                                                        \
        _##name = value;                                                     \
    }

// This macro is similar to the one above, but is reserved for optional settings
// like Profile.Foreground (where null is interpreted
// as an acceptable value, rather than "inherit")
// "type" is exposed as an IReference
#define INHERITABLE_NULLABLE_SETTING(projectedType, type, name, ...)           \
    _BASE_INHERITABLE_SETTING(projectedType, NullableSetting<type>, name, ...) \
public:                                                                        \
    /* Returns the resolved value for this setting */                          \
    /* fallback: user set value --> inherited value --> system set value */    \
    winrt::Windows::Foundation::IReference<type> name() const                  \
    {                                                                          \
        const auto val{ _get##name##Impl() };                                  \
        if (val)                                                               \
        {                                                                      \
            if (*val)                                                          \
            {                                                                  \
                return **val;                                                  \
            }                                                                  \
            return nullptr;                                                    \
        }                                                                      \
        return winrt::Windows::Foundation::IReference<type>{ __VA_ARGS__ };    \
    }                                                                          \
                                                                               \
    /* Overwrite the user set value */                                         \
    void name(const winrt::Windows::Foundation::IReference<type>& value)       \
    {                                                                          \
        if (value) /*set value is different*/                                  \
        {                                                                      \
            _##name = std::optional<type>{ value.Value() };                    \
        }                                                                      \
        else                                                                   \
        {                                                                      \
            /* note we're setting the _inner_ value */                         \
            _##name = std::optional<type>{ std::nullopt };                     \
        }                                                                      \
    }
