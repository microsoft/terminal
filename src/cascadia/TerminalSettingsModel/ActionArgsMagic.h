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
#define DECLARE_ARGS(type, name, jsonKey, required, tag, ...)    \
    static constexpr std::string_view name##Key{ jsonKey }; \
    ACTION_ARG(type, name, ##__VA_ARGS__);

// Parameters to the non-default ctor
#define CTOR_PARAMS(type, name, jsonKey, required, tag, ...) \
    const type &name##Param,

// initializers in the ctor
#define CTOR_INIT(type, name, jsonKey, required, tag, ...) \
    _##name{ name##Param },

// append this argument's description to the internal vector
#define APPEND_ARG_DESCRIPTION(type, name, jsonKey, required, tag, ...)                                      \
    _argDescriptions.push_back({ L## #name, L## #type, std::wstring_view(L## #required) != L"false", tag });

// check each property in the Equals() method. You'll note there's a stray
// `true` in the definition of Equals() below, that's to deal with trailing
// commas
#define EQUALS_ARGS(type, name, jsonKey, required, tag, ...) \
    &&(otherAsUs->_##name == _##name)

// getter and setter for each property by index
#define GET_ARG_BY_INDEX(type, name, jsonKey, required, tag, ...)         \
    if (index == curIndex++)                                              \
    {                                                                     \
        if (_##name.has_value())                                          \
        {                                                                 \
            return winrt::box_value(_##name.value());                     \
        }                                                                 \
        else                                                              \
        {                                                                 \
            return winrt::box_value(static_cast<type>(__VA_ARGS__));      \
        }                                                                 \
    }

#define SET_ARG_BY_INDEX(type, name, jsonKey, required, tag, ...) \
    if (index == curIndex++)                                      \
    {                                                             \
        if (value)                                                \
        {                                                         \
            _##name = winrt::unbox_value<type>(value);            \
        }                                                         \
        else                                                      \
        {                                                         \
            _##name = std::nullopt;                               \
        }                                                         \
    }

// JSON deserialization. If the parameter is required to pass any validation,
// add that as the `required` parameter here, as the body of a conditional
// EX: For the RESIZE_PANE_ARGS
//    X(Model::ResizeDirection, ResizeDirection, "direction", args->ResizeDirection() == ResizeDirection::None, Model::ResizeDirection::None)
// the bit
//    args->ResizeDirection() == ResizeDirection::None
// is used as the conditional for the validation here.
#define FROM_JSON_ARGS(type, name, jsonKey, required, tag, ...)                                                \
    JsonUtils::GetValueForKey(json, jsonKey, args->_##name);                                                   \
    if (required)                                                                                              \
    {                                                                                                          \
        return { nullptr, { SettingsLoadWarnings::MissingRequiredParameter } };                                \
    }

// JSON serialization
#define TO_JSON_ARGS(type, name, jsonKey, required, tag, ...) \
    JsonUtils::SetValueForKey(json, jsonKey, args->_##name);

// Copy each property in the Copy() method
#define COPY_ARGS(type, name, jsonKey, required, tag, ...) \
    copy->_##name = _##name;

// hash each property in Hash(). You'll note there's a stray `0` in the
// definition of Hash() below, that's to deal with trailing commas (or in this
// case, leading.)
#define HASH_ARGS(type, name, jsonKey, required, tag, ...) \
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

#define ACTION_ARG_BODY(className, argsMacro)                    \
    className() { argsMacro(APPEND_ARG_DESCRIPTION) };           \
    className(                                                   \
        argsMacro(CTOR_PARAMS) InitListPlaceholder = {}) :       \
        argsMacro(CTOR_INIT) _placeholder{} {                    \
            argsMacro(APPEND_ARG_DESCRIPTION)                    \
        };                                                       \
    argsMacro(DECLARE_ARGS);                                     \
                                                                 \
private:                                                         \
    InitListPlaceholder _placeholder;                            \
    std::vector<ArgDescription> _argDescriptions;                \
                                                                 \
public:                                                          \
    hstring GenerateName() const;                                \
    bool Equals(const IActionArgs& other)                        \
    {                                                            \
        auto otherAsUs = other.try_as<className>();              \
        if (otherAsUs)                                           \
        {                                                        \
            return true argsMacro(EQUALS_ARGS);                  \
        }                                                        \
        return false;                                            \
    };                                                           \
    static FromJsonResult FromJson(const Json::Value& json)      \
    {                                                            \
        auto args = winrt::make_self<className>();               \
        argsMacro(FROM_JSON_ARGS);                               \
        return { *args, {} };                                    \
    }                                                            \
    static Json::Value ToJson(const IActionArgs& val)            \
    {                                                            \
        if (!val)                                                \
        {                                                        \
            return {};                                           \
        }                                                        \
        Json::Value json{ Json::ValueType::objectValue };        \
        const auto args{ get_self<className>(val) };             \
        argsMacro(TO_JSON_ARGS);                                 \
        return json;                                             \
    }                                                            \
    IActionArgs Copy() const                                     \
    {                                                            \
        auto copy{ winrt::make_self<className>() };              \
        argsMacro(COPY_ARGS);                                    \
        copy->_argDescriptions = _argDescriptions;               \
        return *copy;                                            \
    }                                                            \
    size_t Hash() const                                          \
    {                                                            \
        til::hasher h;                                           \
        argsMacro(HASH_ARGS);                                    \
        return h.finalize();                                     \
    }                                                            \
    uint32_t GetArgCount() const                                 \
    {                                                            \
        return gsl::narrow<uint32_t>(_argDescriptions.size());   \
    }                                                            \
    ArgDescription GetArgDescriptionAt(uint32_t index) const     \
    {                                                            \
        return _argDescriptions.at(index);                       \
    }                                                            \
    IInspectable GetArgAt(uint32_t index) const                  \
    {                                                            \
        uint32_t curIndex{ 0 };                                  \
        argsMacro(GET_ARG_BY_INDEX)                              \
        return nullptr;                                          \
    }                                                            \
    void SetArgAt(uint32_t index, IInspectable value)            \
    {                                                            \
        uint32_t curIndex{ 0 };                                  \
        argsMacro(SET_ARG_BY_INDEX)                              \
    }

#define PARTIAL_ACTION_ARG_BODY(className, argsMacro)          \
    className(){ argsMacro(APPEND_ARG_DESCRIPTION) };          \
    className(                                                 \
        argsMacro(CTOR_PARAMS) InitListPlaceholder = {}) :     \
        argsMacro(CTOR_INIT) _placeholder{} {                  \
            argsMacro(APPEND_ARG_DESCRIPTION)                  \
        };                                                     \
    argsMacro(DECLARE_ARGS);                                   \
                                                               \
private:                                                       \
    InitListPlaceholder _placeholder;                          \
    std::vector<ArgDescription> _argDescriptions;              \
                                                               \
public:                                                        \
    uint32_t GetArgCount() const                               \
    {                                                          \
        return gsl::narrow<uint32_t>(_argDescriptions.size()); \
    }                                                          \
    ArgDescription GetArgDescriptionAt(uint32_t index) const   \
    {                                                          \
        return _argDescriptions.at(index);                     \
    }                                                          \
    IInspectable GetArgAt(uint32_t index) const                \
    {                                                          \
        uint32_t curIndex{ 0 };                                \
        argsMacro(GET_ARG_BY_INDEX)                            \
        return nullptr;                                        \
    }                                                          \
    void SetArgAt(uint32_t index, IInspectable value)          \
    {                                                          \
        uint32_t curIndex{ 0 };                                \
        argsMacro(SET_ARG_BY_INDEX)                            \
    }
