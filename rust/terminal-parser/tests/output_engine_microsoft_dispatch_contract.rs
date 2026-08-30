use terminal_parser::output_engine::{OutputAction, OutputStateMachineEngine, TermDispatch};
use terminal_parser::state_machine::{MAX_PARAMETER_VALUE, ParserMode, StateMachine};

const PARAM_VALUES: [i32; 12] = [
    0,
    1,
    2,
    1_000,
    9_999,
    10_000,
    16_383,
    16_384,
    32_767,
    32_768,
    50_000,
    999_999_999,
];

#[derive(Debug, Default)]
struct RecordingDispatch {
    actions: Vec<OutputAction>,
}

impl TermDispatch for RecordingDispatch {
    fn dispatch(&mut self, action: OutputAction) {
        self.actions.push(action);
    }
}

fn machine() -> StateMachine<OutputStateMachineEngine<RecordingDispatch>> {
    StateMachine::new(OutputStateMachineEngine::new(RecordingDispatch::default()))
}

fn actions(machine: &StateMachine<OutputStateMachineEngine<RecordingDispatch>>) -> &[OutputAction] {
    &machine.engine().dispatch().actions
}

fn expected_numeric(value: i32) -> i32 {
    if value <= 1 {
        1
    } else {
        value.min(MAX_PARAMETER_VALUE)
    }
}

fn movement_action(command: char, distance: i32) -> OutputAction {
    match command {
        'A' => OutputAction::CursorUp(distance),
        'B' => OutputAction::CursorDown(distance),
        'C' => OutputAction::CursorForward(distance),
        'D' => OutputAction::CursorBackward(distance),
        'E' => OutputAction::CursorNextLine(distance),
        'F' => OutputAction::CursorPreviousLine(distance),
        'G' | '`' => OutputAction::CursorHorizontalPositionAbsolute(distance),
        'd' => OutputAction::VerticalLinePositionAbsolute(distance),
        'a' => OutputAction::HorizontalPositionRelative(distance),
        'e' => OutputAction::VerticalPositionRelative(distance),
        '@' => OutputAction::InsertCharacter(distance),
        'P' => OutputAction::DeleteCharacter(distance),
        _ => panic!("unsupported Microsoft movement command {command:?}"),
    }
}

#[test]
fn microsoft_output_cursor_movement_with_values_exhausts_data_matrix() {
    let commands = [
        'A', 'B', 'C', 'D', 'E', 'F', 'G', '`', 'd', 'a', 'e', '@', 'P',
    ];

    for distance in PARAM_VALUES {
        for extra_parameter in [false, true] {
            for command in commands {
                let suffix = if extra_parameter { ";9" } else { "" };
                let sequence = format!("\u{1b}[{distance}{suffix}{command}");
                let mut machine = machine();
                machine.process_str(&sequence);
                assert_eq!(
                    actions(&machine),
                    [movement_action(command, expected_numeric(distance))],
                    "distance={distance}, extra_parameter={extra_parameter}, command={command}"
                );
            }
        }
    }
}

#[test]
fn microsoft_output_cursor_movement_without_values_defaults_to_one() {
    for command in [
        'A', 'B', 'C', 'D', 'E', 'F', 'G', '`', 'd', 'a', 'e', '@', 'P',
    ] {
        let mut machine = machine();
        machine.process_str(&format!("\u{1b}[{command}"));
        assert_eq!(
            actions(&machine),
            [movement_action(command, 1)],
            "command={command}"
        );
    }
}

#[test]
fn microsoft_output_cursor_position_exhausts_row_column_cartesian_product() {
    for row in PARAM_VALUES {
        for column in PARAM_VALUES {
            let mut machine = machine();
            machine.process_str(&format!("\u{1b}[{row};{column}H"));
            assert_eq!(
                actions(&machine),
                [OutputAction::CursorPosition {
                    line: expected_numeric(row),
                    column: expected_numeric(column),
                }],
                "row={row}, column={column}"
            );
        }
    }
}

#[test]
fn microsoft_output_cursor_position_with_only_row_defaults_column_to_one() {
    for row in PARAM_VALUES {
        let mut machine = machine();
        machine.process_str(&format!("\u{1b}[{row}H"));
        assert_eq!(
            actions(&machine),
            [OutputAction::CursorPosition {
                line: expected_numeric(row),
                column: 1,
            }],
            "row={row}"
        );
    }
}

#[test]
fn microsoft_output_cursor_save_load_matches_three_reference_sequences() {
    let mut machine = machine();
    machine.process_str("\u{1b}7\u{1b}8\u{1b}[u");
    assert_eq!(
        actions(&machine),
        [
            OutputAction::CursorSaveState,
            OutputAction::CursorRestoreState,
            OutputAction::CursorRestoreState,
        ]
    );
}

#[test]
fn microsoft_output_ansi_mode_round_trips_between_ansi_and_vt52() {
    let mut machine = machine();
    machine.process_str("\u{1b}[?2l");
    machine.set_parser_mode(ParserMode::Ansi, false);
    machine.process_str("\u{1b}<");
    assert_eq!(
        actions(&machine),
        [
            OutputAction::SetMode {
                private: true,
                enabled: false,
                mode: 2,
            },
            OutputAction::SetMode {
                private: true,
                enabled: true,
                mode: 2,
            },
        ]
    );
}

#[test]
fn microsoft_output_private_modes_cover_all_nine_data_values() {
    for mode in [1, 3, 5, 6, 7, 12, 25, 40, 1049] {
        let mut machine = machine();
        machine.process_str(&format!("\u{1b}[?{mode}h\u{1b}[?{mode}l"));
        assert_eq!(
            actions(&machine),
            [
                OutputAction::SetMode {
                    private: true,
                    enabled: true,
                    mode,
                },
                OutputAction::SetMode {
                    private: true,
                    enabled: false,
                    mode,
                },
            ],
            "mode={mode}"
        );
    }
}

#[test]
fn microsoft_output_multiple_modes_preserve_source_order() {
    let mut machine = machine();
    machine.process_str("\u{1b}[?5;1;6h\u{1b}[?5;1;6l");
    assert_eq!(
        actions(&machine),
        [
            OutputAction::SetMode {
                private: true,
                enabled: true,
                mode: 5,
            },
            OutputAction::SetMode {
                private: true,
                enabled: true,
                mode: 1,
            },
            OutputAction::SetMode {
                private: true,
                enabled: true,
                mode: 6,
            },
            OutputAction::SetMode {
                private: true,
                enabled: false,
                mode: 5,
            },
            OutputAction::SetMode {
                private: true,
                enabled: false,
                mode: 1,
            },
            OutputAction::SetMode {
                private: true,
                enabled: false,
                mode: 6,
            },
        ]
    );
}

#[test]
fn microsoft_output_erase_matrix_covers_display_line_and_default() {
    for (command, make_action) in [
        ('J', OutputAction::EraseInDisplay as fn(i32) -> OutputAction),
        ('K', OutputAction::EraseInLine as fn(i32) -> OutputAction),
    ] {
        for (source_value, encoded, expected) in [
            (0, Some(0), 0),
            (1, Some(1), 1),
            (2, Some(2), 2),
            (10, None, 0),
        ] {
            let parameter = encoded.map_or_else(String::new, |value| value.to_string());
            let mut machine = machine();
            machine.process_str(&format!("\u{1b}[{parameter}{command}"));
            assert_eq!(
                actions(&machine),
                [make_action(expected)],
                "command={command}, Microsoft source value={source_value}"
            );
        }
    }
}

#[test]
fn microsoft_output_multiple_erase_dispatches_each_parameter_in_order() {
    let mut machine = machine();
    machine.process_str("\u{1b}[3;2J\u{1b}[0;1K");
    assert_eq!(
        actions(&machine),
        [
            OutputAction::EraseInDisplay(3),
            OutputAction::EraseInDisplay(2),
            OutputAction::EraseInLine(0),
            OutputAction::EraseInLine(1),
        ]
    );
}
