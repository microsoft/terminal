use terminal_settings::settings_json::{self, JsonMember, JsonValue, LayerValue};

#[test]
fn settings_json_distinguishes_missing_null_and_value() {
    let document = settings_json::parse(r#"{ "explicit": null, "value": "set" }"#)
        .expect("settings JSON should parse");
    let object = document.as_object().expect("root should be an object");

    assert_eq!(
        JsonMember::from_object(object, "missing"),
        JsonMember::Missing
    );
    assert_eq!(
        JsonMember::from_object(object, "explicit"),
        JsonMember::Null
    );
    assert!(matches!(
        JsonMember::from_object(object, "value"),
        JsonMember::Value(JsonValue::String(value)) if value == "set"
    ));
}

#[test]
fn settings_json_accepts_jsonc_and_trailing_commas() {
    let document = settings_json::parse(
        r#"{
            // Windows Terminal settings are JSONC.
            "enabled": true,
            "items": [1, 2, 3,],
            /* JsonCpp also accepts this trailing object comma. */
        }"#,
    )
    .expect("JSONC settings should parse");

    let object = document.as_object().expect("root should be an object");
    assert_eq!(
        object.get("enabled").and_then(JsonValue::as_bool),
        Some(true)
    );
    let items = object
        .get("items")
        .and_then(JsonValue::as_array)
        .expect("items should be an array");
    assert_eq!(items.len(), 3);
    assert_eq!(items[2].as_f64(), Some(3.0));
}

#[test]
fn settings_json_decodes_unicode_surrogate_pairs() {
    let document =
        settings_json::parse(r#"{ "icon": "\uD83D\uDE80" }"#).expect("surrogate pair should parse");
    let object = document.as_object().expect("root should be an object");
    assert_eq!(object.get("icon").and_then(JsonValue::as_str), Some("🚀"));
}

#[test]
fn settings_layer_preserves_inherit_but_applies_null_and_value() {
    let mut setting = LayerValue::Value("defaults");
    setting.overlay(LayerValue::Inherit);
    assert_eq!(setting, LayerValue::Value("defaults"));

    setting.overlay(LayerValue::Null);
    assert_eq!(setting, LayerValue::Null);

    setting.overlay(LayerValue::Value("user"));
    assert_eq!(setting, LayerValue::Value("user"));
}
