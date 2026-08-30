use terminal_settings::keybindings::LayeredActionMap;

#[test]
fn microsoft_keybindings_many_keys_same_action_contract() {
    let mut map = LayeredActionMap::new();
    assert_eq!(map.keybinding_count(), 0);

    map.layer_json(r#"[ { "command": "copy", "keys": ["ctrl+c"] } ]"#)
        .expect("Microsoft copy binding layers");
    assert_eq!(map.keybinding_count(), 1);

    map.layer_json(r#"[ { "command": "copy", "keys": ["enter"] } ]"#)
        .expect("second chord for the same action layers independently");
    assert_eq!(map.keybinding_count(), 2);

    map.layer_json(
        r#"[
            { "command": "paste", "keys": ["ctrl+v"] },
            { "command": "paste", "keys": ["ctrl+shift+v"] }
        ]"#,
    )
    .expect("Microsoft paste bindings layer");
    assert_eq!(map.keybinding_count(), 4);
    assert_eq!(map.action_name_for_key("ctrl+c"), Some("copy"));
    assert_eq!(map.action_name_for_key("enter"), Some("copy"));
    assert_eq!(map.action_name_for_key("ctrl+v"), Some("paste"));
    assert_eq!(map.action_name_for_key("ctrl+shift+v"), Some("paste"));
}

#[test]
fn microsoft_keybindings_layer_keybindings_contract() {
    let mut map = LayeredActionMap::new();
    map.layer_json(r#"[ { "command": "copy", "keys": ["ctrl+c"] } ]"#)
        .expect("first Microsoft layer parses");
    assert_eq!(map.keybinding_count(), 1);
    assert_eq!(map.action_name_for_key("ctrl+c"), Some("copy"));

    map.layer_json(r#"[ { "command": "paste", "keys": ["ctrl+c"] } ]"#)
        .expect("same chord is overwritten by the later layer");
    assert_eq!(map.keybinding_count(), 1);
    assert_eq!(map.action_name_for_key("ctrl+c"), Some("paste"));

    map.layer_json(r#"[ { "command": "copy", "keys": ["enter"] } ]"#)
        .expect("different chord remains additive");
    assert_eq!(map.keybinding_count(), 2);
    assert_eq!(map.action_name_for_key("enter"), Some("copy"));
}

#[test]
fn microsoft_keybindings_hash_deduplication_contract() {
    let mut map = LayeredActionMap::new();
    let layer = r#"[ { "command": "splitPane", "keys": ["ctrl+c"] } ]"#;

    map.layer_json(layer)
        .expect("first Microsoft splitPane layer parses");
    map.layer_json(layer)
        .expect("identical splitPane layer parses again");

    assert_eq!(map.keybinding_count(), 1);
    assert_eq!(map.action_count(), 1);
    assert_eq!(map.action_name_for_key("ctrl+c"), Some("splitPane"));
}

#[test]
fn microsoft_keybindings_hash_content_args_contract() {
    let mut map = LayeredActionMap::new();
    map.layer_json(r#"[ { "command": { "action": "newTab" }, "keys": ["ctrl+c"] } ]"#)
        .expect("newTab default-args vector parses");
    map.layer_json(
        r#"[ { "command": { "action": "newTab", "index": 0 }, "keys": ["ctrl+shift+c"] } ]"#,
    )
    .expect("newTab index vector parses");

    assert_eq!(map.action_count(), 2);
    assert_eq!(map.new_tab_index_for_key("ctrl+c"), Some(None));
    assert_eq!(map.new_tab_index_for_key("ctrl+shift+c"), Some(Some(0)));

    let default_hash = map
        .semantic_hash_for_key("ctrl+c")
        .expect("default newTab has semantic identity");
    let indexed_hash = map
        .semantic_hash_for_key("ctrl+shift+c")
        .expect("indexed newTab has semantic identity");
    assert_ne!(default_hash, indexed_hash);
}

#[test]
fn microsoft_keybindings_unbind_keybindings_contract() {
    let mut map = LayeredActionMap::new();
    let good = r#"[ { "command": "copy", "keys": ["ctrl+c"] } ]"#;

    map.layer_json(good).expect("copy binding layers");
    assert_eq!(map.keybinding_count(), 1);

    map.layer_json(r#"[ { "command": "paste", "keys": ["ctrl+c"] } ]"#)
        .expect("paste replaces copy");
    assert_eq!(map.keybinding_count(), 1);
    assert_eq!(map.action_name_for_key("ctrl+c"), Some("paste"));

    for unbind in [
        r#"[ { "command": "unbound", "keys": ["ctrl+c"] } ]"#,
        r#"[ { "command": null, "keys": ["ctrl+c"] } ]"#,
        r#"[ { "command": "garbage", "keys": ["ctrl+c"] } ]"#,
        r#"[ { "command": 5, "keys": ["ctrl+c"] } ]"#,
    ] {
        map.layer_json(good).expect("good binding is restored");
        assert_eq!(map.keybinding_count(), 1);
        map.layer_json(unbind)
            .expect("Microsoft invalid/unbound command is a recoverable unbind");
        assert_eq!(map.keybinding_count(), 1);
        assert_eq!(map.action_id_for_key("ctrl+c"), None);
    }

    map.layer_json(r#"[ { "command": "unbound", "keys": ["ctrl+x"] } ]"#)
        .expect("unbinding an unused chord remains representable");
    assert_eq!(map.keybinding_count(), 2);
    assert_eq!(map.action_id_for_key("ctrl+x"), None);
}

#[test]
fn microsoft_keybindings_explicit_unbind_contract() {
    let mut map = LayeredActionMap::new();
    assert!(!map.is_key_explicitly_unbound("ctrl+c"));

    map.layer_json(r#"[ { "command": "copy", "keys": ["ctrl+c"] } ]"#)
        .expect("copy binding layers");
    assert!(!map.is_key_explicitly_unbound("ctrl+c"));

    map.layer_json(r#"[ { "command": "unbound", "keys": ["ctrl+c"] } ]"#)
        .expect("explicit unbind layers");
    assert!(map.is_key_explicitly_unbound("ctrl+c"));
    assert_eq!(map.action_id_for_key("ctrl+c"), None);

    map.layer_json(r#"[ { "command": "copy", "keys": ["ctrl+c"] } ]"#)
        .expect("rebinding clears explicit-unbound state");
    assert!(!map.is_key_explicitly_unbound("ctrl+c"));
    assert_eq!(map.action_name_for_key("ctrl+c"), Some("copy"));
}

#[test]
fn microsoft_keybindings_string_overload_contract() {
    let mut map = LayeredActionMap::new();
    map.layer_json(r#"[ { "command": "copy", "id": "Test.Copy", "keys": "ctrl+c" } ]"#)
        .expect("Microsoft string key overload parses");

    assert_eq!(map.keybinding_count(), 1);
    assert_eq!(map.action_id_for_key("ctrl+c"), Some("Test.Copy"));
    assert_eq!(map.action_name_for_key("ctrl+c"), Some("copy"));
    assert_eq!(map.copy_single_line_for_key("ctrl+c"), Some(false));
}

#[test]
fn microsoft_keybindings_get_key_binding_for_action_contract() {
    let mut map = LayeredActionMap::new();
    let cases = [
        (
            r#"[ { "command": "closeWindow", "id": "Test.CloseWindow", "keys": "ctrl+a" } ]"#,
            "Test.CloseWindow",
            "ctrl+a",
        ),
        (
            r#"[ { "command": { "action": "copy", "singleLine": true }, "id": "Test.Copy", "keys": "ctrl+b" } ]"#,
            "Test.Copy",
            "ctrl+b",
        ),
        (
            r#"[ { "command": { "action": "newTab", "index": 0 }, "id": "Test.NewTab", "keys": "ctrl+c" } ]"#,
            "Test.NewTab",
            "ctrl+c",
        ),
        (
            r#"[ { "command": "commandPalette", "id": "Test.CmdPal", "keys": "ctrl+shift+p" } ]"#,
            "Test.CmdPal",
            "ctrl+shift+p",
        ),
    ];

    for (index, (layer, id, expected_key)) in cases.into_iter().enumerate() {
        map.layer_json(layer)
            .expect("Microsoft explicit-ID keybinding layer parses");
        assert_eq!(map.keybinding_count(), index + 1);
        assert_eq!(map.key_binding_for_action(id), Some(expected_key));
        assert_eq!(map.action_id_for_key(expected_key), Some(id));
    }

    assert_eq!(map.copy_single_line_for_key("ctrl+b"), Some(true));
    assert_eq!(map.new_tab_index_for_key("ctrl+c"), Some(Some(0)));
}
