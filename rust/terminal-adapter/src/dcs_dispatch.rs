//! Composite dispatch integration for the safe Rust Adapter migration.
//!
//! R03a-R03c deliberately kept Sixel, DECDMAC macro storage, and the
//! `AdaptDispatch` geometry core independent. R03d composed DCS handlers here;
//! R03e adds the VT page-management control plane without pulling the concrete
//! C++ `TextBuffer` or renderer into Rust yet.

use crate::adapt_dispatch::{AdaptDispatchCore, MarginRange, PageGeometry};
use crate::macro_buffer::{MacroBuffer, MacroDeleteControl, MacroEncoding};
use crate::page_manager::{PageEvent, PageManager, PageTransition};
use crate::sixel::{Background, Config as SixelConfig, Parser as SixelParser, Size as SixelSize};
use terminal_parser::output_engine::{DcsAction, OutputAction, TermDispatch};
use terminal_parser::state_machine::Parameters;

const ESC: u16 = 0x1b;
const PAGE_CURSOR_COUPLING_MODE: i32 = 64;

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum DcsSessionKind {
    Sixel,
    Macro,
}

/// Composite Rust Adapter dispatch surface.
///
/// Regular terminal actions continue to be handled by [`AdaptDispatchCore`].
/// DCS actions are negotiated here so only supported payloads enter the parser
/// pass-through state. R03e also owns [`PageManager`], preserving the C++ page
/// control semantics while page-buffer and renderer mutations remain explicit
/// typed events for later migration slices.
#[derive(Debug, Clone)]
pub struct AdapterDispatch {
    core: AdaptDispatchCore,
    pages: PageManager,
    saved_page: Option<i32>,
    macro_buffer: MacroBuffer,
    sixel_parser: Option<SixelParser>,
    active_dcs: Option<DcsSessionKind>,
}

impl AdapterDispatch {
    #[must_use]
    pub fn new(geometry: PageGeometry) -> Self {
        let mut core = AdaptDispatchCore::new(geometry);
        // C++ AdaptDispatch initializes DECPCCM (page-cursor coupling) enabled.
        core.set_mode(true, PAGE_CURSOR_COUPLING_MODE, true);
        Self {
            core,
            pages: PageManager::new(geometry),
            saved_page: None,
            macro_buffer: MacroBuffer::default(),
            sixel_parser: None,
            active_dcs: None,
        }
    }

    #[must_use]
    pub const fn core(&self) -> &AdaptDispatchCore {
        &self.core
    }

    pub const fn core_mut(&mut self) -> &mut AdaptDispatchCore {
        &mut self.core
    }

    #[must_use]
    pub const fn page_manager(&self) -> &PageManager {
        &self.pages
    }

    pub fn take_page_events(&mut self) -> Vec<PageEvent> {
        self.pages.take_events()
    }

    #[must_use]
    pub const fn macro_buffer(&self) -> &MacroBuffer {
        &self.macro_buffer
    }

    pub const fn macro_buffer_mut(&mut self) -> &mut MacroBuffer {
        &mut self.macro_buffer
    }

    #[must_use]
    pub const fn sixel_parser(&self) -> Option<&SixelParser> {
        self.sixel_parser.as_ref()
    }

    #[must_use]
    pub const fn active_dcs(&self) -> Option<DcsSessionKind> {
        self.active_dcs
    }

    fn apply_page_transition(&mut self, transition: PageTransition) {
        let delayed_eol_wrap = self.core.delayed_eol_wrap();
        let adjusted = transition.adjust_point(self.core.cursor());
        self.core.set_geometry(self.pages.active_geometry());
        self.core.set_cursor(adjusted);
        self.core.set_delayed_eol_wrap(delayed_eol_wrap);
    }

    fn move_page_to(&mut self, page: i32) {
        let transition = self
            .pages
            .move_to(page, self.core.page_cursor_coupling_mode());
        self.apply_page_transition(transition);
    }

    fn move_page_relative(&mut self, count: i32) {
        let transition = self
            .pages
            .move_relative(count, self.core.page_cursor_coupling_mode());
        self.apply_page_transition(transition);
    }

    fn make_active_page_visible(&mut self) {
        if let Some(transition) = self.pages.make_active_page_visible() {
            self.apply_page_transition(transition);
        }
    }

    fn dispatch_regular(&mut self, action: OutputAction) {
        let sixel_display_change = match &action {
            OutputAction::SetMode {
                private: true,
                enabled,
                mode: 80,
            } => Some(*enabled),
            _ => None,
        };

        self.core.dispatch(action);

        if let Some(enabled) = sixel_display_change
            && let Some(parser) = self.sixel_parser.as_mut()
        {
            parser.set_display_mode(enabled);
        }
    }

    fn begin_sixel(&mut self, parameters: &Parameters) -> bool {
        let Some(canvas) = self.sixel_canvas() else {
            return false;
        };

        let mut config = SixelConfig::new(canvas);
        config.macro_parameter = numeric(parameters, 0);
        config.background = match selective(parameters, 1) {
            1 => Background::Transparent,
            2 => Background::Opaque,
            _ => Background::Default,
        };
        config.background_color = parameters.at(2);

        if let Some(parser) = self.sixel_parser.as_mut() {
            parser.restart_image(config);
            parser.set_display_mode(self.core.sixel_display_mode());
        } else {
            let mut parser = SixelParser::new(config);
            parser.set_display_mode(self.core.sixel_display_mode());
            self.sixel_parser = Some(parser);
        }
        self.active_dcs = Some(DcsSessionKind::Sixel);
        true
    }

    fn begin_macro(&mut self, parameters: &Parameters) -> bool {
        let Ok(macro_id) = usize::try_from(selective(parameters, 0)) else {
            return false;
        };
        let delete_control = match selective(parameters, 1) {
            0 => MacroDeleteControl::DeleteId,
            1 => MacroDeleteControl::DeleteAll,
            _ => return false,
        };
        let encoding = match selective(parameters, 2) {
            0 => MacroEncoding::Text,
            1 => MacroEncoding::HexPair,
            _ => return false,
        };

        if !self
            .macro_buffer
            .init_parser(macro_id, delete_control, encoding)
        {
            return false;
        }

        self.active_dcs = Some(DcsSessionKind::Macro);
        true
    }

    fn sixel_canvas(&self) -> Option<SixelSize> {
        // The default Windows Terminal Sixel conformance level uses a 10x20
        // protocol cell. Ask the parser for that value instead of duplicating
        // it here so this integration stays aligned with the R03a core.
        let probe = SixelParser::new(SixelConfig::new(SixelSize::new(1, 1)));
        let cell = probe.cell_size();
        let geometry = self.core.geometry();

        let (width_cells, height_cells) = if self.core.sixel_display_mode() {
            (geometry.width, geometry.height)
        } else {
            let horizontal = self
                .core
                .margins()
                .horizontal()
                .unwrap_or_else(|| MarginRange::new(0, geometry.right()));
            let vertical = self
                .core
                .margins()
                .vertical()
                .unwrap_or_else(|| MarginRange::new(0, geometry.height - 1));
            let bottom = geometry.top.saturating_add(vertical.end);
            let cursor = self.core.cursor();

            // This is the same origin validity rule used by the C++ Sixel
            // integration when display mode is reset: the cursor must be in
            // the horizontal margin area and not below the bottom margin.
            if cursor.x < horizontal.start || cursor.x > horizontal.end || cursor.y > bottom {
                return None;
            }

            let width = horizontal.end.saturating_sub(cursor.x).saturating_add(1);
            let height = bottom.saturating_sub(cursor.y).saturating_add(1);
            (width, height)
        };

        let width = usize::try_from(width_cells).ok()?.checked_mul(cell.width)?;
        let height = usize::try_from(height_cells)
            .ok()?
            .checked_mul(cell.height)?;
        if width == 0 || height == 0 {
            None
        } else {
            Some(SixelSize::new(width, height))
        }
    }

    fn finish_active_dcs(&mut self) {
        self.active_dcs = None;
    }
}

impl TermDispatch for AdapterDispatch {
    fn dispatch(&mut self, action: OutputAction) {
        match action {
            OutputAction::PagePositionAbsolute(page) => self.move_page_to(page),
            OutputAction::PagePositionRelative(count) => self.move_page_relative(count),
            OutputAction::PagePositionBack(count) => {
                self.move_page_relative(count.saturating_neg());
            }
            OutputAction::NextPage(count) => {
                self.move_page_relative(count);
                self.core.cursor_position(1, 1);
            }
            OutputAction::PrecedingPage(count) => {
                self.move_page_relative(count.saturating_neg());
                self.core.cursor_position(1, 1);
            }
            OutputAction::CursorSaveState => {
                self.saved_page = Some(self.pages.active_page_number());
                self.core.dispatch(OutputAction::CursorSaveState);
            }
            OutputAction::CursorRestoreState => {
                self.move_page_to(self.saved_page.unwrap_or(1));
                self.core.dispatch(OutputAction::CursorRestoreState);
            }
            OutputAction::SetMode {
                private: true,
                enabled,
                mode: PAGE_CURSOR_COUPLING_MODE,
            } => {
                self.core.dispatch(OutputAction::SetMode {
                    private: true,
                    enabled,
                    mode: PAGE_CURSOR_COUPLING_MODE,
                });
                if enabled {
                    self.make_active_page_visible();
                }
            }
            other => self.dispatch_regular(other),
        }
    }

    fn begin_dcs(&mut self, action: DcsAction) -> bool {
        self.finish_active_dcs();
        match action {
            DcsAction::DefineSixelImage(parameters) => self.begin_sixel(&parameters),
            DcsAction::DefineMacro(parameters) => self.begin_macro(&parameters),
            other => {
                self.core.dispatch(OutputAction::DcsBegin(other));
                false
            }
        }
    }

    fn dcs_put(&mut self, code_unit: u16) -> bool {
        match self.active_dcs {
            Some(DcsSessionKind::Sixel) => {
                let Some(parser) = self.sixel_parser.as_mut() else {
                    self.finish_active_dcs();
                    return false;
                };
                parser.put(code_unit);
                if code_unit == ESC {
                    self.finish_active_dcs();
                }
                true
            }
            Some(DcsSessionKind::Macro) => {
                let keep_parsing = self.macro_buffer.parse_definition(code_unit);
                if code_unit == ESC || !keep_parsing {
                    self.finish_active_dcs();
                }
                keep_parsing
            }
            None => false,
        }
    }
}

fn numeric(parameters: &Parameters, index: usize) -> i32 {
    match parameters.at(index) {
        Some(value) if value > 0 => value,
        _ => 1,
    }
}

fn selective(parameters: &Parameters, index: usize) -> i32 {
    parameters.at(index).unwrap_or(0)
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::adapt_dispatch::Point as TextPoint;
    use crate::page_manager::{PageBufferRef, PageEvent};
    use terminal_parser::output_engine::OutputStateMachineEngine;
    use terminal_parser::state_machine::{State, StateMachine};

    fn geometry() -> PageGeometry {
        PageGeometry::new(0, 80, 24)
    }

    fn scrolled_geometry() -> PageGeometry {
        PageGeometry::new(20, 80, 24)
    }

    fn machine() -> StateMachine<OutputStateMachineEngine<AdapterDispatch>> {
        StateMachine::new(OutputStateMachineEngine::new(AdapterDispatch::new(
            geometry(),
        )))
    }

    fn dispatch(
        machine: &StateMachine<OutputStateMachineEngine<AdapterDispatch>>,
    ) -> &AdapterDispatch {
        machine.engine().dispatch()
    }

    #[test]
    fn sixel_dcs_flows_end_to_end_into_the_safe_parser() {
        let mut machine = machine();
        machine.process_str("\u{1b}P1;1q@\u{1b}\\");

        assert_eq!(machine.state(), State::Ground);
        assert_eq!(dispatch(&machine).active_dcs(), None);
        let parser = dispatch(&machine)
            .sixel_parser()
            .expect("Sixel parser was negotiated");
        assert_eq!(parser.image_width(), 1);
        assert!(!parser.pixel(0, 0).expect("first pixel exists").transparent);
        assert!(
            !parser
                .pixel(0, 1)
                .expect("aspect-ratio pixel exists")
                .transparent
        );
        assert!(parser.pixel(0, 2).expect("unset pixel exists").transparent);
    }

    #[test]
    fn sixel_parameters_and_display_mode_reach_the_protocol_core() {
        let mut machine = machine();
        machine.process_str("\u{1b}[?80h\u{1b}P7;1q?\u{1b}\\");

        let parser = dispatch(&machine)
            .sixel_parser()
            .expect("Sixel parser was negotiated");
        assert!(parser.display_mode());
        assert_eq!(parser.pixel_aspect_ratio(), 1);
        assert_eq!(parser.image_height(), 6);
    }

    #[test]
    fn sixel_palette_changes_persist_across_image_dcs_sessions() {
        let mut machine = machine();
        machine.process_str("\u{1b}P1;1q#1;2;100;0;0@\u{1b}\\");
        machine.process_str("\u{1b}P1;1q#1@\u{1b}\\");

        let parser = dispatch(&machine)
            .sixel_parser()
            .expect("Sixel parser was reused");
        assert_eq!(
            parser.palette_color(1),
            Some(crate::sixel::Rgb::new(255, 0, 0))
        );
        assert_eq!(parser.pixel(0, 0).expect("pixel exists").color_index, 1);
    }

    #[test]
    fn text_encoded_macro_dcs_persists_in_macro_buffer() {
        let mut machine = machine();
        machine.process_str("\u{1b}P3;0;0!zhello\u{1b}\\");

        assert_eq!(machine.state(), State::Ground);
        assert_eq!(dispatch(&machine).active_dcs(), None);
        let expected = "hello".encode_utf16().collect::<Vec<_>>();
        assert_eq!(
            dispatch(&machine).macro_buffer().macro_contents(3),
            Some(expected.as_slice())
        );
    }

    #[test]
    fn hex_macro_dcs_uses_cpp_selective_parameter_defaults() {
        let mut machine = machine();
        machine.process_str("\u{1b}P4;;1!z4869\u{1b}\\");

        let expected = "Hi".encode_utf16().collect::<Vec<_>>();
        assert_eq!(
            dispatch(&machine).macro_buffer().macro_contents(4),
            Some(expected.as_slice())
        );
    }

    #[test]
    fn invalid_macro_parameters_reject_the_payload_without_mutation() {
        let mut machine = machine();
        machine.process_str("\u{1b}P2;0;0!zkept\u{1b}\\");
        let expected = "kept".encode_utf16().collect::<Vec<_>>();
        assert_eq!(
            dispatch(&machine).macro_buffer().macro_contents(2),
            Some(expected.as_slice())
        );

        machine.process_str("\u{1b}P2;9;0!zdropped\u{1b}\\");
        assert_eq!(
            dispatch(&machine).macro_buffer().macro_contents(2),
            Some(expected.as_slice())
        );
    }

    #[test]
    fn unsupported_dcs_is_deferred_and_its_payload_is_not_misrouted() {
        let mut machine = machine();
        machine.process_str("\u{1b}P1$pignored\u{1b}\\");

        assert_eq!(dispatch(&machine).active_dcs(), None);
        let deferred = dispatch(&machine).core().deferred_actions();
        assert_eq!(deferred.len(), 1);
        assert!(matches!(
            deferred[0],
            OutputAction::DcsBegin(DcsAction::RestoreTerminalState(_))
        ));
    }

    #[test]
    fn regular_output_actions_still_flow_into_adapt_dispatch_core() {
        let mut machine = machine();
        machine.process_str("\u{1b}[10;20H\u{1b}P1;1q@\u{1b}\\");

        assert_eq!(
            dispatch(&machine).core().cursor(),
            TextPoint { x: 19, y: 9 }
        );
        assert!(dispatch(&machine).sixel_parser().is_some());
    }

    #[test]
    fn composite_adapter_defaults_to_page_cursor_coupling() {
        let mut adapter = AdapterDispatch::new(scrolled_geometry());
        assert!(adapter.core().page_cursor_coupling_mode());
        assert_eq!(adapter.page_manager().active_page_number(), 1);
        assert_eq!(adapter.page_manager().visible_page_number(), 1);

        adapter.dispatch(OutputAction::PagePositionAbsolute(3));
        assert_eq!(adapter.page_manager().active_page_number(), 3);
        assert_eq!(adapter.page_manager().visible_page_number(), 3);
        assert_eq!(adapter.core().geometry(), scrolled_geometry());
    }

    #[test]
    fn uncoupled_page_move_preserves_cursor_coordinates_relative_to_page() {
        let mut adapter = AdapterDispatch::new(scrolled_geometry());
        adapter.core_mut().set_cursor(TextPoint { x: 30, y: 35 });
        adapter.core_mut().set_delayed_eol_wrap(true);
        adapter.dispatch(OutputAction::SetMode {
            private: true,
            enabled: false,
            mode: PAGE_CURSOR_COUPLING_MODE,
        });
        adapter.dispatch(OutputAction::PagePositionAbsolute(4));

        assert_eq!(adapter.page_manager().active_page_number(), 4);
        assert_eq!(adapter.page_manager().visible_page_number(), 1);
        assert_eq!(adapter.core().geometry(), PageGeometry::new(0, 80, 24));
        assert_eq!(adapter.core().cursor(), TextPoint { x: 30, y: 15 });
        assert!(adapter.core().delayed_eol_wrap());
        assert!(
            adapter
                .page_manager()
                .pending_events()
                .contains(&PageEvent::CopyProperties {
                    from: PageBufferRef::Visible,
                    to: PageBufferRef::Background(4),
                    old_top: 20,
                    new_top: 0,
                })
        );
        assert!(
            adapter
                .page_manager()
                .pending_events()
                .contains(&PageEvent::SetVisibleCursorVisible(false))
        );
    }

    #[test]
    fn enabling_page_cursor_coupling_makes_active_page_visible() {
        let mut adapter = AdapterDispatch::new(scrolled_geometry());
        adapter.core_mut().set_cursor(TextPoint { x: 30, y: 35 });
        adapter.dispatch(OutputAction::SetMode {
            private: true,
            enabled: false,
            mode: PAGE_CURSOR_COUPLING_MODE,
        });
        adapter.dispatch(OutputAction::PagePositionAbsolute(4));
        adapter.take_page_events();

        adapter.dispatch(OutputAction::SetMode {
            private: true,
            enabled: true,
            mode: PAGE_CURSOR_COUPLING_MODE,
        });

        assert_eq!(adapter.page_manager().active_page_number(), 4);
        assert_eq!(adapter.page_manager().visible_page_number(), 4);
        assert_eq!(adapter.core().geometry(), scrolled_geometry());
        assert_eq!(adapter.core().cursor(), TextPoint { x: 30, y: 35 });
        assert!(
            adapter
                .page_manager()
                .pending_events()
                .contains(&PageEvent::RedrawAll)
        );
    }

    #[test]
    fn next_and_preceding_page_home_the_cursor() {
        let mut adapter = AdapterDispatch::new(scrolled_geometry());
        adapter.core_mut().set_cursor(TextPoint { x: 33, y: 35 });

        adapter.dispatch(OutputAction::NextPage(2));
        assert_eq!(adapter.page_manager().active_page_number(), 3);
        assert_eq!(adapter.page_manager().visible_page_number(), 3);
        assert_eq!(adapter.core().cursor(), TextPoint { x: 0, y: 20 });

        adapter.core_mut().set_cursor(TextPoint { x: 22, y: 30 });
        adapter.dispatch(OutputAction::PrecedingPage(1));
        assert_eq!(adapter.page_manager().active_page_number(), 2);
        assert_eq!(adapter.page_manager().visible_page_number(), 2);
        assert_eq!(adapter.core().cursor(), TextPoint { x: 0, y: 20 });
    }

    #[test]
    fn cursor_save_restore_includes_the_page_number() {
        let mut adapter = AdapterDispatch::new(scrolled_geometry());
        adapter.dispatch(OutputAction::PagePositionAbsolute(3));
        adapter.core_mut().set_cursor(TextPoint { x: 12, y: 27 });
        adapter.core_mut().set_delayed_eol_wrap(true);
        adapter.dispatch(OutputAction::CursorSaveState);

        adapter.dispatch(OutputAction::PagePositionAbsolute(5));
        adapter.core_mut().set_cursor(TextPoint { x: 2, y: 22 });
        adapter.core_mut().set_delayed_eol_wrap(false);
        adapter.dispatch(OutputAction::CursorRestoreState);

        assert_eq!(adapter.page_manager().active_page_number(), 3);
        assert_eq!(adapter.page_manager().visible_page_number(), 3);
        assert_eq!(adapter.core().cursor(), TextPoint { x: 12, y: 27 });
        assert!(adapter.core().delayed_eol_wrap());
    }

    #[test]
    fn parser_routes_ppa_ppr_and_ppb_through_page_manager() {
        let mut machine = StateMachine::new(OutputStateMachineEngine::new(AdapterDispatch::new(
            scrolled_geometry(),
        )));
        machine.process_str("\u{1b}[?64l\u{1b}[3 P\u{1b}[2 Q\u{1b}[ R");

        assert_eq!(dispatch(&machine).page_manager().active_page_number(), 4);
        assert_eq!(dispatch(&machine).page_manager().visible_page_number(), 1);
        assert_eq!(
            dispatch(&machine).core().geometry(),
            PageGeometry::new(0, 80, 24)
        );
    }
}
