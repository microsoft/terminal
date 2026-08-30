use terminal_parser::output_engine::{
    LineFeedType, OutputAction, OutputStateMachineEngine, TermDispatch,
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

fn sgr_values(action: &OutputAction) -> Option<Vec<i32>> {
    let OutputAction::SetGraphicsRendition(parameters) = action else {
        return None;
    };
    Some(
        (0..parameters.size())
            .map(|index| parameters.at(index).unwrap_or(0))
            .collect(),
    )
}

#[test]
fn microsoft_output_strings_preserve_complete_split_and_mixed_processing() {
    let mut one = machine();
    one.process_str("\u{1b}[0m");
    assert_eq!(sgr_values(&actions(&one)[0]), Some(vec![0]));

    let mut combined = machine();
    combined.process_str("\u{1b}[1;4;7;30;45;53m\u{1b}[2J");
    assert_eq!(
        sgr_values(&actions(&combined)[0]),
        Some(vec![1, 4, 7, 30, 45, 53])
    );
    assert_eq!(actions(&combined)[1], OutputAction::EraseInDisplay(2));

    let mut text_between = machine();
    text_between.process_str("\u{1b}[1;30mHello World\u{1b}[2J");
    assert_eq!(sgr_values(&actions(&text_between)[0]), Some(vec![1, 30]));
    assert_eq!(
        actions(&text_between)[1],
        OutputAction::PrintString("Hello World".encode_utf16().collect())
    );
    assert_eq!(actions(&text_between)[2], OutputAction::EraseInDisplay(2));

    let mut split = machine();
    split.process_str("\u{1b}[1;");
    assert!(actions(&split).is_empty());
    split.process_str("30mHello World\u{1b}[2J");
    assert_eq!(sgr_values(&actions(&split)[0]), Some(vec![1, 30]));
    assert_eq!(
        actions(&split)[1],
        OutputAction::PrintString("Hello World".encode_utf16().collect())
    );
    assert_eq!(actions(&split)[2], OutputAction::EraseInDisplay(2));

    let mut mixed = machine();
    mixed.process_str("\u{1b}[1;");
    mixed.process_code_unit(u16::from(b'3'));
    mixed.process_code_unit(u16::from(b'0'));
    assert!(actions(&mixed).is_empty());
    mixed.process_code_unit(u16::from(b'm'));
    mixed.process_str("Hello World\u{1b}[2J");
    assert_eq!(sgr_values(&actions(&mixed)[0]), Some(vec![1, 30]));
    assert_eq!(
        actions(&mixed)[1],
        OutputAction::PrintString("Hello World".encode_utf16().collect())
    );
    assert_eq!(actions(&mixed)[2], OutputAction::EraseInDisplay(2));
}

#[test]
fn microsoft_output_osc_get_color_table_entry_matches_set_query_and_truncation_rules() {
    let mut queries = machine();
    queries.process_str("\u{1b}]4;0;?;1;?;2;;3;?;4;?\u{1b}\\");
    assert_eq!(
        actions(&queries),
        [
            OutputAction::RequestColorTableEntry(0),
            OutputAction::RequestColorTableEntry(1),
            OutputAction::RequestColorTableEntry(3),
            OutputAction::RequestColorTableEntry(4),
        ]
    );

    let mut mixed = machine();
    mixed.process_str(
        "\u{1b}]4;0;rgb:00/00/00;1;rgb:00/00/01;2;?;3;rgb:00/00/03;4;rgb:00/00/04\u{1b}\\",
    );
    assert_eq!(
        actions(&mixed),
        [
            OutputAction::SetColorTableEntry { index: 0, color: 0 },
            OutputAction::SetColorTableEntry {
                index: 1,
                color: 0x0001_0000,
            },
            OutputAction::RequestColorTableEntry(2),
            OutputAction::SetColorTableEntry {
                index: 3,
                color: 0x0003_0000,
            },
            OutputAction::SetColorTableEntry {
                index: 4,
                color: 0x0004_0000,
            },
        ]
    );

    let mut truncated = machine();
    truncated.process_str("\u{1b}]4;0;rgb:f0/00/00;1;?;3\u{1b}\\");
    assert_eq!(
        actions(&truncated),
        [
            OutputAction::SetColorTableEntry {
                index: 0,
                color: 0x0000_00f0,
            },
            OutputAction::RequestColorTableEntry(1),
        ]
    );
}

#[test]
fn microsoft_output_osc_xterm_resource_report_matches_skip_set_and_query_vectors() {
    let mut machine = machine();
    machine.process_str("\u{1b}]10;?\u{1b}\\");
    machine.process_str("\u{1b}]10;;?\u{1b}\\");
    machine.process_str("\u{1b}]10;rgb:11/22/33;?\u{1b}\\");
    machine.process_str("\u{1b}]12;rgb:11/22/33;?\u{1b}\\");
    machine.process_str("\u{1b}]10;?;?;?;?;?;?;?;?;?;?\u{1b}\\");

    let recorded = actions(&machine);
    assert_eq!(recorded[0], OutputAction::RequestXtermColorResource(10));
    assert_eq!(recorded[1], OutputAction::RequestXtermColorResource(11));
    assert_eq!(
        recorded[2],
        OutputAction::SetXtermColorResource {
            resource: 10,
            color: 0x0033_2211,
        }
    );
    assert_eq!(recorded[3], OutputAction::RequestXtermColorResource(11));
    assert_eq!(
        recorded[4],
        OutputAction::SetXtermColorResource {
            resource: 12,
            color: 0x0033_2211,
        }
    );
    assert_eq!(recorded[5], OutputAction::RequestXtermColorResource(13));
    assert_eq!(
        &recorded[6..],
        (10usize..20)
            .map(OutputAction::RequestXtermColorResource)
            .collect::<Vec<_>>()
            .as_slice()
    );
}

#[test]
fn microsoft_output_osc_xterm_resource_reset_requires_empty_payload() {
    let mut machine = machine();
    machine.process_str("\u{1b}]110\u{1b}\\");
    machine.process_str("\u{1b}]111;\u{1b}\\");
    machine.process_str("\u{1b}]111;110\u{1b}\\");
    assert_eq!(
        actions(&machine),
        [
            OutputAction::ResetXtermColorResource(10),
            OutputAction::ResetXtermColorResource(11),
        ]
    );
}

#[test]
fn microsoft_output_osc_color_table_reset_stops_at_first_unparseable_index() {
    let mut all = machine();
    all.process_str("\u{1b}]104\u{1b}\\");
    assert_eq!(actions(&all), [OutputAction::ResetColorTable]);

    let mut selected = machine();
    selected.process_str("\u{1b}]104;1;3;5;7;9\u{1b}\\");
    assert_eq!(
        actions(&selected),
        [1usize, 3, 5, 7, 9].map(OutputAction::ResetColorTableEntry)
    );

    let mut invalid = machine();
    invalid.process_str("\u{1b}]104;1;a;3\u{1b}\\");
    assert_eq!(actions(&invalid), [OutputAction::ResetColorTableEntry(1)]);

    let mut empty = machine();
    empty.process_str("\u{1b}]104;;;\u{1b}\\");
    assert!(actions(&empty).is_empty());
}

#[test]
fn microsoft_output_osc_window_title_covers_all_four_data_driven_numbers() {
    for osc in [0, 1, 2, 21] {
        let mut machine = machine();
        machine.process_str(&format!("\x1b]{osc};Title Text\x1b\\"));
        machine.process_str(&format!("\x1b]{osc};\x1b\\"));
        machine.process_str(&format!("\x1b]{osc}\x1b\\"));
        assert_eq!(
            actions(&machine),
            [
                OutputAction::SetWindowTitle("Title Text".to_owned()),
                OutputAction::SetWindowTitle(String::new()),
                OutputAction::SetWindowTitle(String::new()),
            ],
            "OSC {osc}"
        );
    }
}

#[test]
fn microsoft_output_osc_clipboard_matches_unicode_and_invalid_payload_contract() {
    let valid = [
        (";Zm9v", "foo"),
        (";Zm9vDQpiYXI=", "foo\r\nbar"),
        (";44Gr44G744KT44GU5rGJ6K+t7ZWc6rWt", "にほんご汉语한국"),
        (
            ";8J+RjfCfkY3wn4+78J+RjfCfj7zwn5GN8J+PvfCfkY3wn4++8J+RjfCfj78=",
            "👍👍🏻👍🏼👍🏽👍🏾👍🏿",
        ),
        ("s0;Zm9v", "foo"),
    ];
    for (payload, expected) in valid {
        let mut machine = machine();
        machine.process_str(&format!("\x1b]52;{payload}\x07"));
        assert_eq!(
            actions(&machine),
            [OutputAction::SetClipboard(expected.to_owned())],
            "payload={payload:?}"
        );
    }

    for payload in ["Zm9v", ";???", ";;Zm9v", ";?", "?", ";;?"] {
        let mut machine = machine();
        machine.process_str(&format!("\x1b]52;{payload}\x07"));
        assert!(actions(&machine).is_empty(), "payload={payload:?}");
    }
}

#[test]
fn microsoft_output_osc_hyperlink_matches_ids_parameters_queries_and_close() {
    let cases = [
        ("", "test.url", ""),
        ("id=testId", "test2.url", "testId"),
        ("id=testId", "https://example.com", "testId"),
        ("id=testId:foo=bar", "https://example.com", "testId"),
        ("foo=bar:id=testId", "https://example.com", "testId"),
        ("id=testId", "https://example.com?query1=value1", "testId"),
        (
            "id=testId",
            "https://example.com?query1=value1;value2;value3",
            "testId",
        ),
    ];

    for (parameters, uri, custom_id) in cases {
        let mut machine = machine();
        machine.process_str(&format!("\x1b]8;{parameters};{uri}\x1b\\\x1b]8;;\x1b\\"));
        assert_eq!(
            actions(&machine),
            [
                OutputAction::AddHyperlink {
                    uri: uri.to_owned(),
                    custom_id: custom_id.to_owned(),
                },
                OutputAction::EndHyperlink,
            ],
            "parameters={parameters:?}, uri={uri:?}"
        );
    }
}

#[test]
fn microsoft_output_c1_parser_mode_matches_disabled_and_enabled_sequences() {
    let mut disabled_csi = machine();
    disabled_csi.set_parser_mode(ParserMode::AcceptC1, false);
    disabled_csi.process_str("\u{009b}123A");
    assert_eq!(
        actions(&disabled_csi),
        [OutputAction::PrintString("123A".encode_utf16().collect())]
    );

    let mut enabled_csi = machine();
    enabled_csi.set_parser_mode(ParserMode::AcceptC1, true);
    enabled_csi.process_str("\u{009b}123A");
    assert_eq!(actions(&enabled_csi), [OutputAction::CursorUp(123)]);

    let mut disabled_nel = machine();
    disabled_nel.set_parser_mode(ParserMode::AcceptC1, false);
    disabled_nel.process_str("\u{1b}[12\u{0085};34H");
    assert_eq!(
        actions(&disabled_nel),
        [OutputAction::CursorPosition {
            line: 12,
            column: 34,
        }]
    );

    let mut enabled_nel = machine();
    enabled_nel.set_parser_mode(ParserMode::AcceptC1, true);
    enabled_nel.process_str("\u{1b}[12\u{0085};34H");
    assert_eq!(
        actions(&enabled_nel),
        [
            OutputAction::LineFeed(LineFeedType::WithReturn),
            OutputAction::PrintString(";34H".encode_utf16().collect()),
        ]
    );
}
