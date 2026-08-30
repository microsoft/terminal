use terminal_settings::command_model::{LayeredCommands, SplitDirection};

#[test]
fn microsoft_commands_many_same_action_contract() {
    let mut commands = LayeredCommands::new();
    commands
        .layer_json(r#"[{ "name":"action0", "command":"copy" }]"#)
        .expect("first Microsoft command layer parses");
    commands
        .layer_json(r#"[{ "name":"action1", "command":{ "action":"copy", "singleLine":false } }]"#)
        .expect("second Microsoft command layer parses");
    commands
        .layer_json(
            r#"[
                { "name":"action2", "command":"paste" },
                { "name":"action3", "command":"paste" }
            ]"#,
        )
        .expect("third Microsoft command layer parses");

    assert_eq!(commands.command_count(), 4);
    assert_eq!(commands.action_name("action0"), Some("copy"));
    assert_eq!(commands.action_name("action1"), Some("copy"));
    assert_eq!(commands.action_name("action2"), Some("paste"));
    assert_eq!(commands.action_name("action3"), Some("paste"));
}

#[test]
fn microsoft_commands_layer_command_contract() {
    let mut commands = LayeredCommands::new();
    commands
        .layer_json(r#"[{ "name":"action0", "command":"copy" }]"#)
        .expect("copy layer parses");
    assert_eq!(commands.command_count(), 1);
    assert_eq!(commands.action_name("action0"), Some("copy"));

    commands
        .layer_json(r#"[{ "name":"action0", "command":"paste" }]"#)
        .expect("paste layer parses");
    assert_eq!(commands.command_count(), 1);
    assert_eq!(commands.action_name("action0"), Some("paste"));

    commands
        .layer_json(r#"[{ "name":"action0", "command":"newTab" }]"#)
        .expect("newTab layer parses");
    assert_eq!(commands.command_count(), 1);
    assert_eq!(commands.action_name("action0"), Some("newTab"));

    commands
        .layer_json(r#"[{ "name":"action0", "command":null }]"#)
        .expect("null unbind layer parses");
    assert_eq!(commands.command_count(), 0);
}

#[test]
fn microsoft_commands_split_pane_args_contract() {
    let mut commands = LayeredCommands::new();
    commands
        .layer_json(
            r#"[
                { "name":"command1", "command":{ "action":"splitPane", "split":"vertical" } },
                { "name":"command2", "command":{ "action":"splitPane", "split":"horizontal" } },
                { "name":"command4", "command":{ "action":"splitPane" } },
                { "name":"command5", "command":{ "action":"splitPane", "split":"auto" } },
                { "name":"command6", "command":{ "action":"splitPane", "size":0.25 } },
                { "name":"command7", "command":{ "action":"splitPane", "split":"right" } },
                { "name":"command8", "command":{ "action":"splitPane", "split":"left" } },
                { "name":"command9", "command":{ "action":"splitPane", "split":"up" } },
                { "name":"command10", "command":{ "action":"splitPane", "split":"down" } }
            ]"#,
        )
        .expect("Microsoft splitPane command vectors layer");

    assert_eq!(commands.command_count(), 9);
    assert_eq!(
        commands.split_direction("command1"),
        Some(SplitDirection::Right)
    );
    assert_eq!(
        commands.split_direction("command2"),
        Some(SplitDirection::Down)
    );
    assert_eq!(
        commands.split_direction("command4"),
        Some(SplitDirection::Automatic)
    );
    assert_eq!(
        commands.split_direction("command5"),
        Some(SplitDirection::Automatic)
    );
    assert_eq!(commands.split_size("command6"), Some(0.25));
    assert_eq!(
        commands.split_direction("command7"),
        Some(SplitDirection::Right)
    );
    assert_eq!(
        commands.split_direction("command8"),
        Some(SplitDirection::Left)
    );
    assert_eq!(
        commands.split_direction("command9"),
        Some(SplitDirection::Up)
    );
    assert_eq!(
        commands.split_direction("command10"),
        Some(SplitDirection::Down)
    );
}

#[test]
fn microsoft_commands_split_pane_bad_size_contract() {
    let mut commands = LayeredCommands::new();
    commands
        .layer_json(
            r#"[
                { "name":"command1", "command":{ "action":"splitPane", "size":0.25 } },
                { "name":"command2", "command":{ "action":"splitPane", "size":1.0 } },
                { "name":"command3", "command":{ "action":"splitPane", "size":0 } },
                { "name":"command4", "command":{ "action":"splitPane", "size":50 } }
            ]"#,
        )
        .expect("bad split sizes are recoverable warnings");

    assert_eq!(commands.warning_count(), 3);
    assert_eq!(commands.command_count(), 1);
    assert_eq!(commands.split_size("command1"), Some(0.25));
}

#[test]
fn microsoft_commands_resource_key_name_contract() {
    let mut commands = LayeredCommands::new();
    commands
        .layer_json(r#"[{ "name":{ "key":"DuplicateTabCommandKey" }, "command":"copy" }]"#)
        .expect("resource-key name vector layers");

    assert_eq!(commands.command_count(), 1);
    assert_eq!(commands.action_name("Duplicate tab"), Some("copy"));
}

#[test]
fn microsoft_commands_autogenerated_name_contract() {
    let mut commands = LayeredCommands::new();
    commands
        .layer_json(
            r#"[
                { "command":{ "action":"splitPane", "split":null } },
                { "command":{ "action":"splitPane", "split":"left" } },
                { "command":{ "action":"splitPane", "split":"right" } },
                { "command":{ "action":"splitPane", "split":"up" } },
                { "command":{ "action":"splitPane", "split":"down" } },
                { "command":{ "action":"splitPane", "split":"none" } },
                { "command":{ "action":"splitPane" } },
                { "command":{ "action":"splitPane", "split":"auto" } },
                { "command":{ "action":"splitPane", "split":"foo" } }
            ]"#,
        )
        .expect("Microsoft autogenerated-name vectors layer");

    assert_eq!(commands.command_count(), 5);
    assert_eq!(
        commands.split_direction("Split pane"),
        Some(SplitDirection::Automatic)
    );
    assert_eq!(
        commands.split_direction("Split pane, split: left"),
        Some(SplitDirection::Left)
    );
    assert_eq!(
        commands.split_direction("Split pane, split: right"),
        Some(SplitDirection::Right)
    );
    assert_eq!(
        commands.split_direction("Split pane, split: up"),
        Some(SplitDirection::Up)
    );
    assert_eq!(
        commands.split_direction("Split pane, split: down"),
        Some(SplitDirection::Down)
    );
}

#[test]
fn microsoft_commands_layer_on_autogenerated_name_contract() {
    let mut commands = LayeredCommands::new();
    commands
        .layer_json(
            r#"[
                { "command":{ "action":"splitPane" } },
                { "name":"Split pane", "command":{ "action":"splitPane", "split":"vertical" } }
            ]"#,
        )
        .expect("explicit command layers over autogenerated name");

    assert_eq!(commands.command_count(), 1);
    assert_eq!(
        commands.split_direction("Split pane"),
        Some(SplitDirection::Right)
    );
}

#[test]
fn microsoft_commands_generate_commandline_contract() {
    let mut commands = LayeredCommands::new();
    commands
        .layer_json(
            r#"[
                { "name":"action0", "command":{ "action":"newWindow" } },
                { "name":"action1", "command":{ "action":"newTab", "profile":"foo" } },
                { "name":"action2", "command":{ "action":"newWindow", "profile":"foo" } },
                { "name":"action3", "command":{ "action":"newWindow", "commandline":"bar.exe" } },
                { "name":"action4", "command":{ "action":"newWindow", "commandline":"pop.exe ya ha ha" } },
                { "name":"action5", "command":{ "action":"newWindow", "commandline":"pop.exe \"ya ha ha\"" } },
                { "name":"action6", "command":{ "action":"newWindow", "startingDirectory":"C:\\foo", "commandline":"bar.exe" } },
                { "name":"action7_startingDirectoryWithTrailingSlash", "command":{ "action":"newWindow", "startingDirectory":"C:\\", "commandline":"bar.exe" } },
                { "name":"action8_tabTitleEscaping", "command":{ "action":"newWindow", "tabTitle":"\\\";foo\\" } }
            ]"#,
        )
        .expect("Microsoft commandline vectors layer");

    assert_eq!(commands.command_count(), 9);
    assert_eq!(commands.commandline("action0").as_deref(), Some(""));
    assert_eq!(
        commands.commandline("action1").as_deref(),
        Some(r#"--profile "foo""#)
    );
    assert_eq!(
        commands.commandline("action2").as_deref(),
        Some(r#"--profile "foo""#)
    );
    assert_eq!(
        commands.commandline("action3").as_deref(),
        Some(r#"-- "bar.exe""#)
    );
    assert_eq!(
        commands.commandline("action4").as_deref(),
        Some(r#"-- "pop.exe ya ha ha""#)
    );
    assert_eq!(
        commands.commandline("action5").as_deref(),
        Some(r#"-- "pop.exe "ya ha ha"""#)
    );
    assert_eq!(
        commands.commandline("action6").as_deref(),
        Some(r#"--startingDirectory "C:\foo" -- "bar.exe""#)
    );
    assert_eq!(
        commands
            .commandline("action7_startingDirectoryWithTrailingSlash")
            .as_deref(),
        Some(r#"--startingDirectory "C:\\" -- "bar.exe""#)
    );
    assert_eq!(
        commands.commandline("action8_tabTitleEscaping").as_deref(),
        Some(r#"--title "\\\"\;foo\\""#)
    );
}
