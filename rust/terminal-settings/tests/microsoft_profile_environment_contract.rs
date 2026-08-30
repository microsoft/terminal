use terminal_settings::profile::Profile;

#[test]
fn microsoft_profile_with_env_vars_preserves_entries_verbatim() {
    let profile = Profile::from_json(
        r#"{
            "name": "profile0",
            "guid": "{6239a42c-0000-49a3-80bd-e8fdd045185c}",
            "environment": {
                "VAR_1": "value1",
                "VAR_2": "value2",
                "VAR_3": "%VAR_3%;value3"
            }
        }"#,
    )
    .unwrap();

    let environment = profile.environment_variables();
    assert_eq!(environment.len(), 3);
    assert_eq!(environment.get("VAR_1").map(String::as_str), Some("value1"));
    assert_eq!(environment.get("VAR_2").map(String::as_str), Some("value2"));
    assert_eq!(
        environment.get("VAR_3").map(String::as_str),
        Some("%VAR_3%;value3")
    );
}
