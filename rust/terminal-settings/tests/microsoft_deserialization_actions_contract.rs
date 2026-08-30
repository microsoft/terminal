use terminal_settings::deserialization_actions::{
    DeserializationActionWarning, DeserializedActionMap, SplitDirection,
};

#[test]
fn microsoft_deserialization_commands_and_keybindings_contract() {
    let mut actions = DeserializedActionMap::new();
    actions
        .layer_settings(
            r#"{
                "actions": [
                    { "keys":"ctrl+a",                  "command":{ "action":"splitPane", "split":"vertical" } },
                    {                  "name":"ctrl+b", "command":{ "action":"splitPane", "split":"vertical" } },
                    { "keys":"ctrl+c", "name":"ctrl+c", "command":{ "action":"splitPane", "split":"vertical" } },
                    { "keys":"ctrl+d",                  "command":{ "action":"splitPane", "split":"vertical" } },
                    { "keys":"ctrl+e",                  "command":{ "action":"splitPane", "split":"horizontal" } },
                    { "keys":"ctrl+f", "name":null,     "command":{ "action":"splitPane", "split":"horizontal" } }
                ]
            }"#,
        )
        .expect("Microsoft actions/keybindings document parses");

    assert_eq!(actions.keybinding_count(), 5);
    assert_eq!(actions.name_count(), 1);
    assert_eq!(
        actions.split_direction_for_key("ctrl+a"),
        Some(SplitDirection::Right)
    );
    assert_eq!(
        actions.split_direction_for_key("ctrl+c"),
        Some(SplitDirection::Right)
    );
    assert_eq!(
        actions.split_direction_for_key("ctrl+d"),
        Some(SplitDirection::Right)
    );
    assert_eq!(
        actions.split_direction_for_key("ctrl+e"),
        Some(SplitDirection::Down)
    );
    assert_eq!(
        actions.split_direction_for_key("ctrl+f"),
        Some(SplitDirection::Down)
    );
    assert_eq!(actions.name_action("ctrl+c"), Some("splitPane"));
    assert_eq!(actions.name_action("ctrl+b"), None);
}

#[test]
fn microsoft_deserialization_nested_command_without_name_contract() {
    let mut actions = DeserializedActionMap::new();
    actions
        .layer_settings(
            r#"{
                "actions": [{
                    "commands": [
                        { "name":"child1", "command":{ "action":"newTab", "commandline":"ssh me@first.com" } },
                        { "name":"child2", "command":{ "action":"newTab", "commandline":"ssh me@second.com" } }
                    ]
                }]
            }"#,
        )
        .expect("unnamed nested command document parses");

    assert_eq!(actions.settings_warning_count(), 0);
    assert_eq!(actions.name_count(), 0);
}

#[test]
fn microsoft_deserialization_nested_bad_subcommands_contract() {
    let mut actions = DeserializedActionMap::new();
    actions
        .layer_settings(
            r#"{
                "actions": [{
                    "name":"nested command",
                    "commands":[ { "name":"child1" }, { "name":"child2" } ]
                }]
            }"#,
        )
        .expect("bad subcommands are recoverable warnings");

    assert_eq!(actions.settings_warning_count(), 2);
    assert_eq!(
        actions.warnings(),
        &[DeserializationActionWarning::FailedToParseSubCommands]
    );
    assert_eq!(actions.name_count(), 0);
}

#[test]
fn microsoft_deserialization_unbind_nested_command_contract() {
    let mut actions = DeserializedActionMap::new();
    actions
        .layer_settings(
            r#"{
                "actions": [{
                    "name":"parent",
                    "commands":[
                        { "name":"child1", "command":{ "action":"newTab", "commandline":"ssh me@first.com" } },
                        { "name":"child2", "command":{ "action":"newTab", "commandline":"ssh me@second.com" } }
                    ]
                }]
            }"#,
        )
        .expect("parent nested command parses");
    assert!(actions.name_has_nested_commands("parent"));
    assert_eq!(actions.nested_command_count("parent"), Some(2));

    actions
        .layer_settings(r#"{ "actions":[{ "name":"parent", "commands":null }] }"#)
        .expect("nested command null layer parses");
    assert_eq!(actions.name_count(), 0);
}

#[test]
fn microsoft_deserialization_rebind_nested_command_contract() {
    let mut actions = DeserializedActionMap::new();
    actions
        .layer_settings(
            r#"{
                "actions": [{
                    "name":"parent",
                    "commands":[
                        { "name":"child1", "command":{ "action":"newTab" } },
                        { "name":"child2", "command":{ "action":"newTab" } }
                    ]
                }]
            }"#,
        )
        .expect("parent nested command parses");
    actions
        .layer_settings(r#"{ "actions":[{ "name":"parent", "command":"newTab" }] }"#)
        .expect("action replaces nested command");

    assert_eq!(actions.name_count(), 1);
    assert_eq!(actions.name_action("parent"), Some("newTab"));
    assert!(!actions.name_has_nested_commands("parent"));
}

#[test]
fn microsoft_deserialization_inherited_command_contract() {
    let mut actions = DeserializedActionMap::new();
    actions
        .layer_settings(
            r#"{
                "actions":[{ "name":"foo", "command":"closePane", "keys":"ctrl+shift+w" }]
            }"#,
        )
        .expect("parent action layer parses");
    actions
        .layer_settings(
            r#"{
                "actions":[
                    { "command":null, "keys":"ctrl+shift+w" },
                    { "name":"bar", "command":"closePane", "id":"Test.ClosePane" }
                ]
            }"#,
        )
        .expect("child action layer parses");

    assert_eq!(actions.name_count(), 1);
    assert_eq!(actions.name_action("bar"), Some("closePane"));
    assert_eq!(actions.action_id_for_key("ctrl+shift+w"), None);
    assert_eq!(actions.key_binding_for_action("Test.ClosePane"), None);
}

#[test]
fn microsoft_deserialization_overwrite_parent_action_and_keybinding_contract() {
    let mut actions = DeserializedActionMap::new();
    actions
        .layer_settings(
            r#"{
                "actions":[
                    { "command":"closePane", "id":"Parent.ClosePane" },
                    { "command":"closePane", "id":"Parent.ClosePane2" }
                ],
                "keybindings":[
                    { "keys":"ctrl+shift+w", "id":"Parent.ClosePane" },
                    { "keys":"ctrl+shift+x", "id":"Parent.ClosePane2" }
                ]
            }"#,
        )
        .expect("parent action/keybinding layer parses");
    actions
        .layer_settings(
            r#"{
                "actions":[
                    { "command":"newTab", "id":"Parent.ClosePane" },
                    { "command":"closePane", "id":"Child.ClosePane" }
                ],
                "keybindings":[
                    { "id":"Child.ClosePane", "keys":"ctrl+shift+x" },
                    { "id":"Parent.ClosePane2", "keys":"ctrl+shift+y" }
                ]
            }"#,
        )
        .expect("child action/keybinding layer parses");

    assert_eq!(
        actions.action_id_for_key("ctrl+shift+w"),
        Some("Parent.ClosePane")
    );
    assert_eq!(actions.action_name_for_key("ctrl+shift+w"), Some("newTab"));
    assert_eq!(
        actions.action_id_for_key("ctrl+shift+x"),
        Some("Child.ClosePane")
    );
    assert_eq!(
        actions.action_name_for_key("ctrl+shift+x"),
        Some("closePane")
    );
    assert_eq!(
        actions.action_id_for_key("ctrl+shift+y"),
        Some("Parent.ClosePane2")
    );
    assert_eq!(
        actions.action_name_for_key("ctrl+shift+y"),
        Some("closePane")
    );
}
