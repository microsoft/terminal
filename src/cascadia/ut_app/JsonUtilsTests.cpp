// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "../TerminalApp/JsonUtils.h"

using namespace Microsoft::Console;
using namespace WEX::Logging;
using namespace WEX::TestExecution;
using namespace WEX::Common;
using namespace TerminalApp::JsonUtils;

struct StructWithConverterSpecialization
{
    int value;

    bool operator==(const StructWithConverterSpecialization& other) const
    {
        return value == other.value;
    }
};

template<>
struct ConversionTrait<StructWithConverterSpecialization>
{
    StructWithConverterSpecialization FromJson(const Json::Value& value)
    {
        return StructWithConverterSpecialization{ value.asInt() };
    }

    bool CanConvert(const Json::Value& value)
    {
        return value.isInt();
    }
};

// Converts a JSON string value to an int and multiplies it by a specified factor
struct CustomConverter
{
    int factor{ 1 };

    int FromJson(const Json::Value& value)
    {
        return std::stoi(value.asString()) * factor;
    }

    bool CanConvert(const Json::Value& /*value*/)
    {
        return true;
    }
};

enum class JsonTestEnum : int
{
    First = 0,
    Second,
    Third,
    Fourth,
    Fifth
};

JSON_ENUM_MAPPER(JsonTestEnum)
{
    JSON_MAPPINGS(5) = {
        pair_type{ "first", JsonTestEnum::First }, // DEFAULT
        pair_type{ "second", JsonTestEnum::Second },
        pair_type{ "third", JsonTestEnum::Third },
        pair_type{ "fourth", JsonTestEnum::Fourth },
        pair_type{ "fifth", JsonTestEnum::Fifth },
    };
};

enum class JsonTestFlags : int
{
    None = 0,
    First = 1 << 0,
    Second = 1 << 1,
    Third = 1 << 2,
    Fourth = 1 << 3,
    Fifth = 1 << 4,
    All = ~0,
};

DEFINE_ENUM_FLAG_OPERATORS(JsonTestFlags);

JSON_FLAG_MAPPER(JsonTestFlags)
{
    JSON_MAPPINGS(7) = {
        pair_type{ "none", AllClear },
        pair_type{ "first", JsonTestFlags::First },
        pair_type{ "second", JsonTestFlags::Second },
        pair_type{ "third", JsonTestFlags::Third },
        pair_type{ "fourth", JsonTestFlags::Fourth },
        pair_type{ "fifth", JsonTestFlags::Fifth },
        pair_type{ "all", AllSet },
    };
};

namespace TerminalAppUnitTests
{
    class JsonUtilsTests
    {
        TEST_CLASS(JsonUtilsTests);

        TEST_METHOD(DocumentedBehaviors_GetValue_Returning);
        TEST_METHOD(DocumentedBehaviors_GetValue_Filling);
        TEST_METHOD(DocumentedBehaviors_GetValueForKey_Returning);
        TEST_METHOD(DocumentedBehaviors_GetValueForKey_Filling);

        TEST_METHOD(BasicTypeConversion);
        TEST_METHOD(BasicTypeWithCustomConverter);
        TEST_METHOD(CustomTypeWithConverterSpecialization);
        TEST_METHOD(EnumMapper);
        TEST_METHOD(FlagMapper);

        TEST_METHOD(NestedExceptionDuringKeyParse);
    };

    template<typename T>
    static bool _ReturnTrueForException(T&& /*exception*/)
    {
        return true;
    }

    void JsonUtilsTests::DocumentedBehaviors_GetValue_Returning()
    {
        std::string expected{ "correct" };
        Json::Value object{ expected };

        //// 1. Bare Value ////
        //// 1.a. Type Invalid - Exception ////
        VERIFY_THROWS_SPECIFIC(GetValue<int>(object), TypeMismatchException, _ReturnTrueForException);

        //// 1.b. JSON NULL - Zero Value ////
        std::string zeroValueString{};
        VERIFY_ARE_EQUAL(zeroValueString, GetValue<std::string>(Json::Value::nullSingleton()));

        //// 1.c. Valid - Valid ////
        VERIFY_ARE_EQUAL(expected, GetValue<std::string>(object));

        //// 2. Optional ////
        //// 2.a. Type Invalid - Exception ////
        VERIFY_THROWS_SPECIFIC(GetValue<std::optional<int>>(object), TypeMismatchException, _ReturnTrueForException);

        //// 2.b. JSON NULL - nullopt ////
        VERIFY_ARE_EQUAL(std::nullopt, GetValue<std::optional<std::string>>(Json::Value::nullSingleton()));

        //// 2.c. Valid - Valid ////
        VERIFY_ARE_EQUAL(expected, GetValue<std::optional<std::string>>(object));
    }

    void JsonUtilsTests::DocumentedBehaviors_GetValue_Filling()
    {
        std::string expected{ "correct" };
        Json::Value object{ expected };

        //// 1. Bare Value ////
        int outputRedHerring{ 5 }; // explicitly not the zero value
        std::string output{ "sentinel" }; // explicitly not the zero value

        //// 1.a. Type Invalid - Exception ////
        VERIFY_THROWS_SPECIFIC(GetValue(object, outputRedHerring), TypeMismatchException, _ReturnTrueForException);
        VERIFY_ARE_EQUAL(5, outputRedHerring); // unchanged

        //// 1.b. JSON NULL - Unchanged ////
        VERIFY_IS_FALSE(GetValue(Json::Value::nullSingleton(), output)); // FALSE = storage not modified!
        VERIFY_ARE_EQUAL("sentinel", output); // unchanged

        //// 1.c. Valid ////
        VERIFY_IS_TRUE(GetValue(object, output));
        VERIFY_ARE_EQUAL(expected, output);

        //// 2. Optional ////
        std::optional<int> optionalOutputRedHerring{ 6 }; // explicitly not nullopt
        std::optional<std::string> optionalOutput{ "sentinel2" }; // explicitly not nullopt

        //// 2.a. Type Invalid - Exception ////
        VERIFY_THROWS_SPECIFIC(GetValue(object, optionalOutputRedHerring), TypeMismatchException, _ReturnTrueForException);
        VERIFY_ARE_EQUAL(6, optionalOutputRedHerring); // unchanged

        //// 2.b. JSON NULL - nullopt ////
        VERIFY_IS_TRUE(GetValue(Json::Value::nullSingleton(), optionalOutput)); // TRUE = storage modified!
        VERIFY_ARE_EQUAL(std::nullopt, optionalOutput); // changed to nullopt

        //// 2.c. Valid ////
        VERIFY_IS_TRUE(GetValue(object, optionalOutput));
        VERIFY_ARE_EQUAL(expected, optionalOutput);
    }

    void JsonUtilsTests::DocumentedBehaviors_GetValueForKey_Returning()
    {
        // These are mostly duplicates of the GetValue tests save for the additional case (d)
        std::string expected{ "correct" };
        std::string key{ "key" };
        std::string nullKey{ "nullKey" };
        std::string invalidKey{ "invalidKey" };

        Json::Value object{ Json::objectValue };
        object[key] = Json::Value{ expected };
        object[nullKey] = Json::Value::nullSingleton();

        //// 1. Bare Value ////
        //// 1.a. Type Invalid - Exception ////
        VERIFY_THROWS_SPECIFIC(GetValueForKey<int>(object, key), KeyedException, _ReturnTrueForException);

        //// 1.b. JSON NULL - Zero Value ////
        std::string zeroValueString{};
        VERIFY_ARE_EQUAL(zeroValueString, GetValueForKey<std::string>(object, nullKey));

        //// 1.c. Valid - Valid ////
        VERIFY_ARE_EQUAL(expected, GetValueForKey<std::string>(object, key));

        //// 1.d. Not Found - Zero Value ////
        VERIFY_ARE_EQUAL(zeroValueString, GetValueForKey<std::string>(object, invalidKey));

        //// 2. Optional ////
        //// 2.a. Type Invalid - Exception ////
        VERIFY_THROWS_SPECIFIC(GetValueForKey<std::optional<int>>(object, key), KeyedException, _ReturnTrueForException);

        //// 2.b. JSON NULL - nullopt ////
        VERIFY_ARE_EQUAL(std::nullopt, GetValueForKey<std::optional<std::string>>(object, nullKey));

        //// 2.c. Valid - Valid ////
        VERIFY_ARE_EQUAL(expected, GetValueForKey<std::optional<std::string>>(object, key));

        //// 2.d. Not Found - nullopt ////
        VERIFY_ARE_EQUAL(std::nullopt, GetValueForKey<std::optional<std::string>>(object, invalidKey));
    }

    void JsonUtilsTests::DocumentedBehaviors_GetValueForKey_Filling()
    {
        // These are mostly duplicates of the GetValue tests save for the additional case (d)
        std::string expected{ "correct" };
        std::string key{ "key" };
        std::string nullKey{ "nullKey" };
        std::string invalidKey{ "invalidKey" };

        Json::Value object{ Json::objectValue };
        object[key] = Json::Value{ expected };
        object[nullKey] = Json::Value::nullSingleton();

        //// 1. Bare Value ////
        int outputRedHerring{ 5 }; // explicitly not the zero value
        std::string output{ "sentinel" }; // explicitly not the zero value

        //// 1.a. Type Invalid - Exception ////
        VERIFY_THROWS_SPECIFIC(GetValueForKey(object, key, outputRedHerring), KeyedException, _ReturnTrueForException);
        VERIFY_ARE_EQUAL(5, outputRedHerring); // unchanged

        //// 1.b. JSON NULL - Unchanged ////
        VERIFY_IS_FALSE(GetValueForKey(object, nullKey, output)); // FALSE = storage not modified!
        VERIFY_ARE_EQUAL("sentinel", output); // unchanged

        //// 1.c. Valid ////
        VERIFY_IS_TRUE(GetValueForKey(object, key, output));
        VERIFY_ARE_EQUAL(expected, output);

        //// 1.d. Not Found - Unchanged ////
        // (restore the sentinel)
        output = "sentinel";
        VERIFY_IS_FALSE(GetValueForKey(object, invalidKey, output));
        VERIFY_ARE_EQUAL("sentinel", output);

        //// 2. Optional ////
        std::optional<int> optionalOutputRedHerring{ 6 }; // explicitly not nullopt
        std::optional<std::string> optionalOutput{ "sentinel2" }; // explicitly not nullopt

        //// 2.a. Type Invalid - Exception ////
        VERIFY_THROWS_SPECIFIC(GetValueForKey(object, key, optionalOutputRedHerring), KeyedException, _ReturnTrueForException);
        VERIFY_ARE_EQUAL(6, optionalOutputRedHerring); // unchanged

        //// 2.b. JSON NULL - nullopt ////
        VERIFY_IS_TRUE(GetValueForKey(object, nullKey, optionalOutput)); // TRUE = storage modified!
        VERIFY_ARE_EQUAL(std::nullopt, optionalOutput); // changed to nullopt

        //// 2.c. Valid ////
        VERIFY_IS_TRUE(GetValueForKey(object, key, optionalOutput));
        VERIFY_ARE_EQUAL(expected, optionalOutput);

        //// 2.d. Not Found - Unchanged ////
        optionalOutput = "sentinel";
        VERIFY_IS_FALSE(GetValueForKey(object, invalidKey, optionalOutput));
        VERIFY_ARE_EQUAL("sentinel", optionalOutput);
    }

    // Since we've established the filling and returning functions work,
    // we're going to focus on some type-specific tests and use the returning
    // versions.

    template<typename TExpected, typename TJson>
    static void TryBasicType(TExpected&& expected, TJson&& json)
    {
        Json::Value jsonObject{ json };
        const auto value{ GetValue<TExpected>(jsonObject) };
        VERIFY_ARE_EQUAL(expected, value, NoThrowString{}.Format(L"(type: %hs)", typeid(TExpected).name()));
    }

    void JsonUtilsTests::BasicTypeConversion()
    {
        // Battery of all basic types ;P
        TryBasicType(std::string{ "hello" }, "hello");
        TryBasicType(int{ -1024 }, -1024);
        TryBasicType(unsigned int{ 1024 }, 1024);
        TryBasicType(false, false);
        TryBasicType(1.0f, 1.0f);

        // string -> wstring
        TryBasicType(std::wstring{ L"hello" }, "hello");

        // float -> double
        TryBasicType(1.0, 1.0f);

        // double -> float
        TryBasicType(1.0f, 1.0);

        TryBasicType(til::color{ 0xab, 0xcd, 0xef }, "#abcdef");

        static const std::string testGuidString{ "{AA8147AA-E289-4508-BE83-FB68361EF2F3}" }; // can't use a string_view; jsoncpp hates it
        static const GUID testGuid{ 0xaa8147aa, 0xe289, 0x4508, { 0xbe, 0x83, 0xfb, 0x68, 0x36, 0x1e, 0xf2, 0xf3 } };

        TryBasicType(testGuid, testGuidString);

        VERIFY_THROWS_SPECIFIC(GetValue<GUID>({ "NOT_A_GUID" }), TypeMismatchException, _ReturnTrueForException);
        VERIFY_THROWS_SPECIFIC(GetValue<GUID>({ "{too short for a guid but just a bit}" }), TypeMismatchException, _ReturnTrueForException);
        VERIFY_THROWS_SPECIFIC(GetValue<GUID>({ "{proper length string not a guid tho?}" }), std::exception, _ReturnTrueForException);

        VERIFY_THROWS_SPECIFIC(GetValue<til::color>({ "#" }), TypeMismatchException, _ReturnTrueForException);
        VERIFY_THROWS_SPECIFIC(GetValue<til::color>({ "#1234567890" }), TypeMismatchException, _ReturnTrueForException);
    }

    void JsonUtilsTests::BasicTypeWithCustomConverter()
    {
        // { "key": "100" }
        Json::Value object{ Json::objectValue };
        object["key"] = Json::Value{ "100" };

        //// Temporary (rvalue) Converter ////
        VERIFY_ARE_EQUAL(100, GetValue<int>(object["key"], CustomConverter{}));
        VERIFY_ARE_EQUAL(100, GetValueForKey<int>(object, "key", CustomConverter{}));

        //// lvalue converter ////
        CustomConverter converterWithFactor{ 2 };
        VERIFY_ARE_EQUAL(200, GetValue<int>(object["key"], converterWithFactor));
        VERIFY_ARE_EQUAL(200, GetValueForKey<int>(object, "key", converterWithFactor));
    }

    void JsonUtilsTests::CustomTypeWithConverterSpecialization()
    {
        // { "key": 1024 }
        Json::Value object{ Json::objectValue };
        object["key"] = Json::Value{ 1024 };

        VERIFY_ARE_EQUAL(StructWithConverterSpecialization{ 1024 }, GetValue<StructWithConverterSpecialization>(object["key"]));
        VERIFY_ARE_EQUAL(StructWithConverterSpecialization{ 1024 }, GetValueForKey<StructWithConverterSpecialization>(object, "key"));
    }

    void JsonUtilsTests::EnumMapper()
    {
        // Basic string
        Json::Value stringFirst{ "first" };
        VERIFY_ARE_EQUAL(JsonTestEnum::First, GetValue<JsonTestEnum>(stringFirst));

        Json::Value stringSecond{ "second" };
        VERIFY_ARE_EQUAL(JsonTestEnum::Second, GetValue<JsonTestEnum>(stringSecond));

        // Unknown value should produce something?
        Json::Value stringUnknown{ "unknown" };
        VERIFY_THROWS_SPECIFIC(GetValue<JsonTestEnum>(stringUnknown), UnexpectedValueException, _ReturnTrueForException);
    }

    void JsonUtilsTests::FlagMapper()
    {
        // One flag
        Json::Value stringFirst{ "first" };
        VERIFY_ARE_EQUAL(JsonTestFlags::First, GetValue<JsonTestFlags>(stringFirst));

        Json::Value stringSecond{ "second" };
        VERIFY_ARE_EQUAL(JsonTestFlags::Second, GetValue<JsonTestFlags>(stringSecond));

        Json::Value stringAll{ "all" };
        VERIFY_ARE_EQUAL(JsonTestFlags::All, GetValue<JsonTestFlags>(stringAll));

        // Multiple flags
        Json::Value arrayFirstSecond{ Json::arrayValue };
        arrayFirstSecond.append({ "first" });
        arrayFirstSecond.append({ "second" });
        VERIFY_ARE_EQUAL(JsonTestFlags::First | JsonTestFlags::Second, GetValue<JsonTestFlags>(arrayFirstSecond));

        // No flags
        Json::Value emptyArray{ Json::arrayValue };
        VERIFY_ARE_EQUAL(JsonTestFlags::None, GetValue<JsonTestFlags>(emptyArray));

        // Stacking Always + Any
        Json::Value arrayAllFirst{ Json::arrayValue };
        arrayAllFirst.append({ "all" });
        arrayAllFirst.append({ "first" });
        VERIFY_ARE_EQUAL(JsonTestFlags::All, GetValue<JsonTestFlags>(arrayAllFirst));

        // Stacking None + Any (Exception)
        Json::Value arrayNoneFirst{ Json::arrayValue };
        arrayNoneFirst.append({ "none" });
        arrayNoneFirst.append({ "first" });
        VERIFY_THROWS_SPECIFIC(GetValue<JsonTestFlags>(arrayNoneFirst), UnexpectedValueException, _ReturnTrueForException);

        // Stacking Any + None (Exception; same as above, different order)
        Json::Value arrayFirstNone{ Json::arrayValue };
        arrayFirstNone.append({ "first" });
        arrayFirstNone.append({ "none" });
        VERIFY_THROWS_SPECIFIC(GetValue<JsonTestFlags>(arrayFirstNone), UnexpectedValueException, _ReturnTrueForException);

        // Unknown flag value?
        Json::Value stringUnknown{ "unknown" };
        VERIFY_THROWS_SPECIFIC(GetValue<JsonTestFlags>(stringUnknown), UnexpectedValueException, _ReturnTrueForException);
    }

    void JsonUtilsTests::NestedExceptionDuringKeyParse()
    {
        std::string key{ "key" };
        Json::Value object{ Json::objectValue };
        object[key] = Json::Value{ "string" };

        auto CheckKeyedException = [](const KeyedException& k) {
            try
            {
                k.RethrowInner();
            }
            catch (TypeMismatchException&)
            {
                // This Keyed exception contained the correct inner.
                return true;
            }
            // This Keyed exception did not contain the correct inner.
            return false;
        };
        VERIFY_THROWS_SPECIFIC(GetValueForKey<int>(object, key), KeyedException, CheckKeyedException);
    }

}
