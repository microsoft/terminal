use terminal_adapter::{
    adapt_dispatch::PageGeometry,
    parser_control::{
        CodingSystem, ISO_8859_1_CODE_PAGE, UTF8_CODE_PAGE, designate_coding_system,
        set_accept_c1_controls, set_ansi_mode,
    },
    product_dispatch::AdaptDispatchProductState,
};
use terminal_parser::{
    output_engine::OutputStateMachineEngine,
    state_machine::{ParserMode, StateMachine},
};

fn product_machine() -> StateMachine<OutputStateMachineEngine<AdaptDispatchProductState>> {
    let dispatch = AdaptDispatchProductState::new(PageGeometry::new(20, 100, 29));
    StateMachine::new(OutputStateMachineEngine::new(dispatch))
}

#[test]
fn microsoft_ansi_mode_test_mutates_the_live_state_machine_mode() {
    let mut machine = product_machine();

    machine.set_parser_mode(ParserMode::Ansi, false);
    assert!(!machine.get_parser_mode(ParserMode::Ansi));

    set_ansi_mode(&mut machine, true);
    assert!(machine.get_parser_mode(ParserMode::Ansi));

    set_ansi_mode(&mut machine, false);
    assert!(!machine.get_parser_mode(ParserMode::Ansi));
}

#[test]
fn microsoft_toggling_c1_parser_mode_matches_accept_controls_contract() {
    let mut machine = product_machine();
    machine.set_parser_mode(ParserMode::AcceptC1, false);

    set_accept_c1_controls(&mut machine, true);
    assert!(machine.get_parser_mode(ParserMode::AcceptC1));

    set_accept_c1_controls(&mut machine, false);
    assert!(!machine.get_parser_mode(ParserMode::AcceptC1));
}

#[test]
fn microsoft_toggling_c1_parser_mode_matches_coding_system_contract() {
    let mut machine = product_machine();
    machine.set_parser_mode(ParserMode::AcceptC1, false);

    assert_eq!(
        CodingSystem::from_designator(u64::from(b'@')),
        Some(CodingSystem::Iso2022)
    );
    let code_page = designate_coding_system(&mut machine, CodingSystem::Iso2022);
    assert_eq!(code_page, ISO_8859_1_CODE_PAGE);
    assert!(machine.get_parser_mode(ParserMode::AcceptC1));

    assert_eq!(
        CodingSystem::from_designator(u64::from(b'G')),
        Some(CodingSystem::Utf8)
    );
    let code_page = designate_coding_system(&mut machine, CodingSystem::Utf8);
    assert_eq!(code_page, UTF8_CODE_PAGE);
    assert!(!machine.get_parser_mode(ParserMode::AcceptC1));

    assert_eq!(CodingSystem::from_designator(u64::from(b'Z')), None);
}
