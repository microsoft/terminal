use terminal_settings::keybindings::{
    CommandPaletteLaunchMode, KeyBindingError, LayeredActionMap, MoveTabDirection, SplitDirection,
};

#[test]
fn microsoft_keybindings_arbitrary_args_contract() {
    let mut map = LayeredActionMap::new();
    map.layer_json(
        r#"[
            { "command": "copy", "id": "Test.CopyNoArgs", "keys": ["ctrl+c"] },
            { "command": { "action": "copy", "singleLine": false }, "id": "Test.CopyMultiline", "keys": ["ctrl+shift+c"] },
            { "command": { "action": "copy", "singleLine": true }, "id": "Test.CopySingleline", "keys": ["alt+shift+c"] },
            { "command": "newTab", "id": "Test.NewTabNoArgs", "keys": ["ctrl+t"] },
            { "command": { "action": "newTab", "index": 0 }, "id": "Test.NewTab0", "keys": ["ctrl+shift+t"] },
            { "command": { "action": "newTab", "index": 11 }, "id": "Test.NewTab11", "keys": ["ctrl+shift+y"] },
            { "command": { "action": "copy", "madeUpBool": true }, "id": "Test.CopyFakeArgs", "keys": ["ctrl+b"] },
            { "command": { "action": "copy" }, "id": "Test.CopyNullArgs", "keys": ["ctrl+shift+b"] },
            { "command": { "action": "adjustFontSize", "delta": 1 }, "id": "Test.EnlargeFont", "keys": ["ctrl+f"] },
            { "command": { "action": "adjustFontSize", "delta": -1 }, "id": "Test.ReduceFont", "keys": ["ctrl+g"] }
        ]"#,
    )
    .expect("Microsoft arbitrary action-argument vector layers");

    assert_eq!(map.keybinding_count(), 10);
    assert_eq!(map.copy_single_line_for_key("ctrl+c"), Some(false));
    assert_eq!(map.copy_single_line_for_key("ctrl+shift+c"), Some(false));
    assert_eq!(map.copy_single_line_for_key("alt+shift+c"), Some(true));
    assert_eq!(map.new_tab_index_for_key("ctrl+t"), Some(None));
    assert_eq!(map.new_tab_index_for_key("ctrl+shift+t"), Some(Some(0)));
    assert_eq!(map.new_tab_index_for_key("ctrl+shift+y"), Some(Some(11)));
    assert_eq!(map.copy_single_line_for_key("ctrl+b"), Some(false));
    assert_eq!(map.copy_single_line_for_key("ctrl+shift+b"), Some(false));
    assert_eq!(map.adjust_font_delta_for_key("ctrl+f"), Some(1));
    assert_eq!(map.adjust_font_delta_for_key("ctrl+g"), Some(-1));
}

#[test]
fn microsoft_keybindings_split_pane_args_contract() {
    let mut map = LayeredActionMap::new();
    map.layer_json(
        r#"[
            { "keys": ["ctrl+d"], "id": "Test.SplitPaneVertical", "command": { "action": "splitPane", "split": "vertical" } },
            { "keys": ["ctrl+e"], "id": "Test.SplitPaneHorizontal", "command": { "action": "splitPane", "split": "horizontal" } },
            { "keys": ["ctrl+g"], "id": "Test.SplitPane", "command": { "action": "splitPane" } },
            { "keys": ["ctrl+h"], "id": "Test.SplitPaneAuto", "command": { "action": "splitPane", "split": "auto" } }
        ]"#,
    )
    .expect("Microsoft split-pane vectors layer");

    assert_eq!(map.keybinding_count(), 4);
    assert_eq!(
        map.split_direction_for_key("ctrl+d"),
        Some(SplitDirection::Right)
    );
    assert_eq!(
        map.split_direction_for_key("ctrl+e"),
        Some(SplitDirection::Down)
    );
    assert_eq!(
        map.split_direction_for_key("ctrl+g"),
        Some(SplitDirection::Automatic)
    );
    assert_eq!(
        map.split_direction_for_key("ctrl+h"),
        Some(SplitDirection::Automatic)
    );
}

#[test]
fn microsoft_keybindings_set_tab_color_args_contract() {
    let mut map = LayeredActionMap::new();
    map.layer_json(
        r##"[
            { "keys": ["ctrl+c"], "id": "Test.SetTabColorNull", "command": { "action": "setTabColor", "color": null } },
            { "keys": ["ctrl+d"], "id": "Test.SetTabColor", "command": { "action": "setTabColor", "color": "#123456" } },
            { "keys": ["ctrl+f"], "id": "Test.SetTabColorNoArgs", "command": "setTabColor" }
        ]"##,
    )
    .expect("Microsoft SetTabColor vectors layer");

    assert_eq!(map.keybinding_count(), 3);
    assert_eq!(map.tab_color_for_key("ctrl+c"), Some(None));
    assert_eq!(map.tab_color_for_key("ctrl+d"), Some(Some(0x0056_3412)));
    assert_eq!(map.tab_color_for_key("ctrl+f"), Some(None));
}

#[test]
fn microsoft_keybindings_scroll_args_contract() {
    let mut map = LayeredActionMap::new();
    map.layer_json(
        r#"[
            { "keys": ["up"], "id": "Test.ScrollUp0", "command": "scrollUp" },
            { "keys": ["down"], "id": "Test.ScrollDown0", "command": "scrollDown" },
            { "keys": ["ctrl+up"], "id": "Test.ScrollUp1", "command": { "action": "scrollUp" } },
            { "keys": ["ctrl+down"], "id": "Test.ScrollDown1", "command": { "action": "scrollDown" } },
            { "keys": ["ctrl+shift+up"], "id": "Test.ScrollUp2", "command": { "action": "scrollUp", "rowsToScroll": 10 } },
            { "keys": ["ctrl+shift+down"], "id": "Test.ScrollDown2", "command": { "action": "scrollDown", "rowsToScroll": 10 } }
        ]"#,
    )
    .expect("Microsoft scroll vectors layer");

    assert_eq!(map.keybinding_count(), 6);
    assert_eq!(map.rows_to_scroll_for_key("up"), Some(None));
    assert_eq!(map.rows_to_scroll_for_key("down"), Some(None));
    assert_eq!(map.rows_to_scroll_for_key("ctrl+up"), Some(None));
    assert_eq!(map.rows_to_scroll_for_key("ctrl+down"), Some(None));
    assert_eq!(map.rows_to_scroll_for_key("ctrl+shift+up"), Some(Some(10)));
    assert_eq!(
        map.rows_to_scroll_for_key("ctrl+shift+down"),
        Some(Some(10))
    );

    let err = map
        .layer_json(
            r#"[{ "keys": ["up"], "command": { "action": "scrollDown", "rowsToScroll": -1 } }]"#,
        )
        .expect_err("negative rowsToScroll must fail like Microsoft");
    assert_eq!(err, KeyBindingError::InvalidActionArguments);
}

#[test]
fn microsoft_keybindings_toggle_command_palette_args_contract() {
    let mut map = LayeredActionMap::new();
    map.layer_json(
        r#"[
            { "keys": ["up"], "id": "Test.CmdPal", "command": "commandPalette" },
            { "keys": ["ctrl+up"], "id": "Test.CmdPalActionMode", "command": { "action": "commandPalette", "launchMode": "action" } },
            { "keys": ["ctrl+shift+up"], "id": "Test.CmdPalLineMode", "command": { "action": "commandPalette", "launchMode": "commandLine" } }
        ]"#,
    )
    .expect("Microsoft command-palette vectors layer");

    assert_eq!(
        map.command_palette_launch_mode_for_key("up"),
        Some(CommandPaletteLaunchMode::Action)
    );
    assert_eq!(
        map.command_palette_launch_mode_for_key("ctrl+up"),
        Some(CommandPaletteLaunchMode::Action)
    );
    assert_eq!(
        map.command_palette_launch_mode_for_key("ctrl+shift+up"),
        Some(CommandPaletteLaunchMode::CommandLine)
    );

    let err = map
        .layer_json(r#"[{ "keys": ["up"], "command": { "action": "commandPalette", "launchMode": "bad" } }]"#)
        .expect_err("unknown commandPalette launchMode must fail like Microsoft");
    assert_eq!(err, KeyBindingError::InvalidActionArguments);
}

#[test]
fn microsoft_keybindings_move_tab_args_contract() {
    let mut map = LayeredActionMap::new();
    map.layer_json(
        r#"[
            { "keys": ["up"], "id": "Test.MoveTabUp", "command": { "action": "moveTab", "direction": "forward" } },
            { "keys": ["down"], "id": "Test.MoveTabDown", "command": { "action": "moveTab", "direction": "backward" } }
        ]"#,
    )
    .expect("Microsoft move-tab vectors layer");

    assert_eq!(map.keybinding_count(), 2);
    assert_eq!(
        map.move_tab_direction_for_key("up"),
        Some(MoveTabDirection::Forward)
    );
    assert_eq!(
        map.move_tab_direction_for_key("down"),
        Some(MoveTabDirection::Backward)
    );

    let mut no_args = LayeredActionMap::new();
    no_args
        .layer_json(r#"[{ "keys": ["up"], "command": "moveTab" }]"#)
        .expect("moveTab without required args is a recoverable unbind");
    assert_eq!(no_args.action_id_for_key("up"), None);

    let err = map
        .layer_json(
            r#"[{ "keys": ["up"], "command": { "action": "moveTab", "direction": "bad" } }]"#,
        )
        .expect_err("invalid moveTab direction must fail like Microsoft");
    assert_eq!(err, KeyBindingError::InvalidActionArguments);
}
