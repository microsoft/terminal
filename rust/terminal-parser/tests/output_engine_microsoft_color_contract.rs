use terminal_parser::output_engine::{OutputAction, OutputStateMachineEngine, TermDispatch};
use terminal_parser::state_machine::StateMachine;

#[derive(Default)]
struct RecordingDispatch {
    actions: Vec<OutputAction>,
}

impl TermDispatch for RecordingDispatch {
    fn dispatch(&mut self, action: OutputAction) {
        self.actions.push(action);
    }
}

type Machine = StateMachine<OutputStateMachineEngine<RecordingDispatch>>;

fn machine() -> Machine {
    StateMachine::new(OutputStateMachineEngine::new(RecordingDispatch::default()))
}

fn process(machine: &mut Machine, sequence: &str) -> Vec<OutputAction> {
    machine.engine_mut().dispatch_mut().actions.clear();
    machine.process_str(sequence);
    machine.engine().dispatch().actions.clone()
}

fn resource(resource: usize, red: u8, green: u8, blue: u8) -> OutputAction {
    OutputAction::SetXtermColorResource {
        resource,
        color: u32::from(red) | (u32::from(green) << 8) | (u32::from(blue) << 16),
    }
}

fn table(index: usize, red: u8, green: u8, blue: u8) -> OutputAction {
    OutputAction::SetColorTableEntry {
        index,
        color: u32::from(red) | (u32::from(green) << 8) | (u32::from(blue) << 16),
    }
}

#[test]
fn microsoft_output_osc_set_default_foreground_matches_all_reference_vectors() {
    let mut machine = machine();
    let cases = [
        (
            "\u{1b}]10;rgb:1/1/1\u{1b}\\",
            vec![resource(10, 0x11, 0x11, 0x11)],
        ),
        (
            "\u{1b}]10;rgb:12/34/56\u{1b}\\",
            vec![resource(10, 0x12, 0x34, 0x56)],
        ),
        (
            "\u{1b}]10;#111\u{1b}\\",
            vec![resource(10, 0x10, 0x10, 0x10)],
        ),
        (
            "\u{1b}]10;#123456\u{1b}\\",
            vec![resource(10, 0x12, 0x34, 0x56)],
        ),
        (
            "\u{1b}]10;DarkOrange\u{1b}\\",
            vec![resource(10, 255, 140, 0)],
        ),
        (
            "\u{1b}]10;#111;rgb:2/2/2\u{1b}\\",
            vec![
                resource(10, 0x10, 0x10, 0x10),
                resource(11, 0x22, 0x22, 0x22),
            ],
        ),
        (
            "\u{1b}]10;#111;DarkOrange\u{1b}\\",
            vec![resource(10, 0x10, 0x10, 0x10), resource(11, 255, 140, 0)],
        ),
        (
            "\u{1b}]10;#111;DarkOrange;rgb:2/2/2\u{1b}\\",
            vec![
                resource(10, 0x10, 0x10, 0x10),
                resource(11, 255, 140, 0),
                resource(12, 0x22, 0x22, 0x22),
            ],
        ),
        (
            "\u{1b}]10;#111;\u{1b}\\",
            vec![resource(10, 0x10, 0x10, 0x10)],
        ),
        (
            "\u{1b}]10;#111;rgb:\u{1b}\\",
            vec![resource(10, 0x10, 0x10, 0x10)],
        ),
        (
            "\u{1b}]10;#111;#2\u{1b}\\",
            vec![resource(10, 0x10, 0x10, 0x10)],
        ),
        (
            "\u{1b}]10;;rgb:1/1/1\u{1b}\\",
            vec![resource(11, 0x11, 0x11, 0x11)],
        ),
        (
            "\u{1b}]10;#1;rgb:1/1/1\u{1b}\\",
            vec![resource(11, 0x11, 0x11, 0x11)],
        ),
        ("\u{1b}]10;rgb:1/1/\u{1b}\\", vec![]),
        ("\u{1b}]10;#1\u{1b}\\", vec![]),
    ];

    for (sequence, expected) in cases {
        assert_eq!(
            process(&mut machine, sequence),
            expected,
            "sequence={sequence:?}"
        );
    }
}

#[test]
fn microsoft_output_osc_set_default_background_matches_all_reference_vectors() {
    let mut machine = machine();
    let cases = [
        (
            "\u{1b}]11;rgb:1/1/1\u{1b}\\",
            vec![resource(11, 0x11, 0x11, 0x11)],
        ),
        (
            "\u{1b}]11;rgb:12/34/56\u{1b}\\",
            vec![resource(11, 0x12, 0x34, 0x56)],
        ),
        (
            "\u{1b}]11;#111\u{1b}\\",
            vec![resource(11, 0x10, 0x10, 0x10)],
        ),
        (
            "\u{1b}]11;#123456\u{1b}\\",
            vec![resource(11, 0x12, 0x34, 0x56)],
        ),
        (
            "\u{1b}]11;DarkOrange\u{1b}\\",
            vec![resource(11, 255, 140, 0)],
        ),
        (
            "\u{1b}]11;#111;rgb:2/2/2\u{1b}\\",
            vec![
                resource(11, 0x10, 0x10, 0x10),
                resource(12, 0x22, 0x22, 0x22),
            ],
        ),
        (
            "\u{1b}]11;#111;DarkOrange\u{1b}\\",
            vec![resource(11, 0x10, 0x10, 0x10), resource(12, 255, 140, 0)],
        ),
        (
            "\u{1b}]11;#111;DarkOrange;rgb:2/2/2\u{1b}\\",
            vec![
                resource(11, 0x10, 0x10, 0x10),
                resource(12, 255, 140, 0),
                resource(13, 0x22, 0x22, 0x22),
            ],
        ),
        (
            "\u{1b}]11;#111;\u{1b}\\",
            vec![resource(11, 0x10, 0x10, 0x10)],
        ),
        (
            "\u{1b}]11;#111;rgb:\u{1b}\\",
            vec![resource(11, 0x10, 0x10, 0x10)],
        ),
        (
            "\u{1b}]11;#111;#2\u{1b}\\",
            vec![resource(11, 0x10, 0x10, 0x10)],
        ),
        (
            "\u{1b}]11;;rgb:1/1/1\u{1b}\\",
            vec![resource(12, 0x11, 0x11, 0x11)],
        ),
        (
            "\u{1b}]11;#1;rgb:1/1/1\u{1b}\\",
            vec![resource(12, 0x11, 0x11, 0x11)],
        ),
        ("\u{1b}]11;rgb:1/1/\u{1b}\\", vec![]),
        ("\u{1b}]11;#1\u{1b}\\", vec![]),
    ];

    for (sequence, expected) in cases {
        assert_eq!(
            process(&mut machine, sequence),
            expected,
            "sequence={sequence:?}"
        );
    }
}

#[test]
fn microsoft_output_osc_set_color_table_entry_matches_valid_partial_and_invalid_vectors() {
    let mut machine = machine();
    let cases = [
        (
            "\u{1b}]4;0;rgb:1/1/1\u{1b}\\",
            vec![table(0, 0x11, 0x11, 0x11)],
        ),
        (
            "\u{1b}]4;16;rgb:11/11/11\u{1b}\\",
            vec![table(16, 0x11, 0x11, 0x11)],
        ),
        (
            "\u{1b}]4;64;#111\u{1b}\\",
            vec![table(64, 0x10, 0x10, 0x10)],
        ),
        ("\u{1b}]4;128;orange\u{1b}\\", vec![table(128, 255, 165, 0)]),
        ("\u{1b}]4;\u{1b}\\", vec![]),
        ("\u{1b}]4;;\u{1b}\\", vec![]),
        ("\u{1b}]4;0\u{1b}\\", vec![]),
        ("\u{1b}]4;111\u{1b}\\", vec![]),
        ("\u{1b}]4;#111\u{1b}\\", vec![]),
        ("\u{1b}]4;1;111\u{1b}\\", vec![]),
        ("\u{1b}]4;1;rgb:\u{1b}\\", vec![]),
        (
            "\u{1b}]4;0;rgb:1/1/1;16;rgb:2/2/2\u{1b}\\",
            vec![table(0, 0x11, 0x11, 0x11), table(16, 0x22, 0x22, 0x22)],
        ),
        (
            "\u{1b}]4;0;rgb:1/1/1;16;rgb:2/2/2;64;#111\u{1b}\\",
            vec![
                table(0, 0x11, 0x11, 0x11),
                table(16, 0x22, 0x22, 0x22),
                table(64, 0x10, 0x10, 0x10),
            ],
        ),
        (
            "\u{1b}]4;0;rgb:1/1/1;16;rgb:2/2/2;64;#111;128;orange\u{1b}\\",
            vec![
                table(0, 0x11, 0x11, 0x11),
                table(16, 0x22, 0x22, 0x22),
                table(64, 0x10, 0x10, 0x10),
                table(128, 255, 165, 0),
            ],
        ),
        (
            "\u{1b}]4;0;rgb:11;1;rgb:2/2/2;2;#111;3;orange;4;#111\u{1b}\\",
            vec![
                table(1, 0x22, 0x22, 0x22),
                table(2, 0x10, 0x10, 0x10),
                table(3, 255, 165, 0),
                table(4, 0x10, 0x10, 0x10),
            ],
        ),
        (
            "\u{1b}]4;0;rgb:1/1/1;1;rgb:2/2/2;2;#111;3;orange;4;111\u{1b}\\",
            vec![
                table(0, 0x11, 0x11, 0x11),
                table(1, 0x22, 0x22, 0x22),
                table(2, 0x10, 0x10, 0x10),
                table(3, 255, 165, 0),
            ],
        ),
        (
            "\u{1b}]4;0;rgb:1/1/1;1;rgb:2;2;#111;3;orange;4;#222\u{1b}\\",
            vec![
                table(0, 0x11, 0x11, 0x11),
                table(2, 0x10, 0x10, 0x10),
                table(3, 255, 165, 0),
                table(4, 0x20, 0x20, 0x20),
            ],
        ),
        ("\u{1b}]4;0;;1;;\u{1b}\\", vec![]),
        ("\u{1b}]4;0;;;;;1;;;;;\u{1b}\\", vec![]),
        ("\u{1b}]4;0;rgb:1/1/;16;rgb:2/2/;64;#11\u{1b}\\", vec![]),
    ];

    for (sequence, expected) in cases {
        assert_eq!(
            process(&mut machine, sequence),
            expected,
            "sequence={sequence:?}"
        );
    }
}
