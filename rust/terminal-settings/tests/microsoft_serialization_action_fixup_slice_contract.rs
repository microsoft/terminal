use terminal_settings::{action_map::ActionMapDocument, settings_json};

fn assert_exact_fixup(old: &str, expected: &str, inbox: Option<&str>) {
    let inbox =
        inbox.map(|json| ActionMapDocument::from_json(json).expect("inbox vector is valid"));
    let mut document = ActionMapDocument::from_json(old).expect("Microsoft old vector is valid");
    assert!(
        document
            .fixup_user_actions(inbox.as_ref())
            .expect("Microsoft fixup is supported")
    );

    let expected = settings_json::parse(expected).expect("Microsoft new vector is valid");
    assert_eq!(document.to_json_value(), &expected);

    assert!(
        !document
            .fixup_user_actions(inbox.as_ref())
            .expect("modern vector is stable"),
        "a canonicalized ActionMap must not request another write-back"
    );
}

#[test]
fn microsoft_serialization_roundtrip_legacy_to_modern_actions_contract() {
    let old = r#"
    {
        "actions": [
            {
                "name": "foo",
                "id": "Test.SendInput",
                "command": { "action": "sendInput", "input": "just some input" },
                "keys": "ctrl+shift+w"
            },
            {
                "command": "unbound",
                "keys": "ctrl+shift+x"
            }
        ]
    }
    "#;

    let new = r#"
    {
        "actions": [
            {
                "name": "foo",
                "command": { "action": "sendInput", "input": "just some input" },
                "id": "Test.SendInput"
            }
        ],
        "keybindings": [
            {
                "id": "Test.SendInput",
                "keys": "ctrl+shift+w"
            },
            {
                "id": null,
                "keys": "ctrl+shift+x"
            }
        ]
    }
    "#;

    assert_exact_fixup(old, new, None);
}

#[test]
fn microsoft_serialization_user_actions_same_as_inbox_are_removed_contract() {
    let old = r#"
    {
        "actions": [
            {
                "command": "paste",
                "keys": "ctrl+shift+x"
            }
        ]
    }
    "#;

    let new = r#"
    {
        "actions": [],
        "keybindings": [
            {
                "id": "Terminal.PasteFromClipboard",
                "keys": "ctrl+shift+x"
            }
        ]
    }
    "#;

    // Minimal inbox projection for the exact Microsoft default action involved
    // by this source vector. The product fixup matches by command semantics and
    // adopts the inbox action ID rather than generating a user ID.
    let inbox = r#"
    {
        "actions": [
            {
                "command": "paste",
                "id": "Terminal.PasteFromClipboard"
            }
        ]
    }
    "#;

    assert_exact_fixup(old, new, Some(inbox));
}

#[test]
fn microsoft_serialization_same_name_different_commands_are_retained_contract() {
    let old = r#"
    {
        "actions": [
            {
                "command": { "action": "sendInput", "input": "just some input" },
                "name": "mySendInput"
            },
            {
                "command": { "action": "sendInput", "input": "just some input 2" },
                "name": "mySendInput"
            }
        ]
    }
    "#;

    let mut document = ActionMapDocument::from_json(old).expect("Microsoft vector is valid");
    assert!(document.fixup_user_actions(None).expect("fixup succeeds"));

    let root = document
        .to_json_value()
        .as_object()
        .expect("fixed action map remains an object");
    let actions = root
        .get("actions")
        .and_then(|value| value.as_array())
        .expect("fixed actions remain an array");
    assert_eq!(actions.len(), 2);
    assert!(root.get("keybindings").is_none());

    let expected_first = if usize::BITS == 32 {
        "User.sendInput.56911147"
    } else {
        "User.sendInput.A020D2"
    };
    let expected_second = if usize::BITS == 32 {
        "User.sendInput.35488AA6"
    } else {
        "User.sendInput.58D1971"
    };

    let first = actions[0].as_object().expect("first action is an object");
    let second = actions[1].as_object().expect("second action is an object");
    assert_eq!(
        first.get("name").and_then(|value| value.as_str()),
        Some("mySendInput")
    );
    assert_eq!(
        second.get("name").and_then(|value| value.as_str()),
        Some("mySendInput")
    );
    assert_eq!(
        first.get("id").and_then(|value| value.as_str()),
        Some(expected_first)
    );
    assert_eq!(
        second.get("id").and_then(|value| value.as_str()),
        Some(expected_second)
    );
    assert_ne!(expected_first, expected_second);

    assert!(
        !document
            .fixup_user_actions(None)
            .expect("modern generated-ID vector is stable")
    );
}

#[test]
fn microsoft_serialization_multiple_actions_are_collapsed_contract() {
    let old = r#"
    {
        "actions": [
            {
                "name": "foo",
                "icon": "myCoolIconPath.png",
                "command": { "action": "sendInput", "input": "just some input" },
                "keys": "ctrl+shift+w"
            },
            {
                "command": { "action": "sendInput", "input": "just some input" },
                "keys": "ctrl+shift+x"
            }
        ]
    }
    "#;

    let mut document = ActionMapDocument::from_json(old).expect("Microsoft vector is valid");
    assert!(document.fixup_user_actions(None).expect("fixup succeeds"));

    let expected_id = if usize::BITS == 32 {
        "User.sendInput.56911147"
    } else {
        "User.sendInput.A020D2"
    };

    let root = document
        .to_json_value()
        .as_object()
        .expect("fixed action map remains an object");
    let actions = root
        .get("actions")
        .and_then(|value| value.as_array())
        .expect("fixed actions remain an array");
    assert_eq!(actions.len(), 1);
    let action = actions[0]
        .as_object()
        .expect("collapsed action is an object");
    assert_eq!(
        action.get("name").and_then(|value| value.as_str()),
        Some("foo")
    );
    assert_eq!(
        action.get("icon").and_then(|value| value.as_str()),
        Some("myCoolIconPath.png")
    );
    assert_eq!(
        action.get("id").and_then(|value| value.as_str()),
        Some(expected_id)
    );

    let keybindings = root
        .get("keybindings")
        .and_then(|value| value.as_array())
        .expect("legacy keys are emitted as modern keybindings");
    assert_eq!(keybindings.len(), 2);
    for (binding, expected_keys) in keybindings.iter().zip(["ctrl+shift+w", "ctrl+shift+x"]) {
        let binding = binding.as_object().expect("keybinding is an object");
        assert_eq!(
            binding.get("id").and_then(|value| value.as_str()),
            Some(expected_id)
        );
        assert_eq!(
            binding.get("keys").and_then(|value| value.as_str()),
            Some(expected_keys)
        );
    }

    assert!(
        !document
            .fixup_user_actions(None)
            .expect("collapsed modern vector is stable")
    );
}
