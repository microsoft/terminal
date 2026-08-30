use terminal_settings::action_map::ActionMapDocument;

#[test]
fn microsoft_serialization_roundtrip_generate_action_id_contract() {
    let old_settings_action_map = r#"
    {
        "actions": [
            {
                "name": "foo",
                "command": { "action": "sendInput", "input": "just some input" },
                "keys": "ctrl+shift+w"
            }
        ]
    }
    "#;

    let action_map = ActionMapDocument::from_json(old_settings_action_map)
        .expect("Microsoft legacy action vector is a valid ActionMap");
    let generated = action_map
        .action_id_for_key_chord("ctrl+shift+w")
        .expect("Microsoft sendInput arguments have portable generated-ID semantics")
        .expect("legacy key chord resolves to the generated action");

    let expected = if usize::BITS == 32 {
        "User.sendInput.56911147"
    } else {
        "User.sendInput.A020D2"
    };

    assert_eq!(generated, expected);
    assert_eq!(
        action_map
            .action_id_for_key_chord("ctrl+shift+w")
            .expect("repeat lookup remains valid")
            .as_deref(),
        Some(expected)
    );
    assert_eq!(
        action_map
            .action_id_for_key_chord("ctrl+shift+x")
            .expect("missing key chord is not an error"),
        None
    );
}
