use terminal_settings::keybindings_model::{
    KeyBindingsModel, KeyChord, MOD_ALT, MOD_CONTROL, MOD_SHIFT, MOD_WIN,
};

#[test]
fn microsoft_keybindings_key_chords_contract() {
    let all_modifiers = MOD_CONTROL | MOD_ALT | MOD_SHIFT | MOD_WIN;
    let portable_cases = [
        (0, i32::from(b'A'), 0, "a", i32::from(b'A')),
        (MOD_CONTROL, i32::from(b'A'), 0, "ctrl+a", i32::from(b'A')),
        (MOD_CONTROL | MOD_SHIFT, 0xBB, 0, "ctrl+shift+plus", 0xBB),
        (all_modifiers, 255, 0, "win+ctrl+alt+shift+vk(255)", 255),
    ];

    for (modifiers, vkey, scan_code, expected_text, expected_vkey) in portable_cases {
        let chord = KeyChord::new(modifiers, vkey, scan_code);
        assert_eq!(chord.to_binding_string(), expected_text);

        let parsed = KeyChord::from_string(expected_text)
            .expect("portable Microsoft KeyChordSerialization vector parses");
        assert_eq!(parsed.modifiers(), modifiers);
        assert_eq!(parsed.vkey(), expected_vkey);
        assert_eq!(parsed.scan_code(), scan_code);
    }

    // Microsoft serializes this raw scancode exactly. Its effective VKey is
    // derived by active-layout MapVirtualKeyW, which remains platform-owned.
    let scan_only = KeyChord::new(all_modifiers, 0, 123);
    assert_eq!(scan_only.to_binding_string(), "win+ctrl+alt+shift+sc(123)");
    let parsed = KeyChord::from_string("win+ctrl+alt+shift+sc(123)")
        .expect("raw Microsoft scancode vector remains representable");
    assert_eq!(parsed.modifiers(), all_modifiers);
    assert_eq!(parsed.scan_code(), 123);
}

#[test]
fn microsoft_keybindings_layer_scancode_keybindings_contract() {
    let mut map = KeyBindingsModel::new();

    map.layer_json(r#"[ { "command": "quakeMode", "keys":"win+sc(41)" } ]"#)
        .expect("Microsoft scancode binding layers");
    assert_eq!(map.keybinding_count(), 1);
    assert_eq!(map.action_name_for_key("win+sc(41)"), Some("quakeMode"));

    map.layer_json(
        r#"[ { "keys": "win+`", "command": { "action": "globalSummon", "monitor": "any" } } ]"#,
    )
    .expect("literal grave binding replaces the equivalent US-layout scancode");
    assert_eq!(map.keybinding_count(), 1);
    assert_eq!(map.action_name_for_key("win+sc(41)"), Some("globalSummon"));
    assert_eq!(map.action_name_for_key("win+`"), Some("globalSummon"));

    map.layer_json(r#"[ { "keys": "ctrl+shift+`", "command": { "action": "quakeMode" } } ]"#)
        .expect("different modifiers produce a distinct effective chord");
    assert_eq!(map.keybinding_count(), 2);
    assert_eq!(map.action_name_for_key("ctrl+shift+`"), Some("quakeMode"));
}

#[test]
fn microsoft_keybindings_without_vkey_contract() {
    let mut map = KeyBindingsModel::new();
    map.layer_json(r#"[{"command": "quakeMode", "id": "Test.NoVKey", "keys":"shift+sc(255)"}]"#)
        .expect("Microsoft no-VKey scancode vector layers");

    let chord = KeyChord::new(MOD_SHIFT, 0, 255);
    assert_eq!(chord.vkey(), 0);
    assert_eq!(chord.scan_code(), 255);
    assert_eq!(map.action_id_for_chord(chord), Some("Test.NoVKey"));
}
