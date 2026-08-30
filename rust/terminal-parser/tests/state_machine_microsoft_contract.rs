use terminal_parser::state_machine::{Parameters, State, StateMachine, StateMachineEngine, VtId};

#[derive(Default)]
struct CaptureEngine {
    printed: Vec<u16>,
    passed_through: Vec<u16>,
}

impl StateMachineEngine for CaptureEngine {
    fn action_print_string(&mut self, text: &[u16]) -> bool {
        self.printed.extend_from_slice(text);
        true
    }

    fn action_pass_through_string(&mut self, text: &[u16]) -> bool {
        self.passed_through.extend_from_slice(text);
        true
    }

    fn action_csi_dispatch(&mut self, _id: VtId, _parameters: &Parameters) -> bool {
        false
    }
}

fn input_machine() -> StateMachine<CaptureEngine> {
    StateMachine::new_input(CaptureEngine::default())
}

#[test]
fn microsoft_passthrough_unhandled_sequence_before_printable_text() {
    let mut machine = StateMachine::new(CaptureEngine::default());

    machine.process_str("\u{1b}[?999h 12345 Hello World");

    assert_eq!(
        String::from_utf16(&machine.engine().passed_through)
            .expect("test sequence is valid UTF-16"),
        "\u{1b}[?999h"
    );
    assert_eq!(
        String::from_utf16(&machine.engine().printed).expect("test text is valid UTF-16"),
        " 12345 Hello World"
    );
}

#[test]
fn microsoft_chunked_csi_remains_in_parameter_state() {
    let mut machine = input_machine();
    machine.process_str("\u{1b}[1");
    assert_eq!(machine.state(), State::CsiParam);
}

#[test]
fn microsoft_ss3_entry_transitions_to_ground_after_dispatch() {
    let mut machine = input_machine();
    assert_eq!(machine.state(), State::Ground);

    machine.process_code_unit(0x1b);
    assert_eq!(machine.state(), State::Escape);
    machine.process_code_unit(u16::from(b'O'));
    assert_eq!(machine.state(), State::Ss3Entry);
    machine.process_code_unit(u16::from(b'm'));
    assert_eq!(machine.state(), State::Ground);
}

#[test]
fn microsoft_ss3_immediates_dispatch_directly_from_entry() {
    let mut machine = input_machine();

    for final_byte in *b"$#%?" {
        machine.process_code_unit(0x1b);
        assert_eq!(machine.state(), State::Escape);
        machine.process_code_unit(u16::from(b'O'));
        assert_eq!(machine.state(), State::Ss3Entry);
        machine.process_code_unit(u16::from(final_byte));
        assert_eq!(machine.state(), State::Ground);
    }
}

#[test]
fn microsoft_ss3_parameters_remain_parameter_state_until_final_byte() {
    let mut machine = input_machine();

    machine.process_code_unit(0x1b);
    assert_eq!(machine.state(), State::Escape);
    machine.process_code_unit(u16::from(b'O'));
    assert_eq!(machine.state(), State::Ss3Entry);

    for parameter_byte in *b";324;;8" {
        machine.process_code_unit(u16::from(parameter_byte));
        assert_eq!(machine.state(), State::Ss3Param);
    }

    machine.process_code_unit(u16::from(b'J'));
    assert_eq!(machine.state(), State::Ground);
}
