#![allow(clippy::needless_pass_by_value)]

use terminal_settings::json_utils::{
    DeserializationError, DeserializationErrorKind, EnumMapper, FlagMapper, Guid, JsonConversion,
    JsonConverter, RgbColor, WideString, fill_value, fill_value_for_key, get_value,
    get_value_for_key, get_value_for_key_with, get_value_with, object, set_optional_for_key,
    set_value_for_key,
};
use terminal_settings::settings_json::JsonValue;

#[test]
fn microsoft_json_utils_documented_behaviors_get_value_returning_contract() {
    let value = JsonValue::String("correct".to_owned());
    assert!(get_value::<i32>(&value).is_err());
    assert!(get_value::<String>(&JsonValue::Null).is_err());
    assert_eq!(get_value::<String>(&value).unwrap(), "correct");

    assert!(get_value::<Option<i32>>(&value).is_err());
    assert_eq!(get_value::<Option<String>>(&JsonValue::Null).unwrap(), None);
    assert_eq!(
        get_value::<Option<String>>(&value).unwrap(),
        Some("correct".to_owned())
    );
}

#[test]
fn microsoft_json_utils_documented_behaviors_get_value_filling_contract() {
    let value = JsonValue::String("correct".to_owned());
    let mut integer = 5_i32;
    assert!(fill_value(&value, &mut integer).is_err());
    assert_eq!(integer, 5);

    let mut text = "sentinel".to_owned();
    assert!(fill_value(&JsonValue::Null, &mut text).is_err());
    assert_eq!(text, "sentinel");
    assert!(fill_value(&value, &mut text).unwrap());
    assert_eq!(text, "correct");

    let mut optional_integer = Some(6_i32);
    assert!(fill_value(&value, &mut optional_integer).is_err());
    assert_eq!(optional_integer, Some(6));

    let mut optional_text = Some("sentinel2".to_owned());
    assert!(fill_value(&JsonValue::Null, &mut optional_text).unwrap());
    assert_eq!(optional_text, None);
    assert!(fill_value(&value, &mut optional_text).unwrap());
    assert_eq!(optional_text, Some("correct".to_owned()));
}

#[test]
fn microsoft_json_utils_documented_behaviors_get_value_for_key_returning_contract() {
    let object = object([
        ("key", JsonValue::String("correct".to_owned())),
        ("nullKey", JsonValue::Null),
    ]);

    assert!(get_value_for_key::<i32>(&object, "key").is_err());
    assert!(get_value_for_key::<String>(&object, "nullKey").is_err());
    assert_eq!(
        get_value_for_key::<String>(&object, "key").unwrap(),
        "correct"
    );
    assert_eq!(
        get_value_for_key::<String>(&object, "invalidKey").unwrap(),
        ""
    );

    assert!(get_value_for_key::<Option<i32>>(&object, "key").is_err());
    assert_eq!(
        get_value_for_key::<Option<String>>(&object, "nullKey").unwrap(),
        None
    );
    assert_eq!(
        get_value_for_key::<Option<String>>(&object, "key").unwrap(),
        Some("correct".to_owned())
    );
    assert_eq!(
        get_value_for_key::<Option<String>>(&object, "invalidKey").unwrap(),
        None
    );
}

#[test]
fn microsoft_json_utils_documented_behaviors_get_value_for_key_filling_contract() {
    let object = object([
        ("key", JsonValue::String("correct".to_owned())),
        ("nullKey", JsonValue::Null),
    ]);

    let mut integer = 5_i32;
    assert!(fill_value_for_key(&object, "key", &mut integer).is_err());
    assert_eq!(integer, 5);

    let mut text = "sentinel".to_owned();
    assert!(fill_value_for_key(&object, "nullKey", &mut text).is_err());
    assert!(fill_value_for_key(&object, "key", &mut text).unwrap());
    assert_eq!(text, "correct");
    text = "sentinel".to_owned();
    assert!(!fill_value_for_key(&object, "invalidKey", &mut text).unwrap());
    assert_eq!(text, "sentinel");

    let mut optional_integer = Some(6_i32);
    assert!(fill_value_for_key(&object, "key", &mut optional_integer).is_err());
    assert_eq!(optional_integer, Some(6));

    let mut optional_text = Some("sentinel2".to_owned());
    assert!(fill_value_for_key(&object, "nullKey", &mut optional_text).unwrap());
    assert_eq!(optional_text, None);
    assert!(fill_value_for_key(&object, "key", &mut optional_text).unwrap());
    assert_eq!(optional_text, Some("correct".to_owned()));
    optional_text = Some("sentinel".to_owned());
    assert!(!fill_value_for_key(&object, "invalidKey", &mut optional_text).unwrap());
    assert_eq!(optional_text, Some("sentinel".to_owned()));
}

fn roundtrip<T>(expected: T, json: JsonValue, serialized: Option<JsonValue>)
where
    T: JsonConversion + PartialEq + std::fmt::Debug,
{
    assert_eq!(get_value::<T>(&json).unwrap(), expected);
    let mut object = object([] as [(&str, JsonValue); 0]);
    set_value_for_key(&mut object, "myKey", &expected);
    assert_eq!(
        object.get("myKey"),
        Some(serialized.as_ref().unwrap_or(&json))
    );
}

#[test]
fn microsoft_json_utils_basic_type_conversion_contract() {
    roundtrip(
        "hello".to_owned(),
        JsonValue::String("hello".to_owned()),
        None,
    );
    roundtrip(-1024_i32, JsonValue::Number(-1024.0), None);
    roundtrip(u32::MAX, JsonValue::Number(f64::from(u32::MAX)), None);
    roundtrip(false, JsonValue::Bool(false), None);
    roundtrip(1.1_f32, JsonValue::Number(f64::from(1.1_f32)), None);
    roundtrip(
        WideString("hello".to_owned()),
        JsonValue::String("hello".to_owned()),
        None,
    );
    roundtrip(
        f64::from(1.1_f32),
        JsonValue::Number(f64::from(1.1_f32)),
        None,
    );
    roundtrip(1.1_f32, JsonValue::Number(f64::from(1.1_f32)), None);
    roundtrip(
        RgbColor {
            r: 0xab,
            g: 0xcd,
            b: 0xef,
        },
        JsonValue::String("#ABCDEF".to_owned()),
        None,
    );
    roundtrip(
        RgbColor {
            r: 0xcc,
            g: 0xcc,
            b: 0xcc,
        },
        JsonValue::String("#CCC".to_owned()),
        Some(JsonValue::String("#CCCCCC".to_owned())),
    );

    let guid_text = "{aa8147aa-e289-4508-be83-fb68361ef2f3}";
    let guid = Guid::parse(guid_text).unwrap();
    roundtrip(guid, JsonValue::String(guid_text.to_owned()), None);
    assert!(get_value::<Guid>(&JsonValue::String("NOT_A_GUID".to_owned())).is_err());
    assert!(
        get_value::<Guid>(&JsonValue::String(
            "{too short for a guid but just a bit}".to_owned()
        ))
        .is_err()
    );
    assert!(
        get_value::<Guid>(&JsonValue::String(
            "{proper length string not a guid tho?}".to_owned()
        ))
        .is_err()
    );
    assert!(get_value::<RgbColor>(&JsonValue::String("#".to_owned())).is_err());
    assert!(get_value::<RgbColor>(&JsonValue::String("#1234567890".to_owned())).is_err());
}

struct FactorConverter(i32);

impl JsonConverter<i32> for FactorConverter {
    fn from_json(&self, value: &JsonValue) -> Result<i32, DeserializationError> {
        let text = value
            .as_str()
            .ok_or_else(|| DeserializationError::new(DeserializationErrorKind::TypeMismatch))?;
        let value = text
            .parse::<i32>()
            .map_err(|_| DeserializationError::new(DeserializationErrorKind::InvalidValue))?;
        Ok(value * self.0)
    }
}

#[test]
fn microsoft_json_utils_basic_type_with_custom_converter_contract() {
    let object = object([("key", JsonValue::String("100".to_owned()))]);
    assert_eq!(
        get_value_with(object.get("key").unwrap(), &FactorConverter(1)).unwrap(),
        100
    );
    assert_eq!(
        get_value_for_key_with(&object, "key", &FactorConverter(1)).unwrap(),
        100
    );
    let converter = FactorConverter(2);
    assert_eq!(
        get_value_with(object.get("key").unwrap(), &converter).unwrap(),
        200
    );
    assert_eq!(
        get_value_for_key_with(&object, "key", &converter).unwrap(),
        200
    );
}

#[derive(Debug, Clone, Default, PartialEq, Eq)]
struct CustomStruct {
    value: i32,
}

impl JsonConversion for CustomStruct {
    fn from_json(value: &JsonValue) -> Result<Self, DeserializationError> {
        Ok(Self {
            value: get_value::<i32>(value)?,
        })
    }

    fn to_json(&self) -> JsonValue {
        self.value.to_json()
    }
}

#[test]
fn microsoft_json_utils_custom_type_with_converter_specialization_contract() {
    let object = object([("key", JsonValue::Number(1024.0))]);
    assert_eq!(
        get_value::<CustomStruct>(object.get("key").unwrap()).unwrap(),
        CustomStruct { value: 1024 }
    );
    assert_eq!(
        get_value_for_key::<CustomStruct>(&object, "key").unwrap(),
        CustomStruct { value: 1024 }
    );
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
enum TestEnum {
    First,
    Second,
    Third,
    Fourth,
    Fifth,
}

const ENUM_MAPPER: EnumMapper<TestEnum> = EnumMapper::new(&[
    ("first", TestEnum::First),
    ("second", TestEnum::Second),
    ("third", TestEnum::Third),
    ("fourth", TestEnum::Fourth),
    ("fifth", TestEnum::Fifth),
]);

#[test]
fn microsoft_json_utils_enum_mapper_contract() {
    assert_eq!(
        ENUM_MAPPER
            .from_json(&JsonValue::String("first".to_owned()))
            .unwrap(),
        TestEnum::First
    );
    assert_eq!(
        ENUM_MAPPER
            .from_json(&JsonValue::String("second".to_owned()))
            .unwrap(),
        TestEnum::Second
    );
    assert!(
        ENUM_MAPPER
            .from_json(&JsonValue::String("unknown".to_owned()))
            .is_err()
    );
    assert_eq!(
        ENUM_MAPPER.to_json(TestEnum::Third).unwrap(),
        JsonValue::String("third".to_owned())
    );
}

const FIRST: u32 = 1 << 0;
const SECOND: u32 = 1 << 1;
const THIRD: u32 = 1 << 2;
const FOURTH: u32 = 1 << 3;
const FIFTH: u32 = 1 << 4;
const FLAG_MAPPER: FlagMapper = FlagMapper::new(
    &[
        ("first", FIRST),
        ("second", SECOND),
        ("third", THIRD),
        ("fourth", FOURTH),
        ("fifth", FIFTH),
    ],
    "none",
    "all",
    u32::MAX,
);

#[test]
fn microsoft_json_utils_flag_mapper_contract() {
    assert_eq!(
        FLAG_MAPPER
            .from_json(&JsonValue::String("first".to_owned()))
            .unwrap(),
        FIRST
    );
    assert_eq!(
        FLAG_MAPPER
            .from_json(&JsonValue::String("second".to_owned()))
            .unwrap(),
        SECOND
    );
    assert_eq!(
        FLAG_MAPPER
            .from_json(&JsonValue::String("all".to_owned()))
            .unwrap(),
        u32::MAX
    );
    assert_eq!(
        FLAG_MAPPER.to_json(THIRD).unwrap(),
        JsonValue::String("third".to_owned())
    );
    assert_eq!(
        FLAG_MAPPER.to_json(u32::MAX).unwrap(),
        JsonValue::String("all".to_owned())
    );

    let first_second = JsonValue::Array(vec![
        JsonValue::String("first".to_owned()),
        JsonValue::String("second".to_owned()),
    ]);
    assert_eq!(
        FLAG_MAPPER.from_json(&first_second).unwrap(),
        FIRST | SECOND
    );
    assert_eq!(FLAG_MAPPER.to_json(FIRST | SECOND).unwrap(), first_second);
    assert_eq!(FLAG_MAPPER.from_json(&JsonValue::Array(vec![])).unwrap(), 0);
    assert_eq!(
        FLAG_MAPPER.to_json(0).unwrap(),
        JsonValue::String("none".to_owned())
    );
    assert_eq!(
        FLAG_MAPPER
            .from_json(&JsonValue::Array(vec![
                JsonValue::String("all".to_owned()),
                JsonValue::String("first".to_owned()),
            ]))
            .unwrap(),
        u32::MAX
    );
    assert!(
        FLAG_MAPPER
            .from_json(&JsonValue::Array(vec![
                JsonValue::String("none".to_owned()),
                JsonValue::String("first".to_owned()),
            ]))
            .is_err()
    );
    assert!(
        FLAG_MAPPER
            .from_json(&JsonValue::Array(vec![
                JsonValue::String("first".to_owned()),
                JsonValue::String("none".to_owned()),
            ]))
            .is_err()
    );
    assert!(
        FLAG_MAPPER
            .from_json(&JsonValue::String("unknown".to_owned()))
            .is_err()
    );
}

#[test]
fn microsoft_json_utils_nested_exception_during_key_parse_contract() {
    let object = object([("key", JsonValue::String("string".to_owned()))]);
    let error = get_value_for_key::<i32>(&object, "key").unwrap_err();
    assert_eq!(error.key.as_deref(), Some("key"));
}

#[derive(Debug, Clone, Default, PartialEq, Eq)]
struct HStringLike(String);

impl JsonConversion for HStringLike {
    fn from_json(value: &JsonValue) -> Result<Self, DeserializationError> {
        match value {
            JsonValue::Null => Ok(Self(String::new())),
            JsonValue::String(value) => Ok(Self(value.clone())),
            _ => Err(DeserializationError::new(
                DeserializationErrorKind::TypeMismatch,
            )),
        }
    }

    fn to_json(&self) -> JsonValue {
        if self.0.is_empty() {
            JsonValue::Null
        } else {
            JsonValue::String(self.0.clone())
        }
    }

    fn accepts_null() -> bool {
        true
    }
}

#[test]
fn microsoft_json_utils_set_value_hstring_like_contract() {
    let first = HStringLike(String::new());
    let second = HStringLike("second".to_owned());
    let third = Some(HStringLike(String::new()));
    let fourth = Some(HStringLike("fourth".to_owned()));
    let fifth: Option<HStringLike> = None;
    let mut object = object([] as [(&str, JsonValue); 0]);

    set_value_for_key(&mut object, "first", &first);
    set_value_for_key(&mut object, "second", &second);
    set_optional_for_key(&mut object, "third", &third);
    set_optional_for_key(&mut object, "fourth", &fourth);
    set_optional_for_key(&mut object, "fifth", &fifth);

    assert_eq!(object.get("first"), Some(&JsonValue::Null));
    assert_eq!(
        object.get("second"),
        Some(&JsonValue::String("second".to_owned()))
    );
    assert_eq!(object.get("third"), Some(&JsonValue::Null));
    assert_eq!(
        object.get("fourth"),
        Some(&JsonValue::String("fourth".to_owned()))
    );
    assert!(!object.contains_key("fifth"));
}

#[test]
fn microsoft_json_utils_get_value_hstring_like_contract() {
    let object = object([
        ("string", JsonValue::String("string".to_owned())),
        ("null", JsonValue::Null),
    ]);

    let mut value = HStringLike::default();
    assert!(fill_value_for_key(&object, "string", &mut value).unwrap());
    assert_eq!(value, HStringLike("string".to_owned()));
    assert!(fill_value_for_key(&object, "null", &mut value).unwrap());
    assert_eq!(value, HStringLike(String::new()));
    assert!(!fill_value_for_key(&object, "nonexistent", &mut value).unwrap());

    let mut optional = None::<HStringLike>;
    assert!(fill_value_for_key(&object, "string", &mut optional).unwrap());
    assert_eq!(optional, Some(HStringLike("string".to_owned())));
    optional = None;
    assert!(fill_value_for_key(&object, "null", &mut optional).unwrap());
    assert_eq!(optional, Some(HStringLike(String::new())));
    optional = None;
    assert!(!fill_value_for_key(&object, "nonexistent", &mut optional).unwrap());
    assert_eq!(optional, None);
}

#[test]
fn microsoft_json_utils_double_optional_contract() {
    let first: Option<Option<i32>> = None;
    let second: Option<Option<i32>> = Some(None);
    let third: Option<Option<i32>> = Some(Some(3));
    let mut object = object([] as [(&str, JsonValue); 0]);

    set_optional_for_key(&mut object, "first", &first);
    set_optional_for_key(&mut object, "second", &second);
    set_optional_for_key(&mut object, "third", &third);

    assert!(!object.contains_key("first"));
    assert_eq!(object.get("second"), Some(&JsonValue::Null));
    assert_eq!(object.get("third"), Some(&JsonValue::Number(3.0)));

    let mut first_out = None::<Option<i32>>;
    let mut second_out = None::<Option<i32>>;
    let mut third_out = None::<Option<i32>>;
    assert!(!fill_value_for_key(&object, "first", &mut first_out).unwrap());
    assert_eq!(first_out, None);
    assert!(fill_value_for_key(&object, "second", &mut second_out).unwrap());
    assert_eq!(second_out, Some(None));
    assert!(fill_value_for_key(&object, "third", &mut third_out).unwrap());
    assert_eq!(third_out, Some(Some(3)));
}
