use terminal_adapter::{
    adapt_dispatch::PageGeometry, presentation_state::AdaptDispatchPresentationState,
};
use terminal_buffer::text_color::TextColor;
use terminal_parser::{
    output_engine::{OutputAction, TermDispatch},
    state_machine::Parameters,
};

fn state() -> AdaptDispatchPresentationState {
    AdaptDispatchPresentationState::new(PageGeometry::new(20, 100, 29))
}

fn sgr(state: &mut AdaptDispatchPresentationState, option: i32) {
    state.dispatch(OutputAction::SetGraphicsRendition(Parameters::from_values(
        vec![Some(option)],
    )));
}

#[test]
fn microsoft_graphics_persist_brightness_matches_source_contract() {
    let mut state = state();

    // Microsoft Test 1: SGR 1 is rendition state, so changing a normal 30-series
    // foreground afterwards must not clear the intensity bit.
    sgr(&mut state, 0);
    assert!(!state.current_attributes().is_intense());

    sgr(&mut state, 34); // ForegroundBlue
    assert_eq!(
        state.current_attributes().foreground(),
        TextColor::index16(TextColor::DARK_BLUE)
    );
    assert!(!state.current_attributes().is_intense());

    sgr(&mut state, 1); // Intense
    assert!(state.current_attributes().is_intense());

    sgr(&mut state, 32); // ForegroundGreen
    assert_eq!(
        state.current_attributes().foreground(),
        TextColor::index16(TextColor::DARK_GREEN)
    );
    assert!(state.current_attributes().is_intense());

    // Microsoft Test 2: choosing a 90-series bright color does not itself turn
    // persistent intensity on, and a later normal color remains non-intense.
    sgr(&mut state, 0);
    assert!(!state.current_attributes().is_intense());

    sgr(&mut state, 94); // BrightForegroundBlue
    assert_eq!(
        state.current_attributes().foreground(),
        TextColor::index16(TextColor::BRIGHT_BLUE)
    );
    assert!(!state.current_attributes().is_intense());

    sgr(&mut state, 34); // ForegroundBlue
    assert_eq!(
        state.current_attributes().foreground(),
        TextColor::index16(TextColor::DARK_BLUE)
    );
    assert!(!state.current_attributes().is_intense());

    // Microsoft Test 3: once SGR 1 is active, a 90-series color also must not
    // clear it. Subsequent normal colors continue to observe the intense state.
    sgr(&mut state, 0);
    sgr(&mut state, 34);
    assert!(!state.current_attributes().is_intense());

    sgr(&mut state, 1);
    assert!(state.current_attributes().is_intense());

    sgr(&mut state, 94);
    assert_eq!(
        state.current_attributes().foreground(),
        TextColor::index16(TextColor::BRIGHT_BLUE)
    );
    assert!(state.current_attributes().is_intense());

    sgr(&mut state, 34);
    assert_eq!(
        state.current_attributes().foreground(),
        TextColor::index16(TextColor::DARK_BLUE)
    );
    assert!(state.current_attributes().is_intense());

    sgr(&mut state, 32);
    assert_eq!(
        state.current_attributes().foreground(),
        TextColor::index16(TextColor::DARK_GREEN)
    );
    assert!(state.current_attributes().is_intense());
}
