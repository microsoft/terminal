use terminal_adapter::{
    adapt_dispatch::PageGeometry, presentation_state::AdaptDispatchPresentationState,
};
use terminal_buffer::{
    text_attribute::{TextAttribute, UnderlineStyle},
    text_color::TextColor,
};
use terminal_parser::{
    output_engine::{OutputAction, OutputStateMachineEngine, TermDispatch},
    state_machine::{Parameters, StateMachine},
};

fn state() -> AdaptDispatchPresentationState {
    AdaptDispatchPresentationState::new(PageGeometry::new(20, 100, 29))
}

fn apply_single(starting: TextAttribute, option: i32) -> TextAttribute {
    let mut state = state();
    state.set_current_attributes(starting);
    state.dispatch(OutputAction::SetGraphicsRendition(Parameters::from_values(
        vec![Some(option)],
    )));
    state.current_attributes()
}

fn apply_sequence(sequence: &str) -> TextAttribute {
    let engine = OutputStateMachineEngine::new(state());
    let mut machine = StateMachine::new(engine);
    machine.process_str(sequence);
    machine.engine().dispatch().current_attributes()
}

#[test]
fn microsoft_graphics_base_empty_sgr_resets_attributes() {
    let mut state = state();
    let mut starting = TextAttribute::default();
    starting.set_intense(true);
    starting.set_faint(true);
    starting.set_reverse_video(true);
    starting.set_invisible(true);
    starting.set_crossed_out(true);
    starting.set_overlined(true);
    starting.set_underline_style(UnderlineStyle::Curly);
    starting.set_foreground(TextColor::index16(TextColor::BRIGHT_RED));
    starting.set_background(TextColor::index16(TextColor::BRIGHT_BLUE));
    state.set_current_attributes(starting);

    state.dispatch(OutputAction::SetGraphicsRendition(Parameters::default()));

    assert_eq!(state.current_attributes(), TextAttribute::default());
}

#[test]
fn microsoft_graphics_single_sgr_options_match_source_contract() {
    let default = TextAttribute::default();

    let mut fully_set = default;
    fully_set.set_intense(true);
    fully_set.set_faint(true);
    fully_set.set_reverse_video(true);
    fully_set.set_invisible(true);
    fully_set.set_crossed_out(true);
    fully_set.set_overlined(true);
    fully_set.set_underline_style(UnderlineStyle::Curly);
    fully_set.set_foreground(TextColor::index16(TextColor::BRIGHT_RED));
    fully_set.set_background(TextColor::index16(TextColor::BRIGHT_BLUE));
    assert_eq!(apply_single(fully_set, 0), default);

    let mut expected = default;
    expected.set_intense(true);
    assert_eq!(apply_single(default, 1), expected);

    expected = default;
    expected.set_faint(true);
    assert_eq!(apply_single(default, 2), expected);

    expected = default;
    expected.set_underline_style(UnderlineStyle::Single);
    assert_eq!(apply_single(default, 4), expected);

    expected = default;
    expected.set_reverse_video(true);
    assert_eq!(apply_single(default, 7), expected);

    expected = default;
    expected.set_invisible(true);
    assert_eq!(apply_single(default, 8), expected);

    expected = default;
    expected.set_crossed_out(true);
    assert_eq!(apply_single(default, 9), expected);

    expected = default;
    expected.set_underline_style(UnderlineStyle::Double);
    assert_eq!(apply_single(default, 21), expected);

    let mut intense_faint = default;
    intense_faint.set_intense(true);
    intense_faint.set_faint(true);
    assert_eq!(apply_single(intense_faint, 22), default);

    let mut underlined = default;
    underlined.set_underline_style(UnderlineStyle::Curly);
    assert_eq!(apply_single(underlined, 24), default);

    let mut reversed = default;
    reversed.set_reverse_video(true);
    assert_eq!(apply_single(reversed, 27), default);

    let mut invisible = default;
    invisible.set_invisible(true);
    assert_eq!(apply_single(invisible, 28), default);

    let mut crossed = default;
    crossed.set_crossed_out(true);
    assert_eq!(apply_single(crossed, 29), default);

    let mut overlined = default;
    overlined.set_overlined(true);
    assert_eq!(apply_single(overlined, 55), default);

    expected = default;
    expected.set_overlined(true);
    assert_eq!(apply_single(default, 53), expected);

    for (option, index) in (30..=37).zip(0..=7) {
        let mut starting = default;
        starting.set_background(TextColor::index16(TextColor::BRIGHT_MAGENTA));
        let mut expected = starting;
        expected.set_foreground(TextColor::index16(index));
        assert_eq!(apply_single(starting, option), expected, "SGR {option}");
    }

    let mut starting = default;
    starting.set_foreground(TextColor::index16(TextColor::BRIGHT_GREEN));
    starting.set_background(TextColor::index16(TextColor::BRIGHT_MAGENTA));
    expected = starting;
    expected.set_default_foreground();
    assert_eq!(apply_single(starting, 39), expected);

    for (option, index) in (40..=47).zip(0..=7) {
        let mut starting = default;
        starting.set_foreground(TextColor::index16(TextColor::BRIGHT_CYAN));
        let mut expected = starting;
        expected.set_background(TextColor::index16(index));
        assert_eq!(apply_single(starting, option), expected, "SGR {option}");
    }

    starting = default;
    starting.set_foreground(TextColor::index16(TextColor::BRIGHT_CYAN));
    starting.set_background(TextColor::index16(TextColor::BRIGHT_GREEN));
    expected = starting;
    expected.set_default_background();
    assert_eq!(apply_single(starting, 49), expected);

    for (option, index) in (90..=97).zip(8..=15) {
        let mut starting = default;
        starting.set_background(TextColor::index16(TextColor::DARK_GREEN));
        let mut expected = starting;
        expected.set_foreground(TextColor::index16(index));
        assert_eq!(apply_single(starting, option), expected, "SGR {option}");
    }

    for (option, index) in (100..=107).zip(8..=15) {
        let mut starting = default;
        starting.set_foreground(TextColor::index16(TextColor::DARK_YELLOW));
        let mut expected = starting;
        expected.set_background(TextColor::index16(index));
        assert_eq!(apply_single(starting, option), expected, "SGR {option}");
    }
}

#[test]
fn microsoft_graphics_single_with_subparams_matches_source_contract() {
    let mut expected = TextAttribute::default();
    expected.set_underline_style(UnderlineStyle::Curly);
    assert_eq!(apply_sequence("\u{1b}[4:3m"), expected);

    expected = TextAttribute::default();
    expected.set_foreground(TextColor::index256(TextColor::DARK_RED));
    assert_eq!(apply_sequence("\u{1b}[38:5:1m"), expected);

    expected = TextAttribute::default();
    expected.set_background(TextColor::index256(TextColor::BRIGHT_WHITE));
    assert_eq!(apply_sequence("\u{1b}[48:5:15m"), expected);

    expected = TextAttribute::default();
    expected.set_underline_color(TextColor::index256(TextColor::DARK_RED));
    assert_eq!(apply_sequence("\u{1b}[58:5:1m"), expected);
}

#[test]
fn extended_sgr_rgb_subparams_are_owned_by_the_same_product_path() {
    let mut expected = TextAttribute::default();
    expected.set_foreground(TextColor::rgb(12, 34, 56));
    expected.set_background(TextColor::rgb(78, 90, 123));
    expected.set_underline_color(TextColor::rgb(210, 111, 12));

    assert_eq!(
        apply_sequence("\u{1b}[38:2:12:34:56;48:2:78:90:123;58:2:210:111:12m"),
        expected
    );
}

#[test]
fn microsoft_graphics_push_pop_basic_and_nested_full_stack_matches_source_contract() {
    let mut state = state();
    state.dispatch(OutputAction::SetGraphicsRendition(Parameters::default()));
    assert_eq!(state.current_attributes(), TextAttribute::default());

    // Microsoft Test 1: push and pop without mutation preserves the current rendition.
    state.dispatch(OutputAction::PushGraphicsRendition(Parameters::default()));
    state.dispatch(OutputAction::PopGraphicsRendition);
    assert_eq!(state.current_attributes(), TextAttribute::default());

    // Microsoft Test 2: a color change between push and pop is discarded.
    state.dispatch(OutputAction::PushGraphicsRendition(Parameters::default()));
    state.dispatch(OutputAction::SetGraphicsRendition(Parameters::from_values(
        vec![Some(36)],
    )));
    let mut cyan = TextAttribute::default();
    cyan.set_foreground(TextColor::index16(TextColor::DARK_CYAN));
    assert_eq!(state.current_attributes(), cyan);
    state.dispatch(OutputAction::PopGraphicsRendition);
    assert_eq!(state.current_attributes(), TextAttribute::default());

    // Microsoft Test 3: nested pushes unwind in LIFO order.
    state.dispatch(OutputAction::PushGraphicsRendition(Parameters::default()));
    state.dispatch(OutputAction::SetGraphicsRendition(Parameters::from_values(
        vec![Some(31)],
    )));
    let mut red = TextAttribute::default();
    red.set_foreground(TextColor::index16(TextColor::DARK_RED));
    assert_eq!(state.current_attributes(), red);

    state.dispatch(OutputAction::PushGraphicsRendition(Parameters::default()));
    state.dispatch(OutputAction::SetGraphicsRendition(Parameters::from_values(
        vec![Some(32)],
    )));
    let mut green = TextAttribute::default();
    green.set_foreground(TextColor::index16(TextColor::DARK_GREEN));
    assert_eq!(state.current_attributes(), green);

    state.dispatch(OutputAction::PopGraphicsRendition);
    assert_eq!(state.current_attributes(), red);
    state.dispatch(OutputAction::PopGraphicsRendition);
    assert_eq!(state.current_attributes(), TextAttribute::default());
}

#[test]
fn microsoft_graphics_push_pop_selective_restore_matches_source_contract() {
    let mut state = state();

    // Microsoft Test 4: save only intensity, background and the double-underline bit.
    state.dispatch(OutputAction::SetGraphicsRendition(Parameters::from_values(
        vec![Some(32)],
    )));
    state.dispatch(OutputAction::SetGraphicsRendition(Parameters::from_values(
        vec![Some(1)],
    )));
    state.dispatch(OutputAction::SetGraphicsRendition(Parameters::from_values(
        vec![Some(44)],
    )));
    state.dispatch(OutputAction::PushGraphicsRendition(
        Parameters::from_values(vec![Some(1), Some(31), Some(21)]),
    ));

    state.dispatch(OutputAction::SetGraphicsRendition(Parameters::from_values(
        vec![Some(42), Some(21)],
    )));
    state.dispatch(OutputAction::SetGraphicsRendition(Parameters::from_values(
        vec![Some(31)],
    )));
    state.dispatch(OutputAction::SetGraphicsRendition(Parameters::from_values(
        vec![Some(22)],
    )));
    state.dispatch(OutputAction::PopGraphicsRendition);

    let mut expected = TextAttribute::default();
    expected.set_foreground(TextColor::index16(TextColor::DARK_RED));
    expected.set_background(TextColor::index16(TextColor::DARK_BLUE));
    expected.set_intense(true);
    assert_eq!(state.current_attributes(), expected);

    // Microsoft Test 5: restoring the single-underline bit clears a newly-set single underline.
    state.dispatch(OutputAction::SetGraphicsRendition(Parameters::from_values(
        vec![Some(24)],
    )));
    state.dispatch(OutputAction::PushGraphicsRendition(
        Parameters::from_values(vec![Some(4)]),
    ));
    state.dispatch(OutputAction::SetGraphicsRendition(Parameters::from_values(
        vec![Some(4)],
    )));
    state.dispatch(OutputAction::PopGraphicsRendition);
    assert_eq!(
        state.current_attributes().underline_style(),
        UnderlineStyle::None
    );

    // Microsoft Test 6: the same restore leaves a double underline intact.
    state.dispatch(OutputAction::SetGraphicsRendition(Parameters::from_values(
        vec![Some(24)],
    )));
    state.dispatch(OutputAction::PushGraphicsRendition(
        Parameters::from_values(vec![Some(4)]),
    ));
    state.dispatch(OutputAction::SetGraphicsRendition(Parameters::from_values(
        vec![Some(21)],
    )));
    state.dispatch(OutputAction::PopGraphicsRendition);
    assert_eq!(
        state.current_attributes().underline_style(),
        UnderlineStyle::Double
    );

    // Microsoft Test 7: saving curly and changing to double restores the single bit,
    // reconstructing the original curly style. The SGR 4:3 parser path is covered
    // independently above, so this witness starts from the equivalent product state.
    let mut curly = state.current_attributes();
    curly.set_underline_style(UnderlineStyle::Curly);
    state.set_current_attributes(curly);
    assert_eq!(
        state.current_attributes().underline_style(),
        UnderlineStyle::Curly
    );
    state.dispatch(OutputAction::PushGraphicsRendition(
        Parameters::from_values(vec![Some(4)]),
    ));
    state.dispatch(OutputAction::SetGraphicsRendition(Parameters::from_values(
        vec![Some(21)],
    )));
    state.dispatch(OutputAction::PopGraphicsRendition);
    assert_eq!(
        state.current_attributes().underline_style(),
        UnderlineStyle::Curly
    );
}
