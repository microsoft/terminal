use terminal_settings::profile_duplication::ProfileDuplicationSettings;

#[test]
fn microsoft_duplicate_profile_preserves_local_json_and_inherits_profile_defaults() {
    let user = r#"{
        "profiles": {
            "defaults": {
                "font": {
                    "size": 123
                }
            },
            "list": [
                {
                    "name": "profile0",
                    "guid": "{6239a42c-0000-49a3-80bd-e8fdd045185c}",
                    "backgroundImage": "file:///some/path",
                    "hidden": false
                }
            ]
        }
    }"#;

    let settings = ProfileDuplicationSettings::from_json(user).unwrap();
    let source = &settings.profiles()[0];
    let mut duplicate = settings.duplicate_profile(0).unwrap();

    // GH#11392: the duplicate must continue to resolve nested profile defaults
    // without materializing them into the duplicate's local serialized layer.
    assert_eq!(duplicate.font_size().unwrap(), Some(123));
    assert_eq!(source.font_size().unwrap(), Some(123));

    // Microsoft overwrites the freshly-created copy identity before comparing
    // the serialized profile payload. Do exactly the same at this deterministic
    // boundary; random GUID creation and localized "copy" naming remain native/UI.
    duplicate.set_guid_text(source.guid_text().unwrap());
    duplicate.set_name(source.name().unwrap());

    assert_eq!(duplicate.to_json(), source.to_json());
    assert_eq!(
        duplicate.to_json().get("backgroundImage"),
        source.to_json().get("backgroundImage")
    );
    assert_eq!(
        duplicate.to_json().get("hidden"),
        source.to_json().get("hidden")
    );
    assert!(
        duplicate.to_json().get("font").is_none(),
        "profiles.defaults font settings must remain inherited"
    );
}
