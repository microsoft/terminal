use terminal_parser::state_machine::{
    MAX_PARAMETER_VALUE, Parameters, ParserMode, State, StateMachine, StateMachineEngine, VtId,
};

#[derive(Default)]
struct RecordingEngine {
    osc: Vec<(i32, Vec<u16>)>,
    dcs: Vec<(VtId, Parameters)>,
    csi: Vec<(VtId, Parameters)>,
}

impl StateMachineEngine for RecordingEngine {
    fn action_esc_dispatch(&mut self, _id: VtId) -> bool {
        true
    }

    fn action_csi_dispatch(&mut self, id: VtId, parameters: &Parameters) -> bool {
        self.csi.push((id, parameters.clone()));
        true
    }

    fn action_osc_dispatch(&mut self, parameter: i32, text: &[u16]) -> bool {
        self.osc.push((parameter, text.to_vec()));
        true
    }

    fn action_dcs_dispatch(&mut self, id: VtId, parameters: &Parameters) -> bool {
        self.dcs.push((id, parameters.clone()));
        false
    }
}

fn machine() -> StateMachine<RecordingEngine> {
    StateMachine::new(RecordingEngine::default())
}

fn text(units: &[u16]) -> String {
    String::from_utf16_lossy(units)
}

#[test]
fn microsoft_output_c1_osc_enters_and_bel_terminates() {
    let mut machine = machine();
    machine.set_parser_mode(ParserMode::AcceptC1, true);
    machine.process_code_unit(0x9d);
    assert_eq!(machine.state(), State::OscParam);
    machine.process_code_unit(0x07);
    assert_eq!(machine.state(), State::Ground);
}

#[test]
fn microsoft_output_osc_string_supports_bel_and_st_termination() {
    let mut machine = machine();
    machine.process_str("\u{1b}]0;some text\u{7}");
    assert_eq!(machine.state(), State::Ground);
    machine.process_str("\u{1b}]0;some text\u{1b}\\");
    assert_eq!(machine.state(), State::Ground);

    assert_eq!(machine.engine().osc.len(), 2);
    for (parameter, units) in &machine.engine().osc {
        assert_eq!(*parameter, 0);
        assert_eq!(text(units), "some text");
    }
}

#[test]
fn microsoft_output_osc_string_grows_past_legacy_256_limit() {
    let payload = "s".repeat(260);
    let mut machine = machine();
    machine.process_str(&format!("\u{1b}]0;{payload}\u{7}"));
    assert_eq!(machine.state(), State::Ground);
    assert_eq!(machine.engine().osc.len(), 1);
    assert_eq!(machine.engine().osc[0].1.len(), 260);
}

#[test]
fn microsoft_output_osc_parameter_matches_12345() {
    let mut machine = machine();
    machine.process_str("\u{1b}]12345;s\u{7}");
    assert_eq!(machine.engine().osc.len(), 1);
    assert_eq!(machine.engine().osc[0].0, 12_345);
    assert_eq!(text(&machine.engine().osc[0].1), "s");
}

#[test]
fn microsoft_output_osc_parameter_accepts_many_leading_zeroes() {
    let sequence = format!("\u{1b}]{}12345;s\u{7}", "0".repeat(50));
    let mut machine = machine();
    machine.process_str(&sequence);
    assert_eq!(machine.engine().osc[0].0, 12_345);
}

#[test]
fn microsoft_output_osc_parameter_saturates_at_max_parameter_value() {
    let huge = usize::MAX.to_string();
    let mut machine = machine();
    for _ in 0..2 {
        machine.process_str(&format!("\u{1b}]{huge};s\u{7}"));
    }
    assert_eq!(machine.engine().osc.len(), 2);
    assert!(
        machine
            .engine()
            .osc
            .iter()
            .all(|(parameter, _)| *parameter == MAX_PARAMETER_VALUE)
    );
}

#[test]
fn microsoft_output_invalid_osc_termination_becomes_csi() {
    let mut machine = machine();
    machine.process_str("\u{1b}]1;s\u{1b}[4;m");
    assert_eq!(machine.state(), State::Ground);
    assert!(machine.engine().osc.is_empty());
    assert_eq!(machine.engine().csi.len(), 1);
    assert_eq!(machine.engine().csi[0].1.values(), &[Some(4), None]);
}

#[test]
fn microsoft_output_dcs_entry_terminates_with_st() {
    let mut machine = machine();
    machine.process_str("\u{1b}P");
    assert_eq!(machine.state(), State::DcsEntry);
    machine.process_str("\u{1b}\\");
    assert_eq!(machine.state(), State::Ground);
}

#[test]
fn microsoft_output_c1_dcs_entry_matches_seven_bit_form() {
    let mut machine = machine();
    machine.set_parser_mode(ParserMode::AcceptC1, true);
    machine.process_code_unit(0x90);
    assert_eq!(machine.state(), State::DcsEntry);
    machine.process_str("\u{1b}\\");
    assert_eq!(machine.state(), State::Ground);
}

#[test]
fn microsoft_output_dcs_intermediate_state_accepts_multiple_intermediates() {
    let mut machine = machine();
    machine.process_str("\u{1b}P");
    assert_eq!(machine.state(), State::DcsEntry);
    for intermediate in *b" #%" {
        machine.process_code_unit(u16::from(intermediate));
        assert_eq!(machine.state(), State::DcsIntermediate);
    }
    machine.process_str("\u{1b}\\");
    assert_eq!(machine.state(), State::Ground);
}

#[test]
fn microsoft_output_dcs_colon_enters_ignore_until_st() {
    let mut machine = machine();
    machine.process_str("\u{1b}P:");
    assert_eq!(machine.state(), State::DcsIgnore);
    machine.process_str("\u{1b}\\");
    assert_eq!(machine.state(), State::Ground);
}

#[test]
fn microsoft_output_dcs_parameter_vector_matches_omitted_values() {
    let mut machine = machine();
    machine.process_str("\u{1b}P;324;;8q");
    assert_eq!(machine.state(), State::DcsIgnore);
    assert_eq!(machine.engine().dcs.len(), 1);
    assert_eq!(
        machine.engine().dcs[0].1.values(),
        &[None, Some(324), None, Some(8)]
    );
    machine.process_str("\u{1b}\\");
    assert_eq!(machine.state(), State::Ground);
}

#[test]
fn microsoft_output_dcs_without_handler_ignores_passthrough_data() {
    let mut machine = machine();
    machine.process_str("\u{1b}P x");
    assert_eq!(machine.state(), State::DcsIgnore);
    assert_eq!(machine.engine().dcs.len(), 1);
    machine.process_str("\u{1b}\\");
    assert_eq!(machine.state(), State::Ground);
}

#[test]
fn microsoft_output_dcs_ignore_accepts_long_payload_until_st() {
    let mut machine = machine();
    machine.process_str("\u{1b}Pq#1NNN");
    assert_eq!(machine.state(), State::DcsIgnore);
    machine.process_str("\u{1b}\\");
    assert_eq!(machine.state(), State::Ground);
}

#[test]
fn microsoft_output_invalid_dcs_termination_becomes_csi() {
    let mut machine = machine();
    machine.process_str("\u{1b}Pq#\u{1b}[4;m");
    assert_eq!(machine.state(), State::Ground);
    assert_eq!(machine.engine().csi.len(), 1);
    assert_eq!(machine.engine().csi[0].1.values(), &[Some(4), None]);
}

#[test]
fn microsoft_output_sos_pm_apc_strings_ignore_payload_until_st() {
    for introducer in ['X', '^', '_'] {
        let mut machine = machine();
        machine.process_str(&format!("\u{1b}{introducer}12"));
        assert_eq!(machine.state(), State::SosPmApcString);
        machine.process_str("\u{1b}\\");
        assert_eq!(machine.state(), State::Ground);
    }
}

#[test]
fn microsoft_output_c1_st_terminates_osc_dcs_and_sos_pm_apc() {
    let mut machine = machine();
    machine.set_parser_mode(ParserMode::AcceptC1, true);

    machine.process_str("\u{1b}]1;s");
    assert_eq!(machine.state(), State::OscString);
    machine.process_code_unit(0x9c);
    assert_eq!(machine.state(), State::Ground);

    machine.process_str("\u{1b}Pq#1");
    assert_eq!(machine.state(), State::DcsIgnore);
    machine.process_code_unit(0x9c);
    assert_eq!(machine.state(), State::Ground);

    for introducer in ['X', '^', '_'] {
        machine.process_str(&format!("\u{1b}{introducer}1"));
        assert_eq!(machine.state(), State::SosPmApcString);
        machine.process_code_unit(0x9c);
        assert_eq!(machine.state(), State::Ground);
    }
}
