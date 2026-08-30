use terminal_settings::profile::Profile;

#[test]
fn microsoft_profile_layer_icon_preserves_null_and_missing_semantics() {
    let mut profile = Profile::from_json(
        r#"{
            "name": "profile0",
            "guid": "{6239a42c-1111-49a3-80bd-e8fdd045185c}",
            "icon": "not-null.png"
        }"#,
    )
    .unwrap();
    assert_eq!(profile.icon_path(), "not-null.png");

    profile
        .layer_json(
            r#"{
                "name": "profile1",
                "guid": "{6239a42c-1111-49a3-80bd-e8fdd045185c}",
                "icon": null
            }"#,
        )
        .unwrap();
    assert_eq!(profile.icon_path(), "");

    profile
        .layer_json(
            r#"{
                "name": "profile2",
                "guid": "{6239a42c-1111-49a3-80bd-e8fdd045185c}"
            }"#,
        )
        .unwrap();
    assert_eq!(profile.icon_path(), "");

    profile
        .layer_json(
            r#"{
                "name": "profile3",
                "guid": "{6239a42c-1111-49a3-80bd-e8fdd045185c}",
                "icon": "another-real.png"
            }"#,
        )
        .unwrap();
    assert_eq!(profile.icon_path(), "another-real.png");

    profile
        .layer_json(
            r#"{
                "name": "profile2",
                "guid": "{6239a42c-1111-49a3-80bd-e8fdd045185c}"
            }"#,
        )
        .unwrap();
    assert_eq!(profile.icon_path(), "another-real.png");

    let mut null_profile = Profile::from_json(
        r#"{
            "name": "profile1",
            "guid": "{6239a42c-1111-49a3-80bd-e8fdd045185c}",
            "icon": null
        }"#,
    )
    .unwrap();
    assert_eq!(null_profile.icon_path(), "");
    null_profile
        .layer_json(
            r#"{
                "name": "profile3",
                "guid": "{6239a42c-1111-49a3-80bd-e8fdd045185c}",
                "icon": "another-real.png"
            }"#,
        )
        .unwrap();
    assert_eq!(null_profile.icon_path(), "another-real.png");
}
