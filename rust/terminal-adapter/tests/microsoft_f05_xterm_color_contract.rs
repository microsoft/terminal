use terminal_adapter::{
    adapt_dispatch::PageGeometry, presentation_state::AdaptDispatchPresentationState,
};
use terminal_buffer::{text_attribute::TextAttribute, text_color::TextColor};
use terminal_parser::{output_engine::OutputStateMachineEngine, state_machine::StateMachine};

fn attributes_after(sequence: &str) -> TextAttribute {
    let dispatch = AdaptDispatchPresentationState::new(PageGeometry::new(20, 100, 29));
    let mut machine = StateMachine::new(OutputStateMachineEngine::new(dispatch));
    machine.process_str(sequence);
    machine.engine().dispatch().current_attributes()
}

#[test]
fn microsoft_xterm_256_color_test_matches_all_five_source_cases() {
    let dispatch = AdaptDispatchPresentationState::new(PageGeometry::new(20, 100, 29));
    let mut machine = StateMachine::new(OutputStateMachineEngine::new(dispatch));

    machine.process_str("\u{1b}[38;5;2m");
    assert_eq!(
        machine
            .engine()
            .dispatch()
            .current_attributes()
            .foreground(),
        TextColor::index256(TextColor::DARK_GREEN)
    );

    machine.process_str("\u{1b}[48;5;9m");
    assert_eq!(
        machine
            .engine()
            .dispatch()
            .current_attributes()
            .background(),
        TextColor::index256(TextColor::BRIGHT_RED)
    );

    machine.process_str("\u{1b}[38;5;42m");
    assert_eq!(
        machine
            .engine()
            .dispatch()
            .current_attributes()
            .foreground(),
        TextColor::index256(42)
    );

    machine.process_str("\u{1b}[48;5;142m");
    assert_eq!(
        machine
            .engine()
            .dispatch()
            .current_attributes()
            .background(),
        TextColor::index256(142)
    );

    machine.process_str("\u{1b}[38;5;9m");
    assert_eq!(
        machine
            .engine()
            .dispatch()
            .current_attributes()
            .foreground(),
        TextColor::index256(TextColor::BRIGHT_RED)
    );
}

#[test]
fn microsoft_extended_color_default_parameter_test_matches_source_contract() {
    let attributes = attributes_after("\u{1b}[38;5m");
    assert_eq!(
        attributes.foreground(),
        TextColor::index256(TextColor::DARK_BLACK)
    );

    let attributes = attributes_after("\u{1b}[48;5;m");
    assert_eq!(
        attributes.background(),
        TextColor::index256(TextColor::DARK_BLACK)
    );

    let attributes = attributes_after("\u{1b}[38;2m");
    assert_eq!(attributes.foreground(), TextColor::rgb(0, 0, 0));

    let attributes = attributes_after("\u{1b}[48;2;123m");
    assert_eq!(attributes.background(), TextColor::rgb(123, 0, 0));

    let attributes = attributes_after("\u{1b}[38;2;;;123m");
    assert_eq!(attributes.foreground(), TextColor::rgb(0, 0, 123));

    let default = TextAttribute::default();
    assert_eq!(attributes_after("\u{1b}[38;2;283;182;123m"), default);
    assert_eq!(attributes_after("\u{1b}[38;5;283m"), default);
}

#[test]
fn microsoft_extended_subparameter_color_test_matches_source_contract() {
    let attributes = attributes_after("\u{1b}[38:5m");
    assert_eq!(
        attributes.foreground(),
        TextColor::index256(TextColor::DARK_BLACK)
    );

    let attributes = attributes_after("\u{1b}[48:5:m");
    assert_eq!(
        attributes.background(),
        TextColor::index256(TextColor::DARK_BLACK)
    );

    let attributes = attributes_after("\u{1b}[38:2m");
    assert_eq!(attributes.foreground(), TextColor::rgb(0, 0, 0));

    let attributes = attributes_after("\u{1b}[48:2::123m");
    assert_eq!(attributes.background(), TextColor::rgb(123, 0, 0));

    let attributes = attributes_after("\u{1b}[38:2::::123m");
    assert_eq!(attributes.foreground(), TextColor::rgb(0, 0, 123));

    let default = TextAttribute::default();
    assert_eq!(attributes_after("\u{1b}[38:2:7:182:182:123m"), default);
    assert_eq!(attributes_after("\u{1b}[48:2::128:283:155m"), default);
    assert_eq!(attributes_after("\u{1b}[38:5:283m"), default);
}
