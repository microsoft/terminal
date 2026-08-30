use terminal_settings::action_map::ActionMapDocument;

#[test]
fn microsoft_serialization_generated_action_ids_equal_for_identical_commands_contract() {
    // Exact Microsoft SerializationTests::GeneratedActionIDsEqualForIdenticalCommands
    // vector. Both user settings documents define the same legacy sendInput action,
    // so the generated action ID must be deterministic and identical.
    let settings_json_1 = r#"
    {
        "actions": [
            {
                "name": "foo",
                "command": { "action": "sendInput", "input": "this is some other input string" },
                "keys": "ctrl+shift+w"
            }
        ]
    }
    "#;

    let settings_json_2 = r#"
    {
        "actions": [
            {
                "name": "foo",
                "command": { "action": "sendInput", "input": "this is some other input string" },
                "keys": "ctrl+shift+w"
            }
        ]
    }
    "#;

    let action_map_1 = ActionMapDocument::from_json(settings_json_1)
        .expect("first Microsoft action vector must parse");
    let action_map_2 = ActionMapDocument::from_json(settings_json_2)
        .expect("second Microsoft action vector must parse");

    assert!(action_map_1.fixups_applied_during_load().unwrap());
    assert!(action_map_2.fixups_applied_during_load().unwrap());

    let id_1 = action_map_1
        .action_id_for_key_chord("ctrl+shift+w")
        .expect("first generated ID lookup must succeed")
        .expect("first legacy key chord must resolve an action");
    let id_2 = action_map_2
        .action_id_for_key_chord("ctrl+shift+w")
        .expect("second generated ID lookup must succeed")
        .expect("second legacy key chord must resolve an action");

    assert_eq!(id_1, id_2, "identical commands must generate identical IDs");
    assert!(
        id_1.starts_with("User.sendInput."),
        "the deterministic ID must remain in Microsoft's generated-ID namespace"
    );
}
