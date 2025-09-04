/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- ActionArgsMagic.h

Abstract:
- Contains helpers for x-macro defining all our action args. This doesn't
  contain the actual x-macros themselves, but does contain the logic for
  actually synthesizing an ActionArgs implementation.
- Most of the time, you'll only need ACTION_ARGS_STRUCT
- ACTION_ARG_BODY is for when you've got some other logic to add to the class.

Author(s):
- Mike Griese - December 2021

--*/

#pragma once

// MACRO HACKS
//
// We want to have code that looks like:
//
// FooArgs(const ParamOne& one, const ParamTwo& two) :
//     _One{ one }, _Two{ two } {};
//
// However, if we just use the x-macro for this straight up, then the list will
// have a trailing comma at the end of it, and won't compile. So, we're creating
// this placeholder size-0 struct. It's going to be the last param for all the
// args' ctors. It'll have a default value, so no one will need to know about
// it. This will let us use the macro to populate the ctors as well.

struct InitListPlaceholder
{
};

//
// The complete ActionAndArgs definition. Each macro above ACTION_ARGS_STRUCT is
// some element of the class definition that will use the x-macro.
//
// You'll author a new arg by:
//   1: define a new x-macro above with all its properties
//   2. Define the class with:
//
//   ACTION_ARGS_STRUCT(MyFooArgs, MY_FOO_ARGS);
//
// In that macro, we'll use the passed-in macro (MY_FOO_ARGS) with each of these
// macros below, which will generate the various parts of the class body.
//
// Trying to make changes here? I'd recommend godbolt with the `-E` flag to gcc.
// That'll output the expanded macros, so you can see how it will all get
// expanded. Pretty critical for tracking down extraneous commas, etc.

// Property definitions, and JSON keys
#define DECLARE_ARGS(type, name, jsonKey, required, typeHint, ...) \
    static constexpr std::string_view name##Key{ jsonKey };        \
    ACTION_ARG(type, name, ##__VA_ARGS__);

// Parameters to the non-default ctor
#define CTOR_PARAMS(type, name, jsonKey, required, typeHint, ...) \
    const type &name##Param,

// initializers in the ctor
#define CTOR_INIT(type, name, jsonKey, required, typeHint, ...) \
    _##name{ name##Param },

// Expands to `+1` for every arg in the list
#define COUNT_ONE(type, name, jsonKey, required, typeHint, ...) +1

// Arg count macro
#define ARG_COUNT(argsMacro) (0 argsMacro(COUNT_ONE))

#define ARG_DESC_STRINGIFY2(x) #x
#define ARG_DESC_STRINGIFY(x) ARG_DESC_STRINGIFY2(x)
#define ARG_DESC_WIDEN2(x) L##x
#define ARG_DESC_WIDEN(x) ARG_DESC_WIDEN2(x)
#define LOCALIZED_NAME(name) ARG_DESC_WIDEN(ARG_DESC_STRINGIFY(name##ActionArgumentLocalized))

// append this argument's description to the internal vector
#define APPEND_ARG_DESCRIPTION(type, name, jsonKey, required, typeHint, ...) \
    temp.push_back({ RS_(LOCALIZED_NAME(name)), L## #type, std::wstring_view(L## #required) != L"false", typeHint });

#define INIT_ARG_DESCRIPTORS(argsMacro)                                                                    \
    ([]() -> winrt::Windows::Foundation::Collections::IVectorView<ArgDescriptor> {                         \
        std::vector<ArgDescriptor> temp;                                                                   \
        argsMacro(APPEND_ARG_DESCRIPTION) return winrt::single_threaded_vector(std::move(temp)).GetView(); \
    }())

// check each property in the Equals() method. You'll note there's a stray
// `true` in the definition of Equals() below, that's to deal with trailing
// commas
#define EQUALS_ARGS(type, name, jsonKey, required, typeHint, ...) \
    &&(otherAsUs->_##name == _##name)

#define X_MACRO_INDEX_BASE() \
    constexpr auto X_MACRO_INDEXED_BASE__ = __COUNTER__

#define X_MACRO_INDEX() \
    (__COUNTER__ - X_MACRO_INDEXED_BASE__ - 1)

// getter and setter for each property by index
#define GET_ARG_BY_INDEX(type, name, jsonKey, required, typeHint, ...) \
    if (index == X_MACRO_INDEX())                                      \
    {                                                                  \
        if (_##name.has_value())                                       \
        {                                                              \
            return winrt::box_value(_##name.value());                  \
        }                                                              \
        else                                                           \
        {                                                              \
            return winrt::box_value(static_cast<type>(__VA_ARGS__));   \
        }                                                              \
    }

#define SET_ARG_BY_INDEX(type, name, jsonKey, required, typeHint, ...) \
    if (index == X_MACRO_INDEX())                                      \
    {                                                                  \
        if (value)                                                     \
        {                                                              \
            _##name = winrt::unbox_value<type>(value);                 \
        }                                                              \
        else                                                           \
        {                                                              \
            _##name = std::nullopt;                                    \
        }                                                              \
    }

// JSON deserialization. If the parameter is required to pass any validation,
// add that as the `required` parameter here, as the body of a conditional
// EX: For the RESIZE_PANE_ARGS
//    X(Model::ResizeDirection, ResizeDirection, "direction", args->ResizeDirection() == ResizeDirection::None, Model::ResizeDirection::None)
// the bit
//    args->ResizeDirection() == ResizeDirection::None
// is used as the conditional for the validation here.
#define FROM_JSON_ARGS(type, name, jsonKey, required, typeHint, ...)            \
    JsonUtils::GetValueForKey(json, jsonKey, args->_##name);                    \
    if (required)                                                               \
    {                                                                           \
        return { nullptr, { SettingsLoadWarnings::MissingRequiredParameter } }; \
    }

// JSON serialization
#define TO_JSON_ARGS(type, name, jsonKey, required, typeHint, ...) \
    JsonUtils::SetValueForKey(json, jsonKey, args->_##name);

// Copy each property in the Copy() method
#define COPY_ARGS(type, name, jsonKey, required, typeHint, ...) \
    copy->_##name = _##name;

// hash each property in Hash(). You'll note there's a stray `0` in the
// definition of Hash() below, that's to deal with trailing commas (or in this
// case, leading.)
#define HASH_ARGS(type, name, jsonKey, required, typeHint, ...) \
    h.write(name());

// Use ACTION_ARGS_STRUCT when you've got no other customizing to do.
#define ACTION_ARGS_STRUCT(className, argsMacro)      \
    struct className : public className##T<className> \
    {                                                 \
        ACTION_ARG_BODY(className, argsMacro)         \
    };
// Use ACTION_ARG_BODY when you've got some other methods to add to the args class.
// case in point:
//   * NewTerminalArgs has a ToCommandline method it needs to additionally declare.
//   * GlobalSummonArgs has the QuakeModeFromJson helper

#define ACTION_ARG_BODY(className, argsMacro)                                                     \
    PARTIAL_ACTION_ARG_BODY(className, argsMacro)                                                 \
                                                                                                  \
public:                                                                                           \
    hstring GenerateName() const                                                                  \
    {                                                                                             \
        return GenerateName(                                                                      \
            GetLibraryResourceLoader().ResourceContext());                                        \
    }                                                                                             \
    hstring GenerateName(                                                                         \
        const winrt::Windows::ApplicationModel::Resources::Core::ResourceContext& context) const; \
    bool Equals(const IActionArgs& other)                                                         \
    {                                                                                             \
        auto otherAsUs = other.try_as<className>();                                               \
        if (otherAsUs)                                                                            \
        {                                                                                         \
            return true argsMacro(EQUALS_ARGS);                                                   \
        }                                                                                         \
        return false;                                                                             \
    };                                                                                            \
    static FromJsonResult FromJson(const Json::Value& json)                                       \
    {                                                                                             \
        auto args = winrt::make_self<className>();                                                \
        argsMacro(FROM_JSON_ARGS);                                                                \
        return { *args, {} };                                                                     \
    }                                                                                             \
    static Json::Value ToJson(const IActionArgs& val)                                             \
    {                                                                                             \
        if (!val)                                                                                 \
        {                                                                                         \
            return {};                                                                            \
        }                                                                                         \
        Json::Value json{ Json::ValueType::objectValue };                                         \
        const auto args{ get_self<className>(val) };                                              \
        argsMacro(TO_JSON_ARGS);                                                                  \
        return json;                                                                              \
    }                                                                                             \
    IActionArgs Copy() const                                                                      \
    {                                                                                             \
        auto copy{ winrt::make_self<className>() };                                               \
        argsMacro(COPY_ARGS);                                                                     \
        return *copy;                                                                             \
    }                                                                                             \
    size_t Hash() const                                                                           \
    {                                                                                             \
        til::hasher h;                                                                            \
        argsMacro(HASH_ARGS);                                                                     \
        return h.finalize();                                                                      \
    }

#define PARTIAL_ACTION_ARG_BODY(className, argsMacro)                                       \
    className() = default;                                                                  \
    className(                                                                              \
        argsMacro(CTOR_PARAMS) InitListPlaceholder = {}) :                                  \
        argsMacro(CTOR_INIT)                                                                \
            _placeholder{} {};                                                              \
    argsMacro(DECLARE_ARGS);                                                                \
                                                                                            \
private:                                                                                    \
    InitListPlaceholder _placeholder;                                                       \
                                                                                            \
public:                                                                                     \
    winrt::Windows::Foundation::Collections::IVectorView<ArgDescriptor> GetArgDescriptors() \
    {                                                                                       \
        static const auto descriptors = INIT_ARG_DESCRIPTORS(argsMacro);                    \
        return descriptors;                                                                 \
    }                                                                                       \
    IInspectable GetArgAt(uint32_t index) const                                             \
    {                                                                                       \
        X_MACRO_INDEX_BASE();                                                               \
        argsMacro(GET_ARG_BY_INDEX) return nullptr;                                         \
    }                                                                                       \
    void SetArgAt(uint32_t index, IInspectable value)                                       \
    {                                                                                       \
        X_MACRO_INDEX_BASE();                                                               \
        argsMacro(SET_ARG_BY_INDEX)                                                         \
    }
