use terminal_settings::action_map::ActionMapDocument;

#[test]
fn microsoft_serialization_no_generated_ids_for_iterable_and_nested_commands_contract() {
    // Exact Microsoft SerializationTests::NoGeneratedIDsForIterableAndNestedCommands
    // vector. SettingsLoader must not request a user-settings write-back for any
    // of these three shapes: explicit ID, iterable command, or nested group.
    let settings = r#"
    {
        "actions": [
            {
                "name": "foo",
                "command": "closePane",
                "id": "thisIsMyClosePane"
            },
            {
                "iterateOn": "profiles",
                "icon": "${profile.icon}",
                "name": "${profile.name}",
                "command": { "action": "newTab", "profile": "${profile.name}" }
            },
            {
                "name": "Change font size...",
                "commands": [
                    { "command": { "action": "adjustFontSize", "delta": 1 } },
                    { "command": { "action": "adjustFontSize", "delta": -1 } },
                    { "command": "resetFontSize" }
                ]
            }
        ]
    }
    "#;

    let action_map = ActionMapDocument::from_json(settings)
        .expect("Microsoft action vector must be accepted by the safe Rust ActionMap owner");

    assert!(
        !action_map
            .fixups_applied_during_load()
            .expect("valid Microsoft actions have a deterministic fixup decision"),
        "explicit-ID, iterable and nested commands must not trigger generated-ID fixups"
    );

    // Preserve the positive half of Microsoft's native LayerJson condition so
    // this witness cannot pass through an implementation that simply returns
    // false for every document.
    let missing_id =
        ActionMapDocument::from_json(r#"{ "actions": [ { "command": "closePane" } ] }"#)
            .expect("ordinary user command is valid");
    assert!(missing_id.fixups_applied_during_load().unwrap());

    let legacy_keys = ActionMapDocument::from_json(
        r#"{ "actions": [ { "command": "closePane", "id": "User.Close", "keys": "ctrl+w" } ] }"#,
    )
    .expect("legacy inline keys form is valid");
    assert!(legacy_keys.fixups_applied_during_load().unwrap());
}
