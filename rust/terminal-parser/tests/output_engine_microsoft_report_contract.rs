use terminal_parser::output_engine::{
    DeviceAttributesKind, LineFeedType, OutputAction, OutputStateMachineEngine, TermDispatch,
};
use terminal_parser::state_machine::{ParserMode, StateMachine};

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

fn sgr_values(action: &OutputAction) -> Vec<i32> {
    let OutputAction::SetGraphicsRendition(parameters) = action else {
        panic!("expected SGR action, got {action:?}");
    };
    (0..parameters.size())
        .map(|index| parameters.at(index).unwrap_or(0))
        .collect()
}

#[test]
fn microsoft_output_device_attributes_cover_valid_defaults_and_reject_nonzero_parameters() {
    for (prefix, kind) in [
        ("", DeviceAttributesKind::Primary),
        (">", DeviceAttributesKind::Secondary),
        ("=", DeviceAttributesKind::Tertiary),
    ] {
        let mut machine = machine();
        machine.process_str(&format!(
            "\u{1b}[{prefix}c\u{1b}[{prefix}0c\u{1b}[{prefix}1c"
        ));
        assert_eq!(
            actions(&machine),
            [
                OutputAction::DeviceAttributes(kind),
                OutputAction::DeviceAttributes(kind),
            ],
            "prefix={prefix:?}"
        );
    }
}

#[test]
fn microsoft_output_device_status_report_covers_all_twelve_reference_statuses() {
    let cases = [
        (false, 5),
        (false, 6),
        (true, 6),
        (true, 15),
        (true, 25),
        (true, 26),
        (true, 55),
        (true, 56),
        (true, 62),
        (true, 63),
        (true, 75),
        (true, 85),
    ];
    let mut machine = machine();
    for (private, status) in cases {
        let marker = if private { "?" } else { "" };
        machine.process_str(&format!("\u{1b}[{marker}{status}n"));
    }

    let expected = cases.map(|(private, status)| OutputAction::DeviceStatusReport {
        private,
        status,
        id: None,
    });
    assert_eq!(actions(&machine), expected.as_slice());
}

#[test]
fn microsoft_output_request_terminal_parameters_matches_default_unsolicited_and_solicited() {
    let mut machine = machine();
    machine.process_str("\u{1b}[x\u{1b}[0x\u{1b}[1x");

    assert_eq!(
        actions(&machine),
        [
            OutputAction::RequestTerminalParameters(0),
            OutputAction::RequestTerminalParameters(0),
            OutputAction::RequestTerminalParameters(1),
        ]
    );
}

#[test]
fn microsoft_output_tab_clear_matches_default_all_and_multi_parameter_cases() {
    let mut machine = machine();
    machine.process_str("\u{1b}[g\u{1b}[3g\u{1b}[0;3g");

    assert_eq!(
        actions(&machine),
        [
            OutputAction::TabClear(0),
            OutputAction::TabClear(3),
            OutputAction::TabClear(0),
            OutputAction::TabClear(3),
        ]
    );
}

#[test]
fn microsoft_output_set_graphics_rendition_matches_all_reference_option_vectors() {
    let mut machine = machine();
    machine.process_str(
        "\u{1b}[m\u{1b}[0m\u{1b}[1;4;7;30;45;53m\
         \u{1b}[1;4;1;4;1;4;1;4;1;4;1;4;1;4;1;4;1m\
         \u{1b}[1;m\u{1b}[1;;1m\u{1b}[;31;1m",
    );

    let recorded = actions(&machine);
    assert_eq!(recorded.len(), 7);
    assert_eq!(sgr_values(&recorded[0]), [0]);
    assert_eq!(sgr_values(&recorded[1]), [0]);
    assert_eq!(sgr_values(&recorded[2]), [1, 4, 7, 30, 45, 53]);
    assert_eq!(
        sgr_values(&recorded[3]),
        [1, 4, 1, 4, 1, 4, 1, 4, 1, 4, 1, 4, 1, 4, 1, 4, 1]
    );
    assert_eq!(sgr_values(&recorded[4]), [1, 0]);
    assert_eq!(sgr_values(&recorded[5]), [1, 0, 1]);
    assert_eq!(sgr_values(&recorded[6]), [0, 31, 1]);
}

#[test]
fn microsoft_output_line_feed_matches_ind_nel_lf_ff_and_vt() {
    let mut machine = machine();
    machine.process_str("\u{1b}D\u{1b}E\n\u{000c}\u{000b}");

    assert_eq!(
        actions(&machine),
        [
            OutputAction::LineFeed(LineFeedType::WithoutReturn),
            OutputAction::LineFeed(LineFeedType::WithReturn),
            OutputAction::LineFeed(LineFeedType::DependsOnMode),
            OutputAction::LineFeed(LineFeedType::DependsOnMode),
            OutputAction::LineFeed(LineFeedType::DependsOnMode),
        ]
    );
}

#[test]
fn microsoft_output_control_characters_match_bel_bs_cr_and_ht() {
    let mut machine = machine();
    machine.process_str("\u{0007}\u{0008}\r\t");

    assert_eq!(
        actions(&machine),
        [
            OutputAction::WarningBell,
            OutputAction::CursorBackward(1),
            OutputAction::CarriageReturn,
            OutputAction::ForwardTab(1),
        ]
    );
}

#[test]
fn microsoft_output_vt52_sequences_match_all_reference_operations() {
    let mut machine = machine();
    machine.set_parser_mode(ParserMode::Ansi, false);
    machine.process_str("\u{1b}A\u{1b}B\u{1b}C\u{1b}D\u{1b}H\u{1b}I\u{1b}J\u{1b}K\u{1b}Y#%");

    assert_eq!(
        actions(&machine),
        [
            OutputAction::CursorUp(1),
            OutputAction::CursorDown(1),
            OutputAction::CursorForward(1),
            OutputAction::CursorBackward(1),
            OutputAction::CursorPosition { line: 1, column: 1 },
            OutputAction::ReverseLineFeed,
            OutputAction::EraseInDisplay(0),
            OutputAction::EraseInLine(0),
            OutputAction::CursorPosition { line: 4, column: 6 },
        ]
    );
}

#[test]
fn microsoft_output_identify_device_switches_between_vt52_and_ansi_reports() {
    let mut machine = machine();
    machine.set_parser_mode(ParserMode::Ansi, false);
    machine.process_str("\u{1b}Z");
    machine.set_parser_mode(ParserMode::Ansi, true);
    machine.process_str("\u{1b}Z");

    assert_eq!(
        actions(&machine),
        [
            OutputAction::DeviceAttributes(DeviceAttributesKind::Vt52),
            OutputAction::DeviceAttributes(DeviceAttributesKind::Primary),
        ]
    );
}
