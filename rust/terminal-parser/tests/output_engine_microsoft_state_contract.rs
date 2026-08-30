use terminal_parser::state_machine::{
    MAX_PARAMETER_COUNT, MAX_SUBPARAMETER_COUNT, Parameters, ParserMode, State, StateMachine,
    StateMachineEngine, VtId,
};

#[derive(Default)]
struct RecordingEngine {
    printed: Vec<u16>,
    executed: Vec<u16>,
    csi: Vec<(VtId, Parameters)>,
    dcs_handler: bool,
    dcs_data: Vec<u16>,
}

impl StateMachineEngine for RecordingEngine {
    fn action_execute(&mut self, code_unit: u16) -> bool {
        self.executed.push(code_unit);
        true
    }

    fn action_print_string(&mut self, text: &[u16]) -> bool {
        self.printed.extend_from_slice(text);
        true
    }

    fn action_esc_dispatch(&mut self, _id: VtId) -> bool {
        true
    }

    fn action_csi_dispatch(&mut self, id: VtId, parameters: &Parameters) -> bool {
        self.csi.push((id, parameters.clone()));
        true
    }

    fn action_dcs_dispatch(&mut self, _id: VtId, _parameters: &Parameters) -> bool {
        self.dcs_handler
    }

    fn action_dcs_put(&mut self, code_unit: u16) -> bool {
        self.dcs_data.push(code_unit);
        true
    }
}

fn machine() -> StateMachine<RecordingEngine> {
    StateMachine::new(RecordingEngine::default())
}

fn last_parameters(machine: &StateMachine<RecordingEngine>) -> &Parameters {
    &machine.engine().csi.last().expect("CSI must dispatch").1
}

fn assert_escape_from_output(sequence: &str, expected_before: State, expected_after: State) {
    let mut machine = machine();
    machine.process_str(sequence);
    assert_eq!(machine.state(), expected_before, "setup {sequence:?}");
    machine.process_code_unit(0x1b);
    assert_eq!(
        machine.state(),
        expected_after,
        "escape from {expected_before:?}"
    );
}

#[test]
fn microsoft_output_escape_path_covers_all_eighteen_source_states() {
    // OutputEngineTest::TestEscapePath directly assigns each internal state. Here
    // every output-reachable state is reached through the public parser API.
    assert_escape_from_output("", State::Ground, State::Escape);
    assert_escape_from_output("\u{1b}", State::Escape, State::Escape);
    assert_escape_from_output("\u{1b}#", State::EscapeIntermediate, State::Escape);
    assert_escape_from_output("\u{1b}[", State::CsiEntry, State::Escape);
    assert_escape_from_output("\u{1b}[4;=", State::CsiIgnore, State::Escape);
    assert_escape_from_output("\u{1b}[1", State::CsiParam, State::Escape);
    assert_escape_from_output("\u{1b}[#", State::CsiIntermediate, State::Escape);
    assert_escape_from_output("\u{1b}]", State::OscParam, State::OscTermination);
    assert_escape_from_output("\u{1b}]0;x", State::OscString, State::OscTermination);
    assert_escape_from_output("\u{1b}]0;x\u{1b}", State::OscTermination, State::Escape);
    assert_escape_from_output("\u{1b}P", State::DcsEntry, State::Escape);
    assert_escape_from_output("\u{1b}P:", State::DcsIgnore, State::Escape);
    assert_escape_from_output("\u{1b}P ", State::DcsIntermediate, State::Escape);
    assert_escape_from_output("\u{1b}P1", State::DcsParam, State::Escape);
    assert_escape_from_output("\u{1b}X1", State::SosPmApcString, State::Escape);

    // SS3 states are input-only in the migrated parser. Reaching them through
    // new_input is stronger than mutating a private state field as the C++ test
    // does; ESC still exercises the same global escape transition.
    let mut ss3_entry = StateMachine::new_input(RecordingEngine::default());
    ss3_entry.process_str("\u{1b}O");
    assert_eq!(ss3_entry.state(), State::Ss3Entry);
    ss3_entry.process_code_unit(0x1b);
    assert_eq!(ss3_entry.state(), State::Escape);

    let mut ss3_param = StateMachine::new_input(RecordingEngine::default());
    ss3_param.process_str("\u{1b}O1");
    assert_eq!(ss3_param.state(), State::Ss3Param);
    ss3_param.process_code_unit(0x1b);
    assert_eq!(ss3_param.state(), State::Escape);

    let engine = RecordingEngine {
        dcs_handler: true,
        ..RecordingEngine::default()
    };
    let mut dcs_passthrough = StateMachine::new(engine);
    dcs_passthrough.process_str("\u{1b}Pq");
    assert_eq!(dcs_passthrough.state(), State::DcsPassThrough);
    dcs_passthrough.process_code_unit(0x1b);
    assert_eq!(dcs_passthrough.state(), State::Escape);
    assert_eq!(dcs_passthrough.engine().dcs_data, [0x1b]);
}

#[test]
fn microsoft_output_escape_immediate_path_matches_state_trace() {
    let mut machine = machine();
    assert_eq!(machine.state(), State::Ground);

    machine.process_code_unit(0x1b);
    assert_eq!(machine.state(), State::Escape);
    for intermediate in *b"#()#" {
        machine.process_code_unit(u16::from(intermediate));
        assert_eq!(machine.state(), State::EscapeIntermediate);
    }
    machine.process_code_unit(u16::from(b'6'));
    assert_eq!(machine.state(), State::Ground);
}

#[test]
fn microsoft_output_escape_then_c0_executes_without_interrupting_sequence() {
    let mut machine = machine();
    machine.process_code_unit(0x1b);
    assert_eq!(machine.state(), State::Escape);
    machine.process_code_unit(0x03);
    assert_eq!(machine.state(), State::Escape);
    assert_eq!(machine.engine().executed, [0x03]);

    machine.process_str("[31m");
    assert_eq!(machine.state(), State::Ground);
    assert_eq!(last_parameters(&machine).values(), &[Some(31)]);
}

#[test]
fn microsoft_output_ground_print_remains_ground() {
    let mut machine = machine();
    machine.process_str("a");
    assert_eq!(machine.state(), State::Ground);
    assert_eq!(String::from_utf16_lossy(&machine.engine().printed), "a");
}

#[test]
fn microsoft_output_csi_entry_dispatches_immediate_final() {
    let mut machine = machine();
    machine.process_code_unit(0x1b);
    assert_eq!(machine.state(), State::Escape);
    machine.process_code_unit(u16::from(b'['));
    assert_eq!(machine.state(), State::CsiEntry);
    machine.process_code_unit(u16::from(b'm'));
    assert_eq!(machine.state(), State::Ground);
}

#[test]
fn microsoft_output_c1_csi_entry_matches_seven_bit_form() {
    let mut machine = machine();
    machine.set_parser_mode(ParserMode::AcceptC1, true);
    machine.process_code_unit(0x9b);
    assert_eq!(machine.state(), State::CsiEntry);
    machine.process_code_unit(u16::from(b'm'));
    assert_eq!(machine.state(), State::Ground);
}

#[test]
fn microsoft_output_csi_intermediates_remain_intermediate_until_final() {
    let mut machine = machine();
    machine.process_str("\u{1b}[");
    assert_eq!(machine.state(), State::CsiEntry);
    for intermediate in *b"$#%" {
        machine.process_code_unit(u16::from(intermediate));
        assert_eq!(machine.state(), State::CsiIntermediate);
    }
    machine.process_code_unit(u16::from(b'v'));
    assert_eq!(machine.state(), State::Ground);
}

#[test]
fn microsoft_output_csi_parameter_vector_matches_omitted_values() {
    let mut machine = machine();
    machine.process_str("\u{1b}[;324;;8J");
    assert_eq!(machine.state(), State::Ground);
    assert_eq!(
        last_parameters(&machine).values(),
        &[None, Some(324), None, Some(8)]
    );
}

#[test]
fn microsoft_output_csi_parameter_count_is_capped_at_32() {
    let mut sequence = String::from("\u{1b}[");
    for index in 0..100usize {
        if index > 0 {
            sequence.push(';');
        }
        sequence.push(char::from(
            b'0' + u8::try_from(index % 10).expect("single digit"),
        ));
    }
    sequence.push('J');

    let mut machine = machine();
    machine.process_str(&sequence);
    let values = last_parameters(&machine).values();
    assert_eq!(values.len(), MAX_PARAMETER_COUNT);
    for (index, value) in values.iter().enumerate() {
        assert_eq!(
            *value,
            Some(i32::try_from(index % 10).expect("single digit"))
        );
    }
}

#[test]
fn microsoft_output_csi_parameter_accepts_many_leading_zeroes() {
    let sequence = format!("\u{1b}[{}12345J", "0".repeat(50));
    let mut machine = machine();
    machine.process_str(&sequence);
    assert_eq!(last_parameters(&machine).values(), &[Some(12_345)]);
}

#[test]
fn microsoft_output_csi_subparameter_ranges_match_reference_vector() {
    let mut machine = machine();
    machine.process_str("\u{1b}[:3;9:5::8J");
    let parameters = last_parameters(&machine);

    assert_eq!(parameters.values(), &[None, Some(9)]);
    assert_eq!(parameters.sub_params_for(0), &[Some(3)]);
    assert_eq!(parameters.sub_params_for(1), &[Some(5), None, Some(8)]);
}

#[test]
fn microsoft_output_csi_subparameter_count_is_capped_per_parameter() {
    let mut sequence = String::from("\u{1b}[");
    for parameter in 0..2usize {
        if parameter > 0 {
            sequence.push(';');
        }
        sequence.push('3');
        for index in 0..100usize {
            sequence.push(':');
            sequence.push(char::from(
                b'0' + u8::try_from(index % 10).expect("single digit"),
            ));
        }
    }
    sequence.push('J');

    let mut machine = machine();
    machine.process_str(&sequence);
    let parameters = last_parameters(&machine);
    let expected = (0..MAX_SUBPARAMETER_COUNT)
        .map(|value| Some(i32::try_from(value).expect("small parameter")))
        .collect::<Vec<_>>();
    assert_eq!(parameters.sub_params_for(0), expected.as_slice());
    assert_eq!(parameters.sub_params_for(1), expected.as_slice());
}

#[test]
fn microsoft_output_csi_subparameter_accepts_many_leading_zeroes() {
    let sequence = format!("\u{1b}[3:{}12345J", "0".repeat(50));
    let mut machine = machine();
    machine.process_str(&sequence);
    assert_eq!(last_parameters(&machine).sub_params_for(0), &[Some(12_345)]);
}

#[test]
fn microsoft_output_csi_ignore_paths_return_to_ground_without_dispatch() {
    for sequence in ["\u{1b}[4;=8J", "\u{1b}[4#:8J"] {
        let mut machine = machine();
        machine.process_str(sequence);
        assert_eq!(machine.state(), State::Ground, "sequence {sequence:?}");
        assert!(machine.engine().csi.is_empty(), "sequence {sequence:?}");
    }
}
