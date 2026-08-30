use terminal_app::*;

fn parsed(raw: &[&str]) -> AppCommandlineArgs {
    parse_startup(raw).expect("Microsoft command-line vector parses")
}

fn new_tab(action: &StartupAction) -> &NewTerminalArgs {
    match action {
        StartupAction::NewTab(args) => args,
        other => panic!("expected new-tab, got {other:?}"),
    }
}

fn split(action: &StartupAction) -> (SplitType, SplitDirection, f32, &NewTerminalArgs) {
    match action {
        StartupAction::SplitPane {
            split_type,
            direction,
            size,
            terminal,
        } => (*split_type, *direction, *size, terminal),
        other => panic!("expected split-pane, got {other:?}"),
    }
}

#[test]
fn microsoft_local_terminal_app_parse_simple_commandline_contract() {
    let vectors: &[(&[&str], usize)] = &[
        (&["wt.exe"], 1),
        (&["wt.exe", "an arg with spaces"], 1),
        (&["wt.exe", "--parameter", "an arg with spaces"], 1),
        (&["wt.exe", "new-tab"], 1),
        (&["wt.exe", "new-tab", ";"], 2),
        (&["wt.exe", ";"], 2),
        (&["wt.exe", ";", ";"], 3),
    ];
    for (raw, expected) in vectors {
        assert_eq!(build_commands(raw).len(), *expected);
    }
    let commands = build_commands(&["wt.exe", "new-tab", ";"]);
    assert_eq!(
        commands[0]
            .args()
            .iter()
            .map(String::as_str)
            .collect::<Vec<_>>(),
        vec!["wt.exe", "new-tab"]
    );
    assert_eq!(
        commands[1]
            .args()
            .iter()
            .map(String::as_str)
            .collect::<Vec<_>>(),
        vec!["wt.exe"]
    );
}

#[test]
fn microsoft_local_terminal_app_parse_tricky_commandlines_contract() {
    for (raw, count) in [
        (vec!["wt.exe", "new-tab;"], 2),
        (vec!["wt.exe", ";new-tab;"], 3),
        (vec!["wt.exe;"], 2),
        (vec!["wt.exe;;"], 3),
        (vec!["wt.exe;foo;bar;baz"], 4),
    ] {
        assert_eq!(build_commands(&raw).len(), count);
    }
    let commands = build_commands(&["wt.exe", "-p", "u;", "nt", "-p", "u"]);
    assert_eq!(
        commands[0]
            .args()
            .iter()
            .map(String::as_str)
            .collect::<Vec<_>>(),
        vec!["wt.exe", "-p", "u"]
    );
    assert_eq!(
        commands[1]
            .args()
            .iter()
            .map(String::as_str)
            .collect::<Vec<_>>(),
        vec!["wt.exe", "nt", "-p", "u"]
    );
}

#[test]
fn microsoft_local_terminal_app_test_escape_delimiters_contract() {
    let p = parsed(&[
        "wt.exe",
        "new-tab",
        "powershell.exe",
        "This is an arg ; with spaces",
    ]);
    assert_eq!(p.startup_actions().len(), 2);
    assert_eq!(
        new_tab(&p.startup_actions()[0]).commandline,
        "powershell.exe \"This is an arg \""
    );
    assert_eq!(
        new_tab(&p.startup_actions()[1]).commandline,
        "\" with spaces\""
    );
    let p = parsed(&[
        "wt.exe",
        "new-tab",
        "powershell.exe",
        "This is an arg \\; with spaces",
    ]);
    assert_eq!(
        new_tab(&p.startup_actions()[0]).commandline,
        "powershell.exe \"This is an arg ; with spaces\""
    );
}

#[test]
fn microsoft_local_terminal_app_parse_simple_help_contract() {
    for flag in ["/?", "-?", "-h", "--help"] {
        let mut p = AppCommandlineArgs::new();
        assert!(
            p.parse_command(&build_commands(&["wt.exe", flag])[0])
                .is_ok()
        );
        assert!(!p.exit_message().is_empty());
        assert!(p.should_exit_early());
    }
}

#[test]
fn microsoft_local_terminal_app_parse_bad_options_contract() {
    for flag in ["/Z", "-q", "--bar"] {
        let mut p = AppCommandlineArgs::new();
        assert!(
            p.parse_command(&build_commands(&["wt.exe", flag])[0])
                .is_err()
        );
        assert!(!p.exit_message().is_empty());
    }
}

#[test]
fn microsoft_local_terminal_app_parse_subcommand_help_contract() {
    for raw in [
        ["wt.exe", "new-tab", "-h"],
        ["wt.exe", "new-tab", "--help"],
        ["wt.exe", "split-pane", "-h"],
        ["wt.exe", "split-pane", "--help"],
    ] {
        let mut p = AppCommandlineArgs::new();
        assert!(p.parse_command(&build_commands(&raw)[0]).is_ok());
        assert!(!p.exit_message().is_empty());
    }
}

#[test]
fn microsoft_local_terminal_app_parse_basic_commandline_into_args_contract() {
    let p = parsed(&["wt.exe", "new-tab"]);
    assert_eq!(p.startup_actions().len(), 1);
    assert_eq!(p.startup_actions()[0].kind(), "new-tab");
}

#[test]
fn microsoft_local_terminal_app_parse_new_tab_command_contract() {
    for sub in ["new-tab", "nt"] {
        assert_eq!(
            new_tab(&parsed(&["wt.exe", sub]).startup_actions()[0]),
            &NewTerminalArgs::default()
        );
        assert_eq!(
            new_tab(&parsed(&["wt.exe", sub, "--profile", "cmd"]).startup_actions()[0]).profile,
            "cmd"
        );
        assert_eq!(
            new_tab(
                &parsed(&["wt.exe", sub, "--startingDirectory", "c:\\Foo"]).startup_actions()[0]
            )
            .starting_directory,
            "c:\\Foo"
        );
        assert_eq!(
            new_tab(
                &parsed(&[
                    "wt.exe",
                    sub,
                    "powershell.exe",
                    "This is an arg with spaces"
                ])
                .startup_actions()[0]
            )
            .commandline,
            "powershell.exe \"This is an arg with spaces\""
        );
        let p = parsed(&["wt.exe", sub, "-p", "1", "wsl", "-d", "Alpine"]);
        let a = new_tab(&p.startup_actions()[0]);
        assert_eq!(a.profile, "1");
        assert_eq!(a.commandline, "wsl -d Alpine");
        assert_eq!(
            new_tab(&parsed(&["wt.exe", sub, "--tabColor", "#009999"]).startup_actions()[0])
                .tab_color,
            Some(0x009999)
        );
        assert_eq!(
            new_tab(&parsed(&["wt.exe", sub, "--colorScheme", "Vintage"]).startup_actions()[0])
                .color_scheme,
            "Vintage"
        );
    }
}

#[test]
fn microsoft_local_terminal_app_parse_split_pane_into_args_contract() {
    for sub in ["split-pane", "sp"] {
        assert_eq!(
            split(&parsed(&["wt.exe", sub]).startup_actions()[1]).1,
            SplitDirection::Automatic
        );
        assert_eq!(
            split(&parsed(&["wt.exe", sub, "-H"]).startup_actions()[1]).1,
            SplitDirection::Down
        );
        assert_eq!(
            split(&parsed(&["wt.exe", sub, "-V"]).startup_actions()[1]).1,
            SplitDirection::Right
        );
        assert_eq!(
            split(&parsed(&["wt.exe", sub, "-D"]).startup_actions()[1]).0,
            SplitType::Duplicate
        );
        let p = parsed(&["wt.exe", sub, "-p", "1", "-H", "wsl", "-d", "Alpine"]);
        let (_, d, _, t) = split(&p.startup_actions()[1]);
        assert_eq!(d, SplitDirection::Down);
        assert_eq!(t.commandline, "wsl -d Alpine");
        let p = parsed(&["wt.exe", sub, "-p", "1", "wsl", "-d", "Alpine", "-H"]);
        let (_, d, _, t) = split(&p.startup_actions()[1]);
        assert_eq!(d, SplitDirection::Automatic);
        assert_eq!(t.commandline, "wsl -d Alpine -H");
    }
}

#[test]
fn microsoft_local_terminal_app_parse_combo_commandline_into_args_contract() {
    for nt in ["new-tab", "nt"] {
        for sp in ["split-pane", "sp"] {
            let p = parsed(&["wt.exe", nt, ";", sp]);
            assert_eq!(
                p.startup_actions()
                    .iter()
                    .map(StartupAction::kind)
                    .collect::<Vec<_>>(),
                vec!["new-tab", "split-pane"]
            );
        }
    }
}

#[test]
fn microsoft_local_terminal_app_parse_focus_tab_args_contract() {
    for sub in ["focus-tab", "ft"] {
        assert_eq!(parsed(&["wt.exe", sub]).startup_actions().len(), 1);
        assert!(matches!(
            parsed(&["wt.exe", sub, "-n"]).startup_actions(),
            [StartupAction::NewTab(_), StartupAction::NextTab]
        ));
        assert!(matches!(
            parsed(&["wt.exe", sub, "-p"]).startup_actions(),
            [StartupAction::NewTab(_), StartupAction::PrevTab]
        ));
        assert!(matches!(
            parsed(&["wt.exe", sub, "-t", "2"]).startup_actions(),
            [StartupAction::NewTab(_), StartupAction::SwitchToTab(2)]
        ));
        assert!(parse_startup(&["wt.exe", sub, "-p", "-n"]).is_err());
    }
}

#[test]
fn microsoft_local_terminal_app_parse_move_focus_args_contract() {
    for sub in ["move-focus", "mf"] {
        assert!(parse_startup(&["wt.exe", sub]).is_err());
        for (word, dir) in [
            ("left", FocusDirection::Left),
            ("right", FocusDirection::Right),
            ("up", FocusDirection::Up),
            ("down", FocusDirection::Down),
        ] {
            assert!(
                matches!(parsed(&["wt.exe", sub, word]).startup_actions()[1], StartupAction::MoveFocus(value) if value == dir)
            );
        }
        assert!(parse_startup(&["wt.exe", sub, "badDirection"]).is_err());
    }
    assert!(matches!(
        parsed(&["wt.exe", "move-focus", "left", ";", "move-focus", "right"]).startup_actions(),
        [
            StartupAction::NewTab(_),
            StartupAction::MoveFocus(FocusDirection::Left),
            StartupAction::MoveFocus(FocusDirection::Right)
        ]
    ));
}

#[test]
fn microsoft_local_terminal_app_parse_swap_pane_args_contract() {
    assert!(parse_startup(&["wt.exe", "swap-pane"]).is_err());
    for (word, dir) in [
        ("left", FocusDirection::Left),
        ("right", FocusDirection::Right),
        ("up", FocusDirection::Up),
        ("down", FocusDirection::Down),
    ] {
        assert!(
            matches!(parsed(&["wt.exe", "swap-pane", word]).startup_actions()[1], StartupAction::SwapPane(value) if value == dir)
        );
    }
    assert!(parse_startup(&["wt.exe", "swap-pane", "badDirection"]).is_err());
    assert!(matches!(
        parsed(&["wt.exe", "swap-pane", "left", ";", "swap-pane", "right"]).startup_actions(),
        [
            StartupAction::NewTab(_),
            StartupAction::SwapPane(FocusDirection::Left),
            StartupAction::SwapPane(FocusDirection::Right)
        ]
    ));
}

#[test]
fn microsoft_local_terminal_app_parse_arguments_with_parsing_terminators_contract() {
    let p = parsed(&[
        "wt.exe", "new-tab", "-d", "C:\\", "--", "wsl", "-d", "Alpine",
    ]);
    let a = new_tab(&p.startup_actions()[0]);
    assert_eq!(a.commandline, "wsl -d Alpine");
    assert_eq!(a.starting_directory, "C:\\");
    assert_eq!(
        new_tab(
            &parsed(&[
                "wt.exe", "new-tab", "-d", "C:\\", "--", "wsl", "-d", "Alpine", "--", "sleep", "10"
            ])
            .startup_actions()[0]
        )
        .commandline,
        "wsl -d Alpine -- sleep 10"
    );
    assert_eq!(
        new_tab(
            &parsed(&[
                "wt.exe", "-d", "C:\\", "--", "wsl", "-d", "Alpine", "--", "sleep", "10"
            ])
            .startup_actions()[0]
        )
        .commandline,
        "wsl -d Alpine -- sleep 10"
    );
}

#[test]
fn microsoft_local_terminal_app_parse_focus_pane_args_contract() {
    for sub in ["focus-pane", "fp"] {
        assert!(parse_startup(&["wt.exe", sub]).is_err());
        assert!(parse_startup(&["wt.exe", sub, "left"]).is_err());
        assert!(parse_startup(&["wt.exe", sub, "1"]).is_err());
        assert!(matches!(
            parsed(&["wt.exe", sub, "--target", "0"]).startup_actions()[1],
            StartupAction::FocusPane(0)
        ));
        assert!(matches!(
            parsed(&["wt.exe", sub, "-t", "100"]).startup_actions()[1],
            StartupAction::FocusPane(100)
        ));
        assert!(parse_startup(&["wt.exe", sub, "--target", "-1"]).is_err());
    }
    assert!(matches!(
        parsed(&["wt.exe", "move-focus", "left", ";", "focus-pane", "-t", "1"]).startup_actions(),
        [
            StartupAction::NewTab(_),
            StartupAction::MoveFocus(FocusDirection::Left),
            StartupAction::FocusPane(1)
        ]
    ));
}

#[test]
fn microsoft_local_terminal_app_parse_no_command_is_new_tab_contract() {
    assert_eq!(parsed(&["wt.exe"]).startup_actions()[0].kind(), "new-tab");
    assert_eq!(
        new_tab(&parsed(&["wt.exe", "--profile", "cmd"]).startup_actions()[0]).profile,
        "cmd"
    );
    assert_eq!(
        new_tab(&parsed(&["wt.exe", "--startingDirectory", "c:\\Foo"]).startup_actions()[0])
            .starting_directory,
        "c:\\Foo"
    );
    assert_eq!(
        new_tab(&parsed(&["wt.exe", "powershell.exe"]).startup_actions()[0]).commandline,
        "powershell.exe"
    );
    assert_eq!(
        new_tab(
            &parsed(&["wt.exe", "powershell.exe", "This is an arg with spaces"]).startup_actions()
                [0]
        )
        .commandline,
        "powershell.exe \"This is an arg with spaces\""
    );
}

#[test]
fn microsoft_local_terminal_app_validate_first_command_is_new_tab_contract() {
    assert_eq!(
        parsed(&["wt.exe", "split-pane", ";", "split-pane"])
            .startup_actions()
            .iter()
            .map(StartupAction::kind)
            .collect::<Vec<_>>(),
        vec!["new-tab", "split-pane", "split-pane"]
    );
}

#[test]
fn microsoft_local_terminal_app_check_typos_contract() {
    let p = parsed(&["wt.exe", "new-tab", ";", "slpit-pane"]);
    assert_eq!(new_tab(&p.startup_actions()[1]).commandline, "slpit-pane");
    assert_eq!(
        new_tab(&parsed(&["wt.exe", "slpit-pane", "-H"]).startup_actions()[0]).commandline,
        "slpit-pane -H"
    );
}

#[test]
fn microsoft_local_terminal_app_test_simple_execute_commandline_action_contract() {
    let a = convert_execute_commandline_to_actions("new-tab");
    assert_eq!(a.len(), 1);
    assert_eq!(new_tab(&a[0]), &NewTerminalArgs::default());
}

#[test]
fn microsoft_local_terminal_app_test_multiple_command_execute_commandline_action_contract() {
    assert_eq!(
        convert_execute_commandline_to_actions("new-tab ; split-pane")
            .iter()
            .map(StartupAction::kind)
            .collect::<Vec<_>>(),
        vec!["new-tab", "split-pane"]
    );
}

#[test]
fn microsoft_local_terminal_app_test_invalid_execute_commandline_action_contract() {
    assert!(convert_execute_commandline_to_actions("split-pane -H -V").is_empty());
}

#[test]
fn microsoft_local_terminal_app_test_launch_mode_contract() {
    for (raw, expected) in [
        (vec!["wt.exe", "-F"], Some(LaunchMode::Fullscreen)),
        (vec!["wt.exe", "--fullscreen"], Some(LaunchMode::Fullscreen)),
        (vec!["wt.exe", "-M"], Some(LaunchMode::Maximized)),
        (vec!["wt.exe", "--maximized"], Some(LaunchMode::Maximized)),
        (vec!["wt.exe", "-f"], Some(LaunchMode::Focus)),
        (vec!["wt.exe", "--focus"], Some(LaunchMode::Focus)),
        (vec!["wt.exe", "-fM"], Some(LaunchMode::MaximizedFocus)),
        (
            vec!["wt.exe", "--maximized", "--focus"],
            Some(LaunchMode::MaximizedFocus),
        ),
        (
            vec!["wt.exe", "--maximized", "--focus", "--focus"],
            Some(LaunchMode::MaximizedFocus),
        ),
        (
            vec!["wt.exe", "--maximized", "--focus", "--maximized"],
            Some(LaunchMode::MaximizedFocus),
        ),
    ] {
        assert_eq!(parsed(&raw).launch_mode(), expected);
    }
    assert_eq!(parsed(&["wt.exe"]).launch_mode(), None);
}

#[test]
fn microsoft_local_terminal_app_test_launch_mode_with_no_command_contract() {
    let p = parsed(&["wt.exe", "-M", "--profile", "cmd"]);
    assert_eq!(p.launch_mode(), Some(LaunchMode::Maximized));
    assert_eq!(new_tab(&p.startup_actions()[0]).profile, "cmd");
    let p = parsed(&["wt.exe", "-M", "powershell.exe"]);
    assert_eq!(p.launch_mode(), Some(LaunchMode::Maximized));
    assert_eq!(
        new_tab(&p.startup_actions()[0]).commandline,
        "powershell.exe"
    );
}

#[test]
fn microsoft_local_terminal_app_test_multiple_split_pane_sizes_contract() {
    for sub in ["split-pane", "sp"] {
        assert_eq!(split(&parsed(&["wt.exe", sub]).startup_actions()[1]).2, 0.5);
        assert_eq!(
            split(&parsed(&["wt.exe", sub, "-s", ".3"]).startup_actions()[1]).2,
            0.3
        );
        let p = parsed(&["wt.exe", sub, "-s", ".3", ";", sub]);
        assert_eq!(split(&p.startup_actions()[1]).2, 0.3);
        assert_eq!(split(&p.startup_actions()[2]).2, 0.5);
        let p = parsed(&["wt.exe", sub, "-s", ".3", ";", sub, "-s", ".7"]);
        assert_eq!(split(&p.startup_actions()[1]).2, 0.3);
        assert_eq!(split(&p.startup_actions()[2]).2, 0.7);
    }
}
