use terminal_adapter::adapt_dispatch::PageGeometry;
use terminal_adapter::dcs_dispatch::AdapterDispatch;
use terminal_parser::output_engine::OutputStateMachineEngine;
use terminal_parser::state_machine::StateMachine;

type Machine = StateMachine<OutputStateMachineEngine<AdapterDispatch>>;

fn machine() -> Machine {
    StateMachine::new(OutputStateMachineEngine::new(AdapterDispatch::new(
        PageGeometry::new(20, 100, 29),
    )))
}

fn assert_macro(machine: &Machine, id: usize, expected: &str) {
    let actual = machine
        .engine()
        .dispatch()
        .macro_buffer()
        .macro_contents(id)
        .expect("Microsoft macro id must be valid");
    let expected = expected.encode_utf16().collect::<Vec<_>>();
    assert_eq!(actual, expected.as_slice(), "macro id {id}");
}

#[test]
fn microsoft_adapter_macro_definitions_match_encodings_defaults_and_replacement() {
    let mut machine = machine();

    machine.process_str("\u{1b}P1;0;0!zText Encoding\u{1b}\\");
    assert_macro(&machine, 1, "Text Encoding");

    machine.process_str("\u{1b}P2;0;1!z486578204A4B4C4D4E4F\u{1b}\\");
    assert_macro(&machine, 2, "Hex JKLMNO");

    machine.process_str("\u{1b}P3;0;1!z486578206a6b6c6d6e6f\u{1b}\\");
    assert_macro(&machine, 3, "Hex jklmno");

    machine.process_str("\u{1b}P4;0;!zDefault Encoding\u{1b}\\");
    assert_macro(&machine, 4, "Default Encoding");

    machine.process_str("\u{1b}P;0;0!zDefault ID\u{1b}\\");
    assert_macro(&machine, 0, "Default ID");

    machine.process_str("\u{1b}P1;0;0!zRetained\u{1b}\\");
    machine.process_str("\u{1b}P2;0;0!zReplaced\u{1b}\\");
    machine.process_str("\u{1b}P2;0;0!zNew\u{1b}\\");
    assert_macro(&machine, 1, "Retained");
    assert_macro(&machine, 2, "New");

    machine.process_str("\u{1b}P1;0;0!zErased\u{1b}\\");
    machine.process_str("\u{1b}P2;0;0!zReplaced\u{1b}\\");
    machine.process_str("\u{1b}P2;1;0!zNew\u{1b}\\");
    assert_macro(&machine, 1, "");
    assert_macro(&machine, 2, "New");

    machine.process_str("\u{1b}P1;0;0!zRetained\u{1b}\\");
    machine.process_str("\u{1b}P2;0;0!zReplaced\u{1b}\\");
    machine.process_str("\u{1b}P2;;0!zNew\u{1b}\\");
    assert_macro(&machine, 1, "Retained");
    assert_macro(&machine, 2, "New");
}

#[test]
fn microsoft_adapter_macro_definitions_match_repeat_and_control_vectors() {
    let mut machine = machine();

    machine.process_str("\u{1b}P5;0;1!z526570656174!3;206563686F;207468726565\u{1b}\\");
    assert_macro(&machine, 5, "Repeat echo echo echo three");

    machine.process_str("\u{1b}P6;0;1!z526570656174!0;206563686F;207A65726F\u{1b}\\");
    assert_macro(&machine, 6, "Repeat echo zero");

    machine.process_str("\u{1b}P7;0;1!z526570656174!;206563686F;2064656661756C74\u{1b}\\");
    assert_macro(&machine, 7, "Repeat echo default");

    machine.process_str("\u{1b}P8;0;1!z556E7465726D696E61746564!3;206563686F\u{1b}\\");
    assert_macro(&machine, 8, "Unterminated echo echo echo");

    machine.process_str("\u{1b}P9;0;0!zReplaced\u{1b}\\");
    machine.process_str("\u{1b}P9;0;1!z526570656174!3;206563;686F;207468726565\u{1b}\\");
    assert_macro(&machine, 9, "");

    machine.process_str("\u{1b}P10;0;0!zA\u{7}B\u{8}C\tD\nE\u{b}F\u{c}G\rH\u{1b}\\");
    assert_macro(&machine, 10, "ABCDEFGH");

    machine.process_str("\u{1b}P11;0;1!z41\u{7}42\u{8}43\t44\n45\u{b}46\u{c}47\r48\u{1b}\\");
    assert_macro(&machine, 11, "ABCDEFGH");

    machine.process_str("\u{1b}P12;0;1!z!\u{7}3\u{8};\t4\n1\u{b}4\u{c}2\r4\u{7}3\u{8};\u{1b}\\");
    assert_macro(&machine, 12, "ABCABCABC");

    machine.process_str("\u{1b}P13;0;1!z410742084309440A450B460C470D481B49\u{1b}\\");
    assert_macro(&machine, 13, "A\u{7}B\u{8}C\tD\nE\u{b}F\u{c}G\rH\u{1b}I");
}
